// mylib.memory_data_buffer (maboroutu.memory_data_buffer) のテスト。
//
// NOTE（一次情報）: 本モジュールはlibrary_spec.mdにまだコンポーネント節が
// 無い新規モジュール。テストは映幻から提供された引き継ぎ資料
// 「maboroutu — GTest実装のための引き継ぎ資料」(3.2節・付録A・4章)を
// 一次情報として作成した。
//
// write() の `assert(data.size() == reg.size)` はデバッグビルド前提の
// 事前条件チェックであるため、その違反を確認するテストは
// NDEBUGが定義されていないビルド構成が前提となる。
#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.data_source;
import maboroutu.data_buffer;
import maboroutu.memory_data_buffer;

namespace {

using ds_code_type =
    decltype(std::declval<maboroutu::data_source_result<int>>().error().code());
using buffer_code_type =
    decltype(std::declval<maboroutu::data_buffer_result<int>>().error().code());

static_assert(maboroutu::data_buffer<maboroutu::memory_data_buffer>);

// ---------------------------------------------------------------------
// 初期状態
// ---------------------------------------------------------------------
TEST(MaboroutuMemoryDataBuffer, InitiallyEmpty) {
   maboroutu::memory_data_buffer buf;
   auto size_result = buf.size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 0u);
}

// ---------------------------------------------------------------------
// append
// ---------------------------------------------------------------------
TEST(MaboroutuMemoryDataBuffer, AppendGrowsSizeAndStoresContent) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 3> data{std::byte{1}, std::byte{2}, std::byte{3}};
   auto appended =
       buf.append(std::span<std::byte const>(data.data(), data.size()));
   ASSERT_TRUE(appended.has_value());
   EXPECT_EQ(appended->offset, 0u);
   EXPECT_EQ(appended->size, 3u);

   auto size_result = buf.size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 3u);

   auto read_result = buf.read(*appended);
   ASSERT_TRUE(read_result.has_value());
   for (std::size_t i = 0; i < data.size(); ++i) {
      EXPECT_EQ(read_result->value[i], data[i]);
   }
}

TEST(MaboroutuMemoryDataBuffer, SecondAppendContinuesFromCurrentEnd) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 2> first{std::byte{0xAA}, std::byte{0xBB}};
   std::array<std::byte, 2> second{std::byte{0xCC}, std::byte{0xDD}};
   auto first_region =
       buf.append(std::span<std::byte const>(first.data(), first.size()));
   ASSERT_TRUE(first_region.has_value());
   auto second_region =
       buf.append(std::span<std::byte const>(second.data(), second.size()));
   ASSERT_TRUE(second_region.has_value());
   EXPECT_EQ(second_region->offset, first_region->offset + first_region->size);
}

// ---------------------------------------------------------------------
// read
// ---------------------------------------------------------------------
TEST(MaboroutuMemoryDataBuffer, ReadBeyondEndFailsWithoutShortRead) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 2> data{};
   ASSERT_TRUE(
       buf.append(std::span<std::byte const>(data.data(), data.size()))
           .has_value());

   auto result = buf.read(maboroutu::region{.offset = 0, .size = 100});
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
}

// ---------------------------------------------------------------------
// write
// ---------------------------------------------------------------------
TEST(MaboroutuMemoryDataBuffer, WriteOverwritesExistingRange) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 4> original{std::byte{1}, std::byte{2}, std::byte{3},
                                     std::byte{4}};
   ASSERT_TRUE(
       buf.append(std::span<std::byte const>(original.data(), original.size()))
           .has_value());

   std::array<std::byte, 2> patch{std::byte{0xFF}, std::byte{0xEE}};
   auto write_result =
       buf.write(maboroutu::region{.offset = 1, .size = 2},
                std::span<std::byte const>(patch.data(), patch.size()));
   ASSERT_TRUE(write_result.has_value());

   auto read_result = buf.read(maboroutu::region{.offset = 0, .size = 4});
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(read_result->value[0], std::byte{1});
   EXPECT_EQ(read_result->value[1], std::byte{0xFF});
   EXPECT_EQ(read_result->value[2], std::byte{0xEE});
   EXPECT_EQ(read_result->value[3], std::byte{4});
}

TEST(MaboroutuMemoryDataBuffer, WriteBeyondCurrentSizeAutoExpands) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 2> data{std::byte{0x11}, std::byte{0x22}};
   auto write_result =
       buf.write(maboroutu::region{.offset = 5, .size = 2},
                std::span<std::byte const>(data.data(), data.size()));
   ASSERT_TRUE(write_result.has_value());

   auto size_result = buf.size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 7u);

   auto read_result = buf.read(maboroutu::region{.offset = 5, .size = 2});
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(read_result->value[0], std::byte{0x11});
   EXPECT_EQ(read_result->value[1], std::byte{0x22});
}

