// mylib.binary_convert (maboroutu.binary_convert) のテスト。
// library_spec.md 4.8節・6章。
//
// 6章の検証方針は「(1) 各種数値型・Endian指定の往復変換（write→readで元の値と
// 一致すること）」を要求している。しかし null_data_source /
// null_writable_data_source は書き込みを実際には保持せず、読み込みは要求
// サイズ分のゼロ埋めバッファを返すだけのスタブ（posix /dev/null の模倣）で
// あるため、これらだけでは往復の忠実性を検証できない。
//
// mylib.memory_data_source（library_spec.md「on the horizon」に記載の通り、
// v1.33時点で未実装）が無いため、本テストでは検証専用の最小限の
// in-memory data_source / sequential_source モックをテスト側に用意した。
// これはライブラリの一部ではなく、あくまでこのテストプロジェクト内の
// テストダブルである。
#include <array>
#include <bit>
#include <cstddef>
#include <expected>
#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.data_source;
import maboroutu.sequential_source;
import maboroutu.binary_convert;

namespace {

// ---------------------------------------------------------------------
// テスト専用: in-memory data_source / sequential_source モック
// ---------------------------------------------------------------------
using ds_code_type =
    decltype(std::declval<maboroutu::data_source_result<int>>().error().code());
using seq_code_type =
    decltype(std::declval<maboroutu::sequential_source_result<int>>()
                 .error()
                 .code());

class memory_data_source {
   std::vector<std::byte> _data;

 public:
   template <class T> using result_type = maboroutu::data_source_result<T>;

   explicit memory_data_source(std::size_t initial_size = 0)
       : _data(initial_size) {}

   [[nodiscard]] auto size() const -> result_type<std::size_t> {
      return _data.size();
   }
   [[nodiscard]] auto read(maboroutu::region r) -> result_type<maboroutu::byte_array> {
      if (r.offset + r.size > _data.size()) {
         return std::unexpected(
             maboroutu::error<ds_code_type>(ds_code_type::out_of_range));
      }
      maboroutu::byte_array out{
          .value = std::make_unique<maboroutu::byte_array::value_type>(r.size),
          .size = r.size,
      };
      std::memcpy(out.value.get(), _data.data() + r.offset, r.size);
      return out;
   }
   auto write(maboroutu::region r, std::span<std::byte const> data)
       -> result_type<void> {
      if (r.offset + r.size > _data.size()) {
         _data.resize(r.offset + r.size);
      }
      std::memcpy(_data.data() + r.offset, data.data(), r.size);
      return {};
   }
};
static_assert(maboroutu::writable_data_source<memory_data_source>);

class memory_sequential_source {
   std::vector<std::byte> _data;
   std::size_t _pos = 0;

 public:
   template <class T> using result_type = maboroutu::sequential_source_result<T>;

   explicit memory_sequential_source(std::size_t initial_size = 0)
       : _data(initial_size) {}
   // 既存のバイト列に対し、読み出しカーソルを0から始める別インスタンスを
   // 作るためのコンストラクタ（count()は一方向前進のみのため、
   // 「書いたものを読み直す」検証には新しいインスタンスが必要になる）。
   explicit memory_sequential_source(std::vector<std::byte> data)
       : _data(std::move(data)) {}

