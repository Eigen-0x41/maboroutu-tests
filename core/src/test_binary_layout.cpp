// mylib.binary_layout (maboroutu.binary_layout) のテスト。
//
// NOTE（一次情報）: 本モジュールはlibrary_spec.mdにまだコンポーネント節が
// 無い新規モジュール。テストは映幻から提供された引き継ぎ資料
// 「maboroutu — GTest実装のための引き継ぎ資料」(2章・3.3節・付録B)を
// 一次情報として作成した。
//
// !!! 最重要 (引き継ぎ資料が明記する既知の回帰リスク) !!!
// read_uint/write_uintは当初「バイト配置判定式がホストのネイティブ
// エンディアンと比較する」という誤りを含んでいたが修正済み
// (`(Endian == endian::native) ? i : (Bytes-1-i)` → 正しくは
// `(Endian == endian::little) ? i : (Bytes-1-i)`)。
//
// 引き継ぎ資料は「read/writeの往復テストだけでは検出できないため、
// read_uint/write_uintを経由しない生バイト列の直接検証が必須」と
// 述べているが、本ファイルの実装後に実機検証したところ、
// **x86_64等のlittle-endianホスト上では、生バイト列を直接検証する
// 本ファイルの WriteUint*ProducesExpectedRawBytes 系のテストも
// この特定の回帰を検出できない**ことを確認した（実際に該当バグを
// 一時的に再現させ、全14件のテストが通過してしまうことを確認済み）。
// 理由: little-endianホストでは `endian::native == endian::little`
// が常に真であるため、誤った式 `Endian == endian::native` は、
// 正しい式 `Endian == endian::little` と**あらゆるEndian値に対して
// 完全に同一の結果**を返してしまい、往復テストか生バイト列検証かに
// 関わらず、ランタイムテストでは原理的に区別できない。
// この回帰を実際に検出できるのは big-endian ホスト（SPARC/PowerPC等）
// 上で実行した場合のみであり、GitHub Actions等の一般的なCI環境
// （x86_64 / ARM64、いずれもlittle-endian）では実行時テストによる
// 再発防止は成立しない。静的解析・コードレビューでの検出に頼るしかない
// 点を、映幻に申し送りとして明記しておく。
// 以下のRawBytes系テストはそれでも「現在の実装が正しいこと」自体の
// 検証としては引き続き有効であるため残しているが、上記の限界を
// 理解した上で読むこと。
//
// offset_patch のデストラクタ未解決検出は assert() ベースであるため、
// ASSERT_DEATH系のテストはNDEBUGが定義されていない(assertが有効な)
// ビルド構成が前提となる。
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.data_source;
import maboroutu.data_buffer;
import maboroutu.binary_convert;
import maboroutu.binary_layout;

namespace {

using ds_code_type =
    decltype(std::declval<maboroutu::data_source_result<int>>().error().code());
using buffer_code_type =
    decltype(std::declval<maboroutu::data_buffer_result<int>>().error().code());

// data_buffer concept を満たす、このファイル専用の in-memory モック
// (append/grow/view を持つ点で他ファイルの memory_data_source と異なる)。
class memory_buffer {
   std::vector<std::byte> _data;

 public:
   template <class T> using result_type = maboroutu::data_source_result<T>;

   [[nodiscard]] auto bytes() const -> std::vector<std::byte> const & {
      return _data;
   }
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
   auto append(std::span<std::byte const> data)
       -> maboroutu::data_buffer_result<maboroutu::region> {
      auto const offset = _data.size();
      _data.insert(_data.end(), data.begin(), data.end());
      return maboroutu::region{.offset = offset, .size = data.size()};
   }
   auto grow(std::size_t n) -> maboroutu::data_buffer_result<maboroutu::region> {
      auto const offset = _data.size();
      _data.resize(_data.size() + n);
      return maboroutu::region{.offset = offset, .size = n};
   }
   auto view(maboroutu::region r)
       -> maboroutu::data_buffer_result<std::span<std::byte>> {
      if (r.offset + r.size > _data.size()) {
         return std::unexpected(
             maboroutu::error<buffer_code_type>(buffer_code_type::out_of_range));
      }
      return std::span<std::byte>{_data.data() + r.offset, r.size};
   }
};
static_assert(maboroutu::data_buffer<memory_buffer>);

// =======================================================================
// read_uint / write_uint
// =======================================================================

// --- 生バイト列の直接検証（★最重要。read_uint非経由） -----------------
TEST(MaboroutuBinaryLayout, WriteUintBigEndianProducesExpectedRawBytes) {
   memory_buffer buf;
   auto grown = buf.grow(4);
   ASSERT_TRUE(grown.has_value());
   auto write_result = maboroutu::write_uint<4, maboroutu::endian::big>(
       buf, grown->offset, std::uint32_t{0x11223344});
   ASSERT_TRUE(write_result.has_value());

   auto const &raw = buf.bytes();
   ASSERT_EQ(raw.size(), 4u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[0]), 0x11u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[1]), 0x22u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[2]), 0x33u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[3]), 0x44u);
}