#if defined(GTEST_HAS_DEATH_TEST) && !defined(NDEBUG)
TEST(MaboroutuMemoryDataBufferDeathTest,
    WriteWithMismatchedDataSizeTriggersAssert) {
   std::array<std::byte, 2> data{};
   EXPECT_DEATH(
       {
          maboroutu::memory_data_buffer buf;
          // region.size(3) と data.size()(2) が不一致。
          static_cast<void>(
              buf.write(maboroutu::region{.offset = 0, .size = 3},
                       std::span<std::byte const>(data.data(), data.size())));
       },
       "");
}
#endif // GTEST_HAS_DEATH_TEST && !NDEBUG

// ---------------------------------------------------------------------
// grow
// ---------------------------------------------------------------------
TEST(MaboroutuMemoryDataBuffer, GrowIsZeroFilledThenWritableAndReadable) {
   maboroutu::memory_data_buffer buf;
   auto grown = buf.grow(4);
   ASSERT_TRUE(grown.has_value());
   EXPECT_EQ(grown->offset, 0u);
   EXPECT_EQ(grown->size, 4u);

   auto zero_read = buf.read(*grown);
   ASSERT_TRUE(zero_read.has_value());
   for (std::size_t i = 0; i < 4; ++i) {
      EXPECT_EQ(zero_read->value[i], std::byte{0});
   }

   std::array<std::byte, 4> patch{std::byte{9}, std::byte{8}, std::byte{7},
                                  std::byte{6}};
   ASSERT_TRUE(buf.write(*grown, std::span<std::byte const>(patch.data(),
                                                            patch.size()))
                   .has_value());

   auto patched_read = buf.read(*grown);
   ASSERT_TRUE(patched_read.has_value());
   for (std::size_t i = 0; i < 4; ++i) {
      EXPECT_EQ(patched_read->value[i], patch[i]);
   }
}

// ---------------------------------------------------------------------
// view
// ---------------------------------------------------------------------
TEST(MaboroutuMemoryDataBuffer, ViewWritesAreReflectedInBuffer) {
   maboroutu::memory_data_buffer buf;
   ASSERT_TRUE(buf.grow(3).has_value());

   auto view_result = buf.view(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(view_result.has_value());
   (*view_result)[0] = std::byte{0x11};
   (*view_result)[1] = std::byte{0x22};
   (*view_result)[2] = std::byte{0x33};

   auto read_result = buf.read(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(read_result->value[0], std::byte{0x11});
   EXPECT_EQ(read_result->value[1], std::byte{0x22});
   EXPECT_EQ(read_result->value[2], std::byte{0x33});
}

TEST(MaboroutuMemoryDataBuffer, ViewBeyondEndFails) {
   maboroutu::memory_data_buffer buf;
   ASSERT_TRUE(buf.grow(2).has_value());
   auto view_result = buf.view(maboroutu::region{.offset = 0, .size = 10});
   ASSERT_FALSE(view_result.has_value());
   EXPECT_EQ(view_result.error().code(), buffer_code_type::out_of_range);
}

// ---------------------------------------------------------------------
// 型消去ラッパー(data_source_handle / writable_data_source_handle)経由
// ---------------------------------------------------------------------
static_assert(
    std::is_constructible_v<maboroutu::data_source_handle, maboroutu::memory_data_buffer &>);
static_assert(
    std::is_constructible_v<maboroutu::writable_data_source_handle,
                            maboroutu::memory_data_buffer &>);

TEST(MaboroutuMemoryDataBuffer, WorksThroughWritableDataSourceHandle) {
   maboroutu::memory_data_buffer buf;
   ASSERT_TRUE(buf.grow(4).has_value());

   maboroutu::writable_data_source_handle handle{buf};
   std::array<std::byte, 4> data{std::byte{1}, std::byte{2}, std::byte{3},
                                 std::byte{4}};
   auto write_result =
       handle.write(maboroutu::region{.offset = 0, .size = 4},
                    std::span<std::byte const>(data.data(), data.size()));
   ASSERT_TRUE(write_result.has_value());

   auto read_result = handle.read(maboroutu::region{.offset = 0, .size = 4});
   ASSERT_TRUE(read_result.has_value());
   for (std::size_t i = 0; i < 4; ++i) {
      EXPECT_EQ(read_result->value[i], data[i]);
   }
}

} // namespace