   [[nodiscard]] auto bytes() const -> std::vector<std::byte> const & {
      return _data;
   }
   [[nodiscard]] auto count() const -> std::size_t { return _pos; }
   auto read(std::size_t n) -> result_type<maboroutu::byte_array> {
      if (_pos + n > _data.size()) {
         return std::unexpected(
             maboroutu::error<seq_code_type>(seq_code_type::out_of_range));
      }
      maboroutu::byte_array out{
          .value = std::make_unique<maboroutu::byte_array::value_type>(n),
          .size = n,
      };
      std::memcpy(out.value.get(), _data.data() + _pos, n);
      _pos += n;
      return out;
   }
   auto write(std::span<std::byte const> data) -> result_type<void> {
      if (_pos + data.size() > _data.size()) {
         _data.resize(_pos + data.size());
      }
      std::memcpy(_data.data() + _pos, data.data(), data.size());
      _pos += data.size();
      return {};
   }
};
static_assert(maboroutu::writable_sequential_source<memory_sequential_source>);

// ---------------------------------------------------------------------
// from_bytes / to_bytes: 純粋な変換関数
// ---------------------------------------------------------------------
TEST(MaboroutuBinaryConvert, ToBytesFromBytesRoundTripNativeEndian) {
   constexpr std::int32_t original = -12345;
   auto bytes =
       maboroutu::to_bytes<maboroutu::endian::native, std::int32_t>(original);
   auto restored =
       maboroutu::from_bytes<maboroutu::endian::native, std::int32_t>(bytes);
   EXPECT_EQ(restored, original);
}

TEST(MaboroutuBinaryConvert, ToBytesProducesByteswappedResultForOppositeEndian) {
   constexpr std::uint32_t value = 0x01020304u;
   constexpr auto opposite = (std::endian::native == std::endian::little)
                                 ? std::endian::big
                                 : std::endian::little;
   auto native_bytes =
       maboroutu::to_bytes<std::endian::native, std::uint32_t>(value);
   auto opposite_bytes = maboroutu::to_bytes<opposite, std::uint32_t>(value);
   // native/oppositeでバイト列が反転していること。
   for (std::size_t i = 0; i < sizeof(value); ++i) {
      EXPECT_EQ(native_bytes[i], opposite_bytes[sizeof(value) - 1 - i]);
   }
   // opposite側でエンコードしたバイト列を native として解釈すると、
   // バイトスワップされた値になる。
   auto reinterpreted =
       maboroutu::from_bytes<std::endian::native, std::uint32_t>(
           opposite_bytes);
   EXPECT_EQ(reinterpreted, std::byteswap(value));
}

// ---------------------------------------------------------------------
// read_value / write_value: data_source経由
// ---------------------------------------------------------------------
class BinaryConvertValueRoundTrip
    : public ::testing::TestWithParam<maboroutu::endian> {};

TEST_P(BinaryConvertValueRoundTrip, DataSourceWriteThenReadMatchesOriginal) {
   memory_data_source src(sizeof(std::int32_t));
   constexpr std::int32_t original = 0x1234'5678;
   auto endian = GetParam();
   auto write_result = (endian == maboroutu::endian::little)
       ? maboroutu::write_value<maboroutu::endian::little>(src, 0, original)
       : maboroutu::write_value<maboroutu::endian::big>(src, 0, original);
   ASSERT_TRUE(write_result.has_value());

   auto read_result = (endian == maboroutu::endian::little)
       ? maboroutu::read_value<maboroutu::endian::little, std::int32_t>(src, 0)
       : maboroutu::read_value<maboroutu::endian::big, std::int32_t>(src, 0);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

INSTANTIATE_TEST_SUITE_P(LittleAndBig, BinaryConvertValueRoundTrip,
                        ::testing::Values(maboroutu::endian::little,
                                         maboroutu::endian::big));

TEST(MaboroutuBinaryConvert, ReadValueBeyondEndOfSourcePropagatesError) {
   memory_data_source src(2); // int32_t (4byte) には満たない
   auto result =
       maboroutu::read_value<maboroutu::endian::little, std::int32_t>(src, 0);
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
}

// ---------------------------------------------------------------------
// read_array / write_array: data_source経由
// ---------------------------------------------------------------------
TEST(MaboroutuBinaryConvert, DataSourceArrayRoundTrip) {
   memory_data_source src(sizeof(std::uint16_t) * 3);
   std::array<std::uint16_t, 3> original{1, 2, 3};
   auto write_result =
       maboroutu::write_array<maboroutu::endian::little, std::uint16_t, 3>(
           src, 0, original);
   ASSERT_TRUE(write_result.has_value());

   auto read_result =
       maboroutu::read_array<maboroutu::endian::little, std::uint16_t, 3>(src,
                                                                          0);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

// ---------------------------------------------------------------------
// read_vector / write_vector: data_source経由
// ---------------------------------------------------------------------
TEST(MaboroutuBinaryConvert, DataSourceVectorRoundTrip) {
   memory_data_source src(sizeof(std::int64_t) * 4);
   std::vector<std::int64_t> original{-1, 2, -3, 4};
   auto write_result =
       maboroutu::write_vector<maboroutu::endian::big>(src, 0, original);
   ASSERT_TRUE(write_result.has_value());

   auto read_result =
       maboroutu::read_vector<maboroutu::endian::big, std::int64_t>(src, 0, 4);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

// ---------------------------------------------------------------------
// sequential_source経由 (v1.33追加のoffset省略オーバーロード)
// ---------------------------------------------------------------------
TEST(MaboroutuBinaryConvert, SequentialSourceValueRoundTrip) {
   memory_sequential_source seq(sizeof(std::int32_t));
   constexpr std::int32_t original = 42;
   auto write_result =
       maboroutu::write_value<maboroutu::endian::little>(seq, original);
   ASSERT_TRUE(write_result.has_value());
   EXPECT_EQ(seq.count(), sizeof(std::int32_t));

   // count()は一方向前進のみのため、読み直しには同じバイト列を持つ
   // 新しいインスタンスを使う（他のテストと同じパターン）。
   memory_sequential_source reader(seq.bytes());
   auto read_result =
       maboroutu::read_value<maboroutu::endian::little, std::int32_t>(reader);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

TEST(MaboroutuBinaryConvert, SequentialSourceSequentialReadsAdvanceCount) {
   // binary_convert のoffset省略オーバーロードは sequential_source::count()
   // に沿って前進するため、既知レイアウトを順番に読み書きできる
   // (4.9節「呼び出し例」の記述と対応)。
   memory_sequential_source seq(sizeof(std::uint32_t) + sizeof(std::uint16_t) * 3);
   ASSERT_TRUE(maboroutu::write_value<maboroutu::endian::little>(
                   seq, std::uint32_t{100})
                   .has_value());
   std::array<std::uint16_t, 3> tail{10, 20, 30};
   ASSERT_TRUE((maboroutu::write_array<maboroutu::endian::little, std::uint16_t,
                                       3>(seq, tail)
                    .has_value()));
   EXPECT_EQ(seq.count(), sizeof(std::uint32_t) + sizeof(std::uint16_t) * 3);

   // count()は一方向前進のみ(4.9節参照)のため、書いたものを読み直すには
   // 同じバイト列を持つ新しいインスタンス（カーソルは0から）を使う。
   memory_sequential_source reader(seq.bytes());
   auto head =
       maboroutu::read_value<maboroutu::endian::little, std::uint32_t>(reader);
   ASSERT_TRUE(head.has_value());
   EXPECT_EQ(*head, 100u);
   auto rest =
       maboroutu::read_array<maboroutu::endian::little, std::uint16_t, 3>(
           reader);
   ASSERT_TRUE(rest.has_value());
   EXPECT_EQ(*rest, tail);
}

TEST(MaboroutuBinaryConvert, SequentialAndOffsetVersionsAgreeOnTheSameBytes) {
   // 6章検証方針(3): binary_convertのoffset版を直接呼び出した場合と、
   // sequential_source経由で同一順序に呼び出した場合とで、
   // 得られる値が一致することを確認する。
   constexpr std::size_t total = sizeof(std::uint32_t) + sizeof(std::uint16_t);

   memory_data_source ds(total);
   ASSERT_TRUE(maboroutu::write_value<maboroutu::endian::little>(
                   ds, 0, std::uint32_t{7})
                   .has_value());
   ASSERT_TRUE(maboroutu::write_value<maboroutu::endian::little>(
                   ds, sizeof(std::uint32_t), std::uint16_t{9})
                   .has_value());
   auto offset_a =
       maboroutu::read_value<maboroutu::endian::little, std::uint32_t>(ds, 0);
   auto offset_b = maboroutu::read_value<maboroutu::endian::little, std::uint16_t>(
       ds, sizeof(std::uint32_t));
   ASSERT_TRUE(offset_a.has_value());
   ASSERT_TRUE(offset_b.has_value());

   memory_sequential_source seq(total);
   ASSERT_TRUE(maboroutu::write_value<maboroutu::endian::little>(
                   seq, std::uint32_t{7})
                   .has_value());
   ASSERT_TRUE(maboroutu::write_value<maboroutu::endian::little>(
                   seq, std::uint16_t{9})
                   .has_value());

   memory_sequential_source seq_reader(seq.bytes());
   auto seq_a =
       maboroutu::read_value<maboroutu::endian::little, std::uint32_t>(
           seq_reader);
   auto seq_b =
       maboroutu::read_value<maboroutu::endian::little, std::uint16_t>(
           seq_reader);
   ASSERT_TRUE(seq_a.has_value());
   ASSERT_TRUE(seq_b.has_value());

   EXPECT_EQ(*offset_a, *seq_a);
   EXPECT_EQ(*offset_b, *seq_b);
}

} // namespace
