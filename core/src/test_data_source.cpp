// mylib.data_source (maboroutu.data_source) のテスト。library_spec.md 4.3節。
//
// errc::data_source はモジュールローカル・非exportであるため（v1.29〜v1.31
// 確定方式）、本テストのカスタム data_source 実装（bounded_fake_source）は
// v1.31注記に記載された手法——decltype による式ベースの型推論——を用いて
// errc::data_source という名前を一切綴らずにエラー値を構築している。
// これは同注記の主張（「外部からは decltype(...) 経由でCode型・列挙子へ
// 到達可能」）が実際に機能することを示す実地検証も兼ねる。
#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.data_source;

namespace {

// ---------------------------------------------------------------------
// null_data_source / null_writable_data_source: concept充足の確認
// （ライブラリ側の static_assert と重複するが、消費側の翻訳単位からも
//   同じ契約が見えることを確認する）。
// ---------------------------------------------------------------------
static_assert(maboroutu::data_source<maboroutu::null_data_source>);
static_assert(!maboroutu::writable_data_source<maboroutu::null_data_source>);
static_assert(maboroutu::data_source<maboroutu::null_writable_data_source>);
static_assert(
    maboroutu::writable_data_source<maboroutu::null_writable_data_source>);

TEST(MaboroutuDataSource, NullDataSourceReadReturnsRequestedSize) {
   maboroutu::null_data_source src;
   auto result = src.read(maboroutu::region{.offset = 0, .size = 8});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(result->size, 8u);
}

TEST(MaboroutuDataSource, NullWritableDataSourceWriteAlwaysSucceeds) {
   maboroutu::null_writable_data_source src;
   std::array<std::byte, 4> buf{};
   auto result =
       src.write(maboroutu::region{.offset = 0, .size = buf.size()},
                std::span<std::byte const>(buf.data(), buf.size()));
   EXPECT_TRUE(result.has_value());
}

// ---------------------------------------------------------------------
// data_source_handle / writable_data_source_handle: 型消去・move-only性
// ---------------------------------------------------------------------
static_assert(maboroutu::data_source<maboroutu::data_source_handle>);
static_assert(!maboroutu::writable_data_source<maboroutu::data_source_handle>);
static_assert(!std::is_copy_constructible_v<maboroutu::data_source_handle>);
static_assert(std::is_move_constructible_v<maboroutu::data_source_handle>);

static_assert(
    maboroutu::writable_data_source<maboroutu::writable_data_source_handle>);
static_assert(
    !std::is_copy_constructible_v<maboroutu::writable_data_source_handle>);
static_assert(
    std::is_move_constructible_v<maboroutu::writable_data_source_handle>);

TEST(MaboroutuDataSource, HandleDelegatesReadAndSizeToWrappedValue) {
   maboroutu::data_source_handle handle{maboroutu::null_data_source{}};
   auto size_result = handle.size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 0u); // null_data_source::size() は常に0

   auto read_result = handle.read(maboroutu::region{.offset = 0, .size = 5});
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(read_result->size, 5u);
}

TEST(MaboroutuDataSource, WritableHandleDelegatesWrite) {
   maboroutu::writable_data_source_handle handle{
       maboroutu::null_writable_data_source{}};
   std::array<std::byte, 3> buf{};
   auto result =
       handle.write(maboroutu::region{.offset = 0, .size = buf.size()},
                    std::span<std::byte const>(buf.data(), buf.size()));
   EXPECT_TRUE(result.has_value());
}

// try_copy(): null_data_source は copy_constructible なので複製が成功する
// (4.3節 v1.23注記)。
TEST(MaboroutuDataSource, TryCopySucceedsForCopyableConcreteType) {
   maboroutu::data_source_handle handle{maboroutu::null_data_source{}};
   std::optional<maboroutu::data_source_handle> cloned = handle.try_copy();
   ASSERT_TRUE(cloned.has_value());
   // 複製後も独立して read() を呼べる。
   auto result = cloned->read(maboroutu::region{.offset = 0, .size = 2});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(result->size, 2u);
}

// move-onlyな具象型を包んだ場合、try_copy() は std::nullopt を返す。
struct move_only_data_source {
   template <class T> using result_type = maboroutu::data_source_result<T>;

   move_only_data_source() = default;
   move_only_data_source(move_only_data_source const &) = delete;
   move_only_data_source(move_only_data_source &&) = default;

   [[nodiscard]] auto size() const -> result_type<std::size_t> { return 0; }
   [[nodiscard]] auto read(maboroutu::region r) const
       -> result_type<maboroutu::byte_array> {
      return maboroutu::byte_array{
          .value =
              std::make_unique<maboroutu::byte_array::value_type>(r.size),
          .size = r.size,
      };
   }
};
static_assert(maboroutu::data_source<move_only_data_source>);
static_assert(!std::is_copy_constructible_v<move_only_data_source>);

TEST(MaboroutuDataSource, TryCopyReturnsNulloptForNonCopyableConcreteType) {
   maboroutu::data_source_handle handle{move_only_data_source{}};
   EXPECT_FALSE(handle.try_copy().has_value());
}

// ---------------------------------------------------------------------
// read() の契約: 短縮読み込みを行わず、要求範囲を満たせない場合は
// mylib::error を伴う std::unexpected を返す（library_spec.md 4.3節 v1.17）。
//
// errc::data_source を直接綴らずにエラー値を構築するため、
// data_source_result<T> の error_type::code_type を decltype で導出する
// (v1.31注記の手法)。
// ---------------------------------------------------------------------
using data_source_code_type =
    decltype(std::declval<maboroutu::data_source_result<int>>()
                 .error()
                 .code());

struct bounded_fake_source {
   std::size_t capacity;
   template <class T> using result_type = maboroutu::data_source_result<T>;

   [[nodiscard]] auto size() const -> result_type<std::size_t> {
      return capacity;
   }
   [[nodiscard]] auto read(maboroutu::region r) const
       -> result_type<maboroutu::byte_array> {
      if (r.offset + r.size > capacity) {
         return std::unexpected(maboroutu::error<data_source_code_type>(
             data_source_code_type::out_of_range));
      }
      return maboroutu::byte_array{
          .value =
              std::make_unique<maboroutu::byte_array::value_type>(r.size),
          .size = r.size,
      };
   }
};
static_assert(maboroutu::data_source<bounded_fake_source>);

TEST(MaboroutuDataSource, ReadWithinBoundsSucceeds) {
   bounded_fake_source src{.capacity = 16};
   auto result = src.read(maboroutu::region{.offset = 0, .size = 16});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(result->size, 16u);
}

TEST(MaboroutuDataSource, ReadBeyondEndOfSourceFailsWithoutShortRead) {
   bounded_fake_source src{.capacity = 16};
   // offset+size (10+16=26) がcapacity(16)を超える。
   auto result = src.read(maboroutu::region{.offset = 10, .size = 16});
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), data_source_code_type::out_of_range);
}

TEST(MaboroutuDataSource, HandlePropagatesErrorFromWrappedSource) {
   maboroutu::data_source_handle handle{bounded_fake_source{.capacity = 4}};
   auto result = handle.read(maboroutu::region{.offset = 0, .size = 100});
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), data_source_code_type::out_of_range);
}

} // namespace
