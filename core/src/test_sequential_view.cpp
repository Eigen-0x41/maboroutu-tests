// mylib.sequential_view (maboroutu.sequential_view) のテスト。
// library_spec.md 4.9節・6章。
#include <array>
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
import maboroutu.sequential_view;

namespace {

using ds_code_type =
    decltype(std::declval<maboroutu::data_source_result<int>>().error().code());

// このファイル専用の、書き込みを実際に保持するdata_sourceモック
// （test_binary_convert.cpp の memory_data_source と役割は同じだが、
//  各テストファイルはモジュールと同様に自己完結させる方針のため
//  重複して用意している）。
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
static_assert(maboroutu::data_source<memory_data_source>);
static_assert(maboroutu::writable_data_source<memory_data_source>);

// ---------------------------------------------------------------------
// concept充足 (4.9節末尾のstatic_assertと同内容だが、消費側からも確認)
// ---------------------------------------------------------------------
static_assert(
    maboroutu::sequential_source<maboroutu::sequential_view<memory_data_source>>);
static_assert(maboroutu::writable_sequential_source<
              maboroutu::sequential_view<memory_data_source>>);

// ---------------------------------------------------------------------
// count()の累積前進 (6章 検証方針(1))
// ---------------------------------------------------------------------
TEST(MaboroutuSequentialView, CountAccumulatesAcrossReadsAndWrites) {
   memory_data_source src(sizeof(std::uint32_t) + sizeof(std::uint16_t) * 3);
   maboroutu::sequential_view view{src};
   EXPECT_EQ(view.count(), 0u);

   ASSERT_TRUE(
       maboroutu::write_value<maboroutu::endian::little>(view, std::uint32_t{7})
           .has_value());
   EXPECT_EQ(view.count(), sizeof(std::uint32_t));

   std::array<std::uint16_t, 3> tail{1, 2, 3};
   ASSERT_TRUE((maboroutu::write_array<maboroutu::endian::little, std::uint16_t,
                                       3>(view, tail)
                    .has_value()));
   EXPECT_EQ(view.count(), sizeof(std::uint32_t) + sizeof(std::uint16_t) * 3);
}

// ---------------------------------------------------------------------
// 失敗時は count() を進めない (6章 検証方針(2)、v1.17短縮読み込み契約の精神)
// ---------------------------------------------------------------------
TEST(MaboroutuSequentialView, CountDoesNotAdvanceOnFailedRead) {
   memory_data_source src(2); // std::uint32_t(4byte) には満たない容量
   maboroutu::sequential_view view{src};

   auto result =
       maboroutu::read_value<maboroutu::endian::little, std::uint32_t>(view);
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(view.count(), 0u);
}

// ---------------------------------------------------------------------
// offset版とsequential_view経由が同一の値を返す (6章 検証方針(3))
// ---------------------------------------------------------------------
TEST(MaboroutuSequentialView, AgreesWithOffsetVersionOnSameBytes) {
   constexpr std::size_t total = sizeof(std::uint32_t) + sizeof(std::uint16_t);
   memory_data_source src(total);
   ASSERT_TRUE(maboroutu::write_value<maboroutu::endian::little>(
                   src, 0, std::uint32_t{123})
                   .has_value());
   ASSERT_TRUE(maboroutu::write_value<maboroutu::endian::little>(
                   src, sizeof(std::uint32_t), std::uint16_t{45})
                   .has_value());

   auto offset_a =
       maboroutu::read_value<maboroutu::endian::little, std::uint32_t>(src, 0);
   auto offset_b = maboroutu::read_value<maboroutu::endian::little, std::uint16_t>(
       src, sizeof(std::uint32_t));
   ASSERT_TRUE(offset_a.has_value());
   ASSERT_TRUE(offset_b.has_value());

   maboroutu::sequential_view view{src};
   auto view_a =
       maboroutu::read_value<maboroutu::endian::little, std::uint32_t>(view);
   auto view_b =
       maboroutu::read_value<maboroutu::endian::little, std::uint16_t>(view);
   ASSERT_TRUE(view_a.has_value());
   ASSERT_TRUE(view_b.has_value());

   EXPECT_EQ(*offset_a, *view_a);
   EXPECT_EQ(*offset_b, *view_b);
}

// ---------------------------------------------------------------------
// write()（6章 検証方針(4)、writable_data_source制約）
// ---------------------------------------------------------------------
TEST(MaboroutuSequentialView, WriteRequiresWritableUnderlyingSource) {
   memory_data_source src(4);
   maboroutu::sequential_view view{src};
   std::array<std::byte, 4> data{};
   auto result =
       view.write(std::span<std::byte const>(data.data(), data.size()));
   EXPECT_TRUE(result.has_value());
   EXPECT_EQ(view.count(), 4u);
}

// ---------------------------------------------------------------------
// skip(): count() をn前進させるオプション実装
// ---------------------------------------------------------------------
TEST(MaboroutuSequentialView, SkipAdvancesCountWithoutReading) {
   memory_data_source src(8);
   maboroutu::sequential_view view{src};
   view.skip(3);
   EXPECT_EQ(view.count(), 3u);
   auto result = maboroutu::read_value<maboroutu::endian::little, std::uint8_t>(
       view); // 残り5byte中1byte読み込み
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(view.count(), 4u);
}

// ---------------------------------------------------------------------
// 初期位置の指定（コンストラクタ引数、seek()は非公開）
// ---------------------------------------------------------------------
TEST(MaboroutuSequentialView, InitialCountCanBeSpecifiedViaConstructor) {
   memory_data_source src(8);
   maboroutu::sequential_view view{src, /*initial_count=*/4};
   EXPECT_EQ(view.count(), 4u);
}

} // namespace
