// mylib.memory_data_buffer (maboroutu.memory_data_buffer) のテスト。
//
// write() の `assert(data.size() == reg.size)` はデバッグビルド前提の
// 事前条件チェックであるため、その違反を確認するテストは
// NDEBUGが定義されていないビルド構成が前提となる。
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.data_source;
import maboroutu.data_buffer;
import maboroutu.memory_data_buffer;


// ---------------------------------------------------------------------
// アロケーション失敗の注入。
// このテスト実行ファイルの operator new を置き換え、g_fail_countdown 回目の
// 確保で std::bad_alloc を送出する。-1 の間は、通常どおり確保する。
// ---------------------------------------------------------------------
namespace {
std::atomic<long> g_fail_countdown{-1};
} // namespace

void *operator new(std::size_t size) {
   auto const countdown = g_fail_countdown.load();
   if (countdown == 0) {
      g_fail_countdown.store(-1);
      throw std::bad_alloc();
   }
   if (countdown > 0) {
      g_fail_countdown.store(countdown - 1);
   }
   if (void *ptr = std::malloc(size ? size : 1)) {
      return ptr;
   }
   throw std::bad_alloc();
}
void operator delete(void *ptr) noexcept { std::free(ptr); }
void operator delete(void *ptr, std::size_t) noexcept { std::free(ptr); }

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

// ---------------------------------------------------------------------
// offset + size が size_t を超える領域（回帰テスト）
// ---------------------------------------------------------------------
TEST(MaboroutuMemoryDataBuffer, WriteWithRegionOverflowFailsWithoutChange) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 4> data{};
   ASSERT_TRUE(
       buf.append(std::span<std::byte const>(data.data(), data.size()))
           .has_value());

   std::array<std::byte, 8> payload{};
   auto result = buf.write(
       maboroutu::region{.offset = ~std::size_t{0} - 2, .size = payload.size()},
       std::span<std::byte const>(payload.data(), payload.size()));
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
   EXPECT_EQ(*buf.size(), data.size());
}

TEST(MaboroutuMemoryDataBuffer, AppendAndGrowWithSizeOverflowFail) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 4> data{};
   ASSERT_TRUE(
       buf.append(std::span<std::byte const>(data.data(), data.size()))
           .has_value());

   auto grown = buf.grow(~std::size_t{0});
   ASSERT_FALSE(grown.has_value());
   EXPECT_EQ(grown.error().code(), buffer_code_type::out_of_range);
   EXPECT_EQ(*buf.size(), data.size());
}

// 新しいサイズがSIZE_MAX付近の場合、ブロック数の算出（切り上げ）が桁あふれして、
// 領域が確保されないまま size() だけが大きくなる不整合が起きる（回帰テスト）。
TEST(MaboroutuMemoryDataBuffer, GrowToSizeNearSizeMaxFailsWithoutChange) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 10> data{};
   ASSERT_TRUE(
       buf.append(std::span<std::byte const>(data.data(), data.size()))
           .has_value());

   // size() + n == SIZE_MAX（加算自体は桁あふれしない）。
   auto grown = buf.grow(~std::size_t{0} - data.size());
   ASSERT_FALSE(grown.has_value());
   EXPECT_EQ(grown.error().code(), buffer_code_type::out_of_range);
   EXPECT_EQ(*buf.size(), data.size());
   EXPECT_TRUE(buf.view(maboroutu::region{.offset = 0, .size = data.size()})
                   .has_value());
}

TEST(MaboroutuMemoryDataBuffer, WriteEndingNearSizeMaxFailsWithoutChange) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 10> data{};
   ASSERT_TRUE(
       buf.append(std::span<std::byte const>(data.data(), data.size()))
           .has_value());

   std::array<std::byte, 5> payload{};
   // offset + size == SIZE_MAX - 5（桁あふれはしない）。
   auto result = buf.write(
       maboroutu::region{.offset = ~std::size_t{0} - 10, .size = payload.size()},
       std::span<std::byte const>(payload.data(), payload.size()));
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
   EXPECT_EQ(*buf.size(), data.size());
}