TEST(MaboroutuBinaryLayout, WriteUintLittleEndianProducesExpectedRawBytes) {
   memory_buffer buf;
   auto grown = buf.grow(4);
   ASSERT_TRUE(grown.has_value());
   auto write_result = maboroutu::write_uint<4, maboroutu::endian::little>(
       buf, grown->offset, std::uint32_t{0x11223344});
   ASSERT_TRUE(write_result.has_value());

   auto const &raw = buf.bytes();
   ASSERT_EQ(raw.size(), 4u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[0]), 0x44u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[1]), 0x33u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[2]), 0x22u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[3]), 0x11u);
}

// --- 標準幅(1/2/4/8バイト)×little/bigの往復 -----------------------------
class BinaryLayoutUintRoundTrip
    : public ::testing::TestWithParam<maboroutu::endian> {};

TEST_P(BinaryLayoutUintRoundTrip, Width1) {
   memory_buffer buf;
   auto grown = buf.grow(1);
   ASSERT_TRUE(grown.has_value());
   auto const endian = GetParam();
   constexpr std::uint8_t original = 0xAB;
   auto write_result = (endian == maboroutu::endian::little)
       ? maboroutu::write_uint<1, maboroutu::endian::little>(buf, grown->offset, original)
       : maboroutu::write_uint<1, maboroutu::endian::big>(buf, grown->offset, original);
   ASSERT_TRUE(write_result.has_value());
   auto read_result = (endian == maboroutu::endian::little)
       ? maboroutu::read_uint<1, maboroutu::endian::little>(buf, grown->offset)
       : maboroutu::read_uint<1, maboroutu::endian::big>(buf, grown->offset);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

TEST_P(BinaryLayoutUintRoundTrip, Width2) {
   memory_buffer buf;
   auto grown = buf.grow(2);
   ASSERT_TRUE(grown.has_value());
   auto const endian = GetParam();
   constexpr std::uint16_t original = 0xABCD;
   auto write_result = (endian == maboroutu::endian::little)
       ? maboroutu::write_uint<2, maboroutu::endian::little>(buf, grown->offset, original)
       : maboroutu::write_uint<2, maboroutu::endian::big>(buf, grown->offset, original);
   ASSERT_TRUE(write_result.has_value());
   auto read_result = (endian == maboroutu::endian::little)
       ? maboroutu::read_uint<2, maboroutu::endian::little>(buf, grown->offset)
       : maboroutu::read_uint<2, maboroutu::endian::big>(buf, grown->offset);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

TEST_P(BinaryLayoutUintRoundTrip, Width4) {
   memory_buffer buf;
   auto grown = buf.grow(4);
   ASSERT_TRUE(grown.has_value());
   auto const endian = GetParam();
   constexpr std::uint32_t original = 0x89ABCDEF;
   auto write_result = (endian == maboroutu::endian::little)
       ? maboroutu::write_uint<4, maboroutu::endian::little>(buf, grown->offset, original)
       : maboroutu::write_uint<4, maboroutu::endian::big>(buf, grown->offset, original);
   ASSERT_TRUE(write_result.has_value());
   auto read_result = (endian == maboroutu::endian::little)
       ? maboroutu::read_uint<4, maboroutu::endian::little>(buf, grown->offset)
       : maboroutu::read_uint<4, maboroutu::endian::big>(buf, grown->offset);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

TEST_P(BinaryLayoutUintRoundTrip, Width8) {
   memory_buffer buf;
   auto grown = buf.grow(8);
   ASSERT_TRUE(grown.has_value());
   auto const endian = GetParam();
   constexpr std::uint64_t original = 0x0123456789ABCDEFull;
   auto write_result = (endian == maboroutu::endian::little)
       ? maboroutu::write_uint<8, maboroutu::endian::little>(buf, grown->offset, original)
       : maboroutu::write_uint<8, maboroutu::endian::big>(buf, grown->offset, original);
   ASSERT_TRUE(write_result.has_value());
   auto read_result = (endian == maboroutu::endian::little)
       ? maboroutu::read_uint<8, maboroutu::endian::little>(buf, grown->offset)
       : maboroutu::read_uint<8, maboroutu::endian::big>(buf, grown->offset);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

INSTANTIATE_TEST_SUITE_P(LittleAndBig, BinaryLayoutUintRoundTrip,
                        ::testing::Values(maboroutu::endian::little,
                                         maboroutu::endian::big));

// --- 非標準幅（3バイト、FLACの24bit長フィールド相当） -------------------
TEST(MaboroutuBinaryLayout, Width3NonStandardRawBytesBigEndian) {
   memory_buffer buf;
   auto grown = buf.grow(3);
   ASSERT_TRUE(grown.has_value());
   constexpr std::uint32_t original = 0x00ABCDEF;
   auto write_result =
       maboroutu::write_uint<3, maboroutu::endian::big>(buf, grown->offset, original);
   ASSERT_TRUE(write_result.has_value());

   auto const &raw = buf.bytes();
   ASSERT_EQ(raw.size(), 3u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[0]), 0xABu);
   EXPECT_EQ(std::to_integer<unsigned>(raw[1]), 0xCDu);
   EXPECT_EQ(std::to_integer<unsigned>(raw[2]), 0xEFu);

   auto read_result =
       maboroutu::read_uint<3, maboroutu::endian::big>(buf, grown->offset);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

TEST(MaboroutuBinaryLayout, Width3NonStandardRawBytesLittleEndian) {
   memory_buffer buf;
   auto grown = buf.grow(3);
   ASSERT_TRUE(grown.has_value());
   constexpr std::uint32_t original = 0x00ABCDEF;
   auto write_result = maboroutu::write_uint<3, maboroutu::endian::little>(
       buf, grown->offset, original);
   ASSERT_TRUE(write_result.has_value());

   auto const &raw = buf.bytes();
   ASSERT_EQ(raw.size(), 3u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[0]), 0xEFu);
   EXPECT_EQ(std::to_integer<unsigned>(raw[1]), 0xCDu);
   EXPECT_EQ(std::to_integer<unsigned>(raw[2]), 0xABu);

   auto read_result =
       maboroutu::read_uint<3, maboroutu::endian::little>(buf, grown->offset);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, original);
}

// --- 範囲外読み取り ------------------------------------------------------
TEST(MaboroutuBinaryLayout, ReadUintBeyondEndOfSourceFails) {
   memory_buffer buf;
   ASSERT_TRUE(buf.grow(2).has_value()); // 2byteしか無い
   auto result = maboroutu::read_uint<4, maboroutu::endian::big>(buf, 0);
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
}

// --- 隣接領域の非汚染 -----------------------------------------------------
TEST(MaboroutuBinaryLayout, ConsecutiveWriteUintDoNotCorruptAdjacentRegions) {
   memory_buffer buf;
   auto grown = buf.grow(8); // uint16_t x4
   ASSERT_TRUE(grown.has_value());

   std::array<std::uint16_t, 4> values{0x1111, 0x2222, 0x3333, 0x4444};
   for (std::size_t i = 0; i < values.size(); ++i) {
      auto write_result = maboroutu::write_uint<2, maboroutu::endian::big>(
          buf, grown->offset + i * 2, values[i]);
      ASSERT_TRUE(write_result.has_value());
   }
   for (std::size_t i = 0; i < values.size(); ++i) {
      auto read_result = maboroutu::read_uint<2, maboroutu::endian::big>(
          buf, grown->offset + i * 2);
      ASSERT_TRUE(read_result.has_value());
      EXPECT_EQ(*read_result, values[i]);
   }
}

// =======================================================================
// index_ref
// =======================================================================
namespace table_tags {
struct sample_table {};
} // namespace table_tags

TEST(MaboroutuBinaryLayout, IndexRefHoldsIndexValue) {
   maboroutu::index_ref<table_tags::sample_table> ref{.index = 42};
   EXPECT_EQ(ref.index, 42u);
}

// =======================================================================
// offset_patch / reserve_patch
// =======================================================================
TEST(MaboroutuBinaryLayout, ReservePatchIsZeroFilledInitially) {
   memory_buffer buf;
   auto patch = maboroutu::reserve_patch<4, maboroutu::endian::big>(buf);
   ASSERT_TRUE(patch.has_value());

   auto const &raw = buf.bytes();
   ASSERT_EQ(raw.size(), 4u);
   for (auto b : raw) {
      EXPECT_EQ(std::to_integer<unsigned>(b), 0u);
   }
   patch->abandon(); // デストラクタのassert回避
}

TEST(MaboroutuBinaryLayout, ResolvePatchWritesFinalValueReadableViaReadUint) {
   memory_buffer buf;
   auto patch = maboroutu::reserve_patch<4, maboroutu::endian::big>(buf);
   ASSERT_TRUE(patch.has_value());
   auto const placeholder = patch->placeholder();

   auto resolve_result = patch->resolve(std::uint32_t{0xCAFEBABE});
   ASSERT_TRUE(resolve_result.has_value());
   EXPECT_TRUE(patch->resolved());

   auto read_result =
       maboroutu::read_uint<4, maboroutu::endian::big>(buf, placeholder.offset);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, 0xCAFEBABEu);
}

TEST(MaboroutuBinaryLayout, NestedPatchesResolveOuterWithInnerEndOffset) {
   // FBXのEndOffset相当シナリオ: 親のプレースホルダを、子要素を書き込んだ
   // 後の終端オフセットで解決する。
   memory_buffer buf;
   auto outer_patch = maboroutu::reserve_patch<4, maboroutu::endian::big>(buf);
   ASSERT_TRUE(outer_patch.has_value());

   // 子要素本体を書き込む。
   std::array<std::byte, 6> child_payload{};
   for (std::size_t i = 0; i < child_payload.size(); ++i) {
      child_payload[i] = static_cast<std::byte>(i);
   }
   auto appended = buf.append(
       std::span<std::byte const>(child_payload.data(), child_payload.size()));
   ASSERT_TRUE(appended.has_value());
   auto const end_offset = appended->offset + appended->size;

   // 外側(親)のプレースホルダを、子要素の終端オフセットで解決する。
   ASSERT_TRUE(
       outer_patch->resolve(static_cast<std::uint32_t>(end_offset)).has_value());

   auto read_result = maboroutu::read_uint<4, maboroutu::endian::big>(
       buf, outer_patch->placeholder().offset);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, end_offset);
}

