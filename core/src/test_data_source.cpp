// mylib.data_source (maboroutu.data_source) のテスト。
//
// errc::data_source はモジュールローカル・非exportであるため、本テストの
// カスタム data_source 実装（bounded_fake_source）は、decltype による
// 式ベースの型推論で errc::data_source という名前を一切綴らずに
// エラー値を構築している。外部からは decltype(...) 経由でCode型・列挙子へ
// 到達できることの検証も兼ねる。
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include <string_view>
#include <type_traits>
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

TEST(MaboroutuDataSource, NullDataSourceReadOfEmptyRegionReturnsEmptyArray) {
   maboroutu::null_data_source src;
   auto result = src.read(maboroutu::region{.offset = 0, .size = 0});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(result->size, 0u);
   EXPECT_NE(result->value, nullptr);
}

TEST(MaboroutuDataSource, NullDataSourceReadOfNonEmptyRegionFails) {
   maboroutu::null_data_source src;
   auto result = src.read(maboroutu::region{.offset = 0, .size = 8});
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(),
             decltype(result.error().code())::out_of_range);
}

TEST(MaboroutuDataSource, NullWritableDataSourceReadOfNonEmptyRegionFails) {
   maboroutu::null_writable_data_source src;
   EXPECT_TRUE(src.read(maboroutu::region{.offset = 0, .size = 0}).has_value());
   EXPECT_FALSE(
       src.read(maboroutu::region{.offset = 0, .size = 1}).has_value());
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

   auto read_result = handle.read(maboroutu::region{.offset = 0, .size = 0});
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(read_result->size, 0u);
   EXPECT_FALSE(
       handle.read(maboroutu::region{.offset = 0, .size = 5}).has_value());
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

// try_copy(): null_data_source は copy_constructible なので複製が成功する。
TEST(MaboroutuDataSource, TryCopySucceedsForCopyableConcreteType) {
   maboroutu::data_source_handle handle{maboroutu::null_data_source{}};
   std::optional<maboroutu::data_source_handle> cloned = handle.try_copy();
   ASSERT_TRUE(cloned.has_value());
   // 複製後も独立して read() を呼べる。
   auto result = cloned->read(maboroutu::region{.offset = 0, .size = 0});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(result->size, 0u);
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
   [[nodiscard]] auto read_into(maboroutu::region r,
                                std::span<std::byte> dst) const
       -> result_type<void> {
      std::fill_n(dst.data(), r.size, std::byte{0});
      return {};
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
// mylib::error を伴う std::unexpected を返す。
//
// errc::data_source を直接綴らずにエラー値を構築するため、
// data_source_result<T> の error_type::code_type を decltype で導出する。
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
   [[nodiscard]] auto read_into(maboroutu::region r,
                                std::span<std::byte> dst) const
       -> result_type<void> {
      if (r.offset + r.size > capacity) {
         return std::unexpected(maboroutu::error<data_source_code_type>(
             data_source_code_type::out_of_range));
      }
      std::fill_n(dst.data(), r.size, std::byte{0x5a});
      return {};
   }
};
static_assert(maboroutu::data_source<bounded_fake_source>);

TEST(MaboroutuDataSource, ReadIntoWritesOnlyRequestedPrefix) {
   bounded_fake_source src{.capacity = 16};
   std::array<std::byte, 8> dst{};
   auto result = src.read_into(maboroutu::region{.offset = 0, .size = 4},
                               std::span<std::byte>(dst));
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(dst[3], std::byte{0x5a});
   EXPECT_EQ(dst[4], std::byte{0});
}

TEST(MaboroutuDataSource, ReadIntoBeyondEndFailsAndPropagatesThroughHandle) {
   maboroutu::data_source_handle handle{bounded_fake_source{.capacity = 4}};
   std::array<std::byte, 8> dst{};
   auto result = handle.read_into(maboroutu::region{.offset = 0, .size = 8},
                                  std::span<std::byte>(dst));
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), data_source_code_type::out_of_range);
   auto ok = handle.read_into(maboroutu::region{.offset = 0, .size = 4},
                              std::span<std::byte>(dst));
   EXPECT_TRUE(ok.has_value());
   EXPECT_EQ(dst[0], std::byte{0x5a});
}

template <class Source> void expect_null_read_into_only_empty_region() {
   Source src;
   std::array<std::byte, 4> dst{};
   EXPECT_TRUE(src.read_into(maboroutu::region{.offset = 0, .size = 0},
                             std::span<std::byte>(dst))
                   .has_value());
   EXPECT_FALSE(src.read_into(maboroutu::region{.offset = 0, .size = 1},
                              std::span<std::byte>(dst))
                    .has_value());
}

TEST(MaboroutuDataSource, NullSourcesReadIntoOnlyEmptyRegion) {
   expect_null_read_into_only_empty_region<maboroutu::null_data_source>();
   expect_null_read_into_only_empty_region<
       maboroutu::null_writable_data_source>();
}

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

// errc::data_source を名前で直接参照できること（enum単位のexport）。
TEST(MaboroutuDataSourceErrc, EnumeratorsAreReachableByName) {
   using code = maboroutu::errc::data_source;
   static_assert(std::is_same_v<code, data_source_code_type>);
   maboroutu::error<code> e(code::out_of_range);
   EXPECT_EQ(e.code(), code::out_of_range);
   EXPECT_FALSE(e.message().empty());
   EXPECT_NE(maboroutu::errc::error_message(code::out_of_range),
             maboroutu::errc::error_message(code::operation_failure));
   EXPECT_EQ(maboroutu::errc::error_message(static_cast<code>(99)),
             "unknown error");
}