// ---------------------------------------------------------------------
// ムーブ後の状態（回帰テスト）: ムーブ元は空になり、そのまま再利用できる
// ---------------------------------------------------------------------
namespace {
auto make_filled_buffer(std::size_t n, std::byte value)
    -> maboroutu::memory_data_buffer {
   maboroutu::memory_data_buffer buf;
   std::vector<std::byte> data(n, value);
   auto appended =
       buf.append(std::span<std::byte const>(data.data(), data.size()));
   EXPECT_TRUE(appended.has_value());
   return buf;
}
} // namespace

TEST(MaboroutuMemoryDataBuffer, MoveConstructedSourceBecomesEmptyAndReusable) {
   auto src = make_filled_buffer(3, std::byte{0x5A});
   maboroutu::memory_data_buffer dst(std::move(src));

   EXPECT_EQ(*dst.size(), 3u);
   auto dst_read = dst.read(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(dst_read.has_value());
   EXPECT_EQ(dst_read->value[2], std::byte{0x5A});

   EXPECT_EQ(*src.size(), 0u);
   auto src_read = src.read(maboroutu::region{.offset = 0, .size = 1});
   ASSERT_FALSE(src_read.has_value());
   EXPECT_EQ(src_read.error().code(), ds_code_type::out_of_range);

   std::array<std::byte, 2> data{std::byte{1}, std::byte{2}};
   auto appended =
       src.append(std::span<std::byte const>(data.data(), data.size()));
   ASSERT_TRUE(appended.has_value());
   EXPECT_EQ(appended->offset, 0u);
   EXPECT_EQ(*src.size(), 2u);
   EXPECT_EQ(src.read(*appended)->value[1], std::byte{2});
}

TEST(MaboroutuMemoryDataBuffer, MoveAssignedSourceBecomesEmptyAndReusable) {
   auto src = make_filled_buffer(3, std::byte{0x5A});
   auto dst = make_filled_buffer(100, std::byte{0x11});
   dst = std::move(src);

   EXPECT_EQ(*dst.size(), 3u);
   auto dst_read = dst.read(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(dst_read.has_value());
   EXPECT_EQ(dst_read->value[0], std::byte{0x5A});
   EXPECT_FALSE(
       dst.read(maboroutu::region{.offset = 0, .size = 4}).has_value());

   EXPECT_EQ(*src.size(), 0u);
   EXPECT_FALSE(
       src.read(maboroutu::region{.offset = 0, .size = 1}).has_value());
}

} // namespace

TEST(MaboroutuMemoryDataBuffer, CopyAssignmentToSmallerBufferCopiesContent) {
   auto const src = make_filled_buffer(3, std::byte{0x5A});
   auto dst = make_filled_buffer(100, std::byte{0x11});
   dst = src;

   EXPECT_EQ(*dst.size(), 3u);
   auto dst_read = dst.read(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(dst_read.has_value());
   EXPECT_EQ(dst_read->value[2], std::byte{0x5A});
   EXPECT_EQ(*src.size(), 3u);
}

TEST(MaboroutuMemoryDataBuffer, CopyAssignmentToSelfKeepsContent) {
   auto buf = make_filled_buffer(70, std::byte{0x33});
   auto &alias = buf;
   buf = alias;

   EXPECT_EQ(*buf.size(), 70u);
   auto read = buf.read(maboroutu::region{.offset = 0, .size = 70});
   ASSERT_TRUE(read.has_value());
   EXPECT_EQ(read->value[69], std::byte{0x33});
}

TEST(MaboroutuMemoryDataBuffer, ReadIntoMatchesReadAndKeepsBytesBeyondRequest) {
   auto buf = make_filled_buffer(200, std::byte{0x42});
   std::array<std::byte, 150> dst{};
   dst.fill(std::byte{0xEE});
   auto result = buf.read_into(maboroutu::region{.offset = 10, .size = 140},
                               std::span<std::byte>(dst));
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(dst[0], std::byte{0x42});
   EXPECT_EQ(dst[139], std::byte{0x42});
   EXPECT_EQ(dst[140], std::byte{0xEE});
}

TEST(MaboroutuMemoryDataBuffer, ReadIntoBeyondSizeFailsWithoutChangingDst) {
   auto buf = make_filled_buffer(8, std::byte{0x42});
   std::array<std::byte, 16> dst{};
   dst.fill(std::byte{0xEE});
   auto result = buf.read_into(maboroutu::region{.offset = 4, .size = 8},
                               std::span<std::byte>(dst));
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(dst[0], std::byte{0xEE});
}

// ---------------------------------------------------------------------
// 範囲外への書き込み（0埋め）と、アロケーション失敗（v1.59）
// ---------------------------------------------------------------------
TEST(MaboroutuMemoryDataBuffer, WriteBeyondSizeFillsGapWithZero) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 4> head{std::byte{1}, std::byte{2}, std::byte{3},
                                 std::byte{4}};
   ASSERT_TRUE(buf.write(maboroutu::region{.offset = 0, .size = 4}, head)
                   .has_value());
   // 現在の大きさ（4）より先の、10000 から書く。間の区間は 0 で埋まる。
   std::array<std::byte, 2> tail{std::byte{0xAA}, std::byte{0xBB}};
   ASSERT_TRUE(buf.write(maboroutu::region{.offset = 10000, .size = 2}, tail)
                   .has_value());
   ASSERT_EQ(buf.size().value_or(0), 10002u);
   auto bytes = buf.read(maboroutu::region{.offset = 0, .size = 10002});
   ASSERT_TRUE(bytes.has_value());
   EXPECT_EQ(bytes->value[0], std::byte{1});
   EXPECT_EQ(bytes->value[3], std::byte{4});
   for (std::size_t i = 4; i < 10000; ++i) {
      ASSERT_EQ(bytes->value[i], std::byte{0}) << "offset " << i;
   }
   EXPECT_EQ(bytes->value[10000], std::byte{0xAA});
   EXPECT_EQ(bytes->value[10001], std::byte{0xBB});
}

TEST(MaboroutuMemoryDataBuffer, GrowFillsWithZero) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 3> data{std::byte{9}, std::byte{9}, std::byte{9}};
   ASSERT_TRUE(buf.append(data).has_value());
   auto region = buf.grow(5000);
   ASSERT_TRUE(region.has_value());
   auto bytes = buf.read(*region);
   ASSERT_TRUE(bytes.has_value());
   for (std::size_t i = 0; i < region->size; ++i) {
      ASSERT_EQ(bytes->value[i], std::byte{0}) << "offset " << i;
   }
}