TEST(MaboroutuBinaryLayout, AbandonedPatchDestructsWithoutAssertFailure) {
   memory_buffer buf;
   auto patch = maboroutu::reserve_patch<4, maboroutu::endian::big>(buf);
   ASSERT_TRUE(patch.has_value());
   patch->abandon();
   EXPECT_TRUE(patch->resolved());
   // ここでスコープを抜けてデストラクタが呼ばれるが、abandon済みのため
   // assertは発火しない(発火すればこのテスト自体がクラッシュする)。
}

#if defined(GTEST_HAS_DEATH_TEST) && !defined(NDEBUG)
TEST(MaboroutuBinaryLayoutDeathTest, UnresolvedPatchDestructionTriggersAssert) {
   EXPECT_DEATH(
       {
          memory_buffer buf;
          auto patch =
              (maboroutu::reserve_patch<4, maboroutu::endian::big>(buf));
          ASSERT_TRUE(patch.has_value());
          // resolve()もabandon()もせずスコープを抜ける。
       },
       "");
}

TEST(MaboroutuBinaryLayoutDeathTest, DoubleResolveTriggersAssert) {
   EXPECT_DEATH(
       {
          memory_buffer buf;
          auto patch =
              (maboroutu::reserve_patch<4, maboroutu::endian::big>(buf));
          ASSERT_TRUE(patch.has_value());
          static_cast<void>(patch->resolve(std::uint32_t{1}));
          static_cast<void>(patch->resolve(std::uint32_t{2})); // 二重解決
       },
       "");
}
#endif // GTEST_HAS_DEATH_TEST && !NDEBUG