#if defined(__cpp_exceptions)
// アロケーションに失敗した場合は allocation_failure を返し、バッファを
// 変更しない。以降の操作は、通常どおり成功する。
// NOTE: AddressSanitizer は、過大な確保要求を、既定で致命的エラーとして
//       終了する（allocator_may_return_null=0）ため、このテストはASanでは
//       実行しない。失敗の注入によるテスト（下記）は、ASanでも実行する。
#if defined(__SANITIZE_ADDRESS__)
#define MABOROUTU_TEST_UNDER_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define MABOROUTU_TEST_UNDER_ASAN 1
#endif
#endif
#if !defined(MABOROUTU_TEST_UNDER_ASAN)
TEST(MaboroutuMemoryDataBuffer, AllocationFailureLeavesBufferUnchanged) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 4> data{std::byte{1}, std::byte{2}, std::byte{3},
                                 std::byte{4}};
   ASSERT_TRUE(buf.append(data).has_value());

   // 要求サイズが過大で、確保できない（std::vector::reserve が失敗する）。
   constexpr auto huge = std::numeric_limits<std::size_t>::max() / 4;
   auto written = buf.write(maboroutu::region{.offset = huge, .size = 1},
                            std::array<std::byte, 1>{std::byte{7}});
   ASSERT_FALSE(written.has_value());
   EXPECT_EQ(written.error().code(), ds_code_type::allocation_failure);

   auto grown = buf.grow(huge);
   ASSERT_FALSE(grown.has_value());
   EXPECT_EQ(grown.error().code(), buffer_code_type::allocation_failure);

   std::vector<std::byte> big(1);
   auto appended = buf.append(std::span<std::byte const>(big));
   ASSERT_TRUE(appended.has_value());

   ASSERT_EQ(buf.size().value_or(0), 5u);
   auto bytes = buf.read(maboroutu::region{.offset = 0, .size = 4});
   ASSERT_TRUE(bytes.has_value());
   EXPECT_EQ(bytes->value[0], std::byte{1});
   EXPECT_EQ(bytes->value[3], std::byte{4});
}
#endif