// =======================================================================
// TLV (Tag-Length-Value)
// =======================================================================
enum class sample_tag : std::uint8_t { alpha = 1, beta = 2 };

// --- 生バイト列の直接検証（★最重要。read_tlv_record非経由） -----------
TEST(MaboroutuBinaryLayout, WriteTlvRecordProducesExpectedRawBytes) {
   memory_buffer buf;
   std::array<std::byte, 3> payload{std::byte{0xAA}, std::byte{0xBB},
                                    std::byte{0xCC}};
   auto write_result =
       maboroutu::write_tlv_record<1, 2, maboroutu::endian::big, sample_tag>(
           buf, sample_tag::alpha,
           std::span<std::byte const>(payload.data(), payload.size()));
   ASSERT_TRUE(write_result.has_value());

   auto const &raw = buf.bytes();
   ASSERT_EQ(raw.size(), 1u + 2u + 3u);
   EXPECT_EQ(std::to_integer<unsigned>(raw[0]), 1u); // tag = alpha
   EXPECT_EQ(std::to_integer<unsigned>(raw[1]), 0u); // length高位byte
   EXPECT_EQ(std::to_integer<unsigned>(raw[2]), 3u); // length低位byte
   EXPECT_EQ(std::to_integer<unsigned>(raw[3]), 0xAAu);
   EXPECT_EQ(std::to_integer<unsigned>(raw[4]), 0xBBu);
   EXPECT_EQ(std::to_integer<unsigned>(raw[5]), 0xCCu);
}

TEST(MaboroutuBinaryLayout, WriteThenReadTlvRecordRoundTrip) {
   memory_buffer buf;
   std::array<std::byte, 4> payload{std::byte{1}, std::byte{2}, std::byte{3},
                                    std::byte{4}};
   auto write_result =
       maboroutu::write_tlv_record<1, 2, maboroutu::endian::little, sample_tag>(
           buf, sample_tag::beta,
           std::span<std::byte const>(payload.data(), payload.size()));
   ASSERT_TRUE(write_result.has_value());

   auto read_result =
       maboroutu::read_tlv_record<1, 2, maboroutu::endian::little, sample_tag>(
           buf, write_result->offset);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(read_result->tag, sample_tag::beta);
   EXPECT_EQ(read_result->payload.size, payload.size());
   EXPECT_EQ(read_result->record.offset, write_result->offset);
   EXPECT_EQ(read_result->record.size, write_result->size);

   auto payload_read = buf.read(read_result->payload);
   ASSERT_TRUE(payload_read.has_value());
   for (std::size_t i = 0; i < payload.size(); ++i) {
      EXPECT_EQ(payload_read->value[i], payload[i]);
   }
}

TEST(MaboroutuBinaryLayout, MultipleTlvRecordsCanBeWalkedSequentially) {
   memory_buffer buf;
   std::array<std::byte, 2> payload_a{std::byte{0x01}, std::byte{0x02}};
   std::array<std::byte, 1> payload_b{std::byte{0xFF}};

   auto rec_a =
       maboroutu::write_tlv_record<1, 1, maboroutu::endian::big, sample_tag>(
           buf, sample_tag::alpha,
           std::span<std::byte const>(payload_a.data(), payload_a.size()));
   ASSERT_TRUE(rec_a.has_value());
   auto rec_b =
       maboroutu::write_tlv_record<1, 1, maboroutu::endian::big, sample_tag>(
           buf, sample_tag::beta,
           std::span<std::byte const>(payload_b.data(), payload_b.size()));
   ASSERT_TRUE(rec_b.has_value());

   // record.offset + record.size で次レコード位置を求めながら前進する
   // (終端判定は呼び出し側の責務、という設計の確認)。
   auto first =
       maboroutu::read_tlv_record<1, 1, maboroutu::endian::big, sample_tag>(buf,
                                                                            0);
   ASSERT_TRUE(first.has_value());
   EXPECT_EQ(first->tag, sample_tag::alpha);

   auto const next_offset = first->record.offset + first->record.size;
   auto second =
       maboroutu::read_tlv_record<1, 1, maboroutu::endian::big, sample_tag>(
           buf, next_offset);
   ASSERT_TRUE(second.has_value());
   EXPECT_EQ(second->tag, sample_tag::beta);

   // さらに前進するとデータが尽きており、out_of_rangeで失敗する
   // (終端判定を呼び出し側の失敗検知に委ねる設計の確認)。
   auto const past_end_offset = second->record.offset + second->record.size;
   auto third =
       maboroutu::read_tlv_record<1, 1, maboroutu::endian::big, sample_tag>(
           buf, past_end_offset);
   EXPECT_FALSE(third.has_value());
}

} // namespace