// n 回目の確保で失敗させ、すべての段階で、バッファが変更されないこと、
// その後の操作が成功すること、を確認する（巻き戻しの経路の検証）。
TEST(MaboroutuMemoryDataBuffer, AllocationFailureAtEveryStepRollsBack) {
   // 複数のユニットにまたがる大きさ。
   constexpr std::size_t length = 40000;
   std::vector<std::byte> payload(length, std::byte{0x5A});

   bool failed_at_least_once = false;
   for (long step = 0; step < 64; ++step) {
      maboroutu::memory_data_buffer buf;
      std::array<std::byte, 2> head{std::byte{1}, std::byte{2}};
      ASSERT_TRUE(buf.append(head).has_value());

      g_fail_countdown.store(step);
      auto result = buf.append(std::span<std::byte const>(payload));
      g_fail_countdown.store(-1);

      if (result.has_value()) {
         // これ以上の段階では、失敗が起きない。
         EXPECT_EQ(buf.size().value_or(0), 2 + length);
         break;
      }
      failed_at_least_once = true;
      EXPECT_EQ(result.error().code(), buffer_code_type::allocation_failure)
          << "step " << step;
      // バッファは、変更されていない。
      ASSERT_EQ(buf.size().value_or(0), 2u) << "step " << step;
      auto bytes = buf.read(maboroutu::region{.offset = 0, .size = 2});
      ASSERT_TRUE(bytes.has_value());
      EXPECT_EQ(bytes->value[0], std::byte{1});
      EXPECT_EQ(bytes->value[1], std::byte{2});
      // 失敗の後も、通常どおり拡張できる。
      auto retry = buf.append(std::span<std::byte const>(payload));
      ASSERT_TRUE(retry.has_value()) << "step " << step;
      ASSERT_EQ(buf.size().value_or(0), 2 + length);
      auto tail = buf.read(maboroutu::region{.offset = 2, .size = length});
      ASSERT_TRUE(tail.has_value());
      EXPECT_EQ(tail->value[0], std::byte{0x5A});
      EXPECT_EQ(tail->value[length - 1], std::byte{0x5A});
   }
   EXPECT_TRUE(failed_at_least_once);
}

TEST(MaboroutuMemoryDataBuffer, CopyAssignmentFailureKeepsDestination) {
   maboroutu::memory_data_buffer source;
   std::vector<std::byte> payload(20000, std::byte{3});
   ASSERT_TRUE(source.append(std::span<std::byte const>(payload)).has_value());
   maboroutu::memory_data_buffer destination;
   std::array<std::byte, 1> one{std::byte{9}};
   ASSERT_TRUE(destination.append(one).has_value());

   g_fail_countdown.store(1);
   EXPECT_THROW(destination = source, std::bad_alloc);
   g_fail_countdown.store(-1);

   ASSERT_EQ(destination.size().value_or(0), 1u);
   auto bytes = destination.read(maboroutu::region{.offset = 0, .size = 1});
   ASSERT_TRUE(bytes.has_value());
   EXPECT_EQ(bytes->value[0], std::byte{9});
}
#endif
