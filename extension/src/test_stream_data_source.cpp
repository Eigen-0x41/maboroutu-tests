// mylib.stream_data_source (maboroutu.stream_data_source) のテスト。
// library_spec.md 8章 v1.26変更履歴に「新規タスクとして、std::iostreamベース
// のstream_data_sourceを別モジュールとして実装する構想を追加（設計未着手）」
// と記載されている、その実装済み版に対するテスト。
//
// NOTE: モジュール自体が `#if __STDC_HOSTED__ != 0` でガードされている
// (freestanding環境では既定で無効、3章のfreestandingガード方針の一種)。
// 本テストはhosted環境を前提とする。
#include <array>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.data_source;
import maboroutu.stream_data_source;

namespace {

using ds_code_type =
    decltype(std::declval<maboroutu::data_source_result<int>>().error().code());

// ---------------------------------------------------------------------
// concept充足・move-only性
// ---------------------------------------------------------------------
static_assert(
    maboroutu::data_source<maboroutu::stream_data_source<std::istream>>);
static_assert(
    !maboroutu::writable_data_source<maboroutu::stream_data_source<std::istream>>);
static_assert(!std::is_copy_constructible_v<
              maboroutu::stream_data_source<std::istream>>);
static_assert(!std::is_default_constructible_v<
              maboroutu::stream_data_source<std::istream>>);

static_assert(maboroutu::writable_data_source<
              maboroutu::stream_writable_data_source<std::iostream>>);

// ---------------------------------------------------------------------
// stream_data_source<std::istringstream>
// ---------------------------------------------------------------------
TEST(MaboroutuStreamDataSource, SizeReturnsStreamLength) {
   maboroutu::stream_data_source<std::istringstream> src(
       std::string("hello world"));
   auto size_result = src.size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 11u);
}

TEST(MaboroutuStreamDataSource, ReadReturnsRequestedRegion) {
   maboroutu::stream_data_source<std::istringstream> src(
       std::string("hello world"));
   auto result = src.read(maboroutu::region{.offset = 6, .size = 5});
   ASSERT_TRUE(result.has_value());
   ASSERT_EQ(result->size, 5u);
   std::string_view text(reinterpret_cast<char const *>(result->value.get()),
                         result->size);
   EXPECT_EQ(text, "world");
}

TEST(MaboroutuStreamDataSource, ReadBeyondEndFailsWithoutShortRead) {
   // read()の契約(library_spec.md 4.3節 v1.17)は data_source と同一
   // （短縮読み込みを行わない）。
   maboroutu::stream_data_source<std::istringstream> src(
       std::string("short"));
   auto result = src.read(maboroutu::region{.offset = 0, .size = 100});
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
}

TEST(MaboroutuStreamDataSource, SizeResetsPositionToBeginning) {
   // Doxygenコメント「サイズを取得した際後のポジションは先頭となる」の検証。
   maboroutu::stream_data_source<std::istringstream> src(
       std::string("0123456789"));
   ASSERT_TRUE(src.size().has_value());
   auto result = src.read(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(result.has_value());
   std::string_view text(reinterpret_cast<char const *>(result->value.get()),
                         result->size);
   EXPECT_EQ(text, "012");
}

// ---------------------------------------------------------------------
// stream_writable_data_source<std::stringstream>
// ---------------------------------------------------------------------
TEST(MaboroutuStreamWritableDataSource, WriteThenReadRoundTrip) {
   maboroutu::stream_writable_data_source<std::stringstream> src(
       std::string("0123456789"));

   std::array<std::byte, 5> payload{};
   char const text[] = "ABCDE";
   std::memcpy(payload.data(), text, payload.size());

   auto write_result = src.write(
       maboroutu::region{.offset = 2, .size = payload.size()},
       std::span<std::byte const>(payload.data(), payload.size()));
   ASSERT_TRUE(write_result.has_value());

   auto read_result =
       src.read(maboroutu::region{.offset = 2, .size = payload.size()});
   ASSERT_TRUE(read_result.has_value());
   std::string_view text_view(
       reinterpret_cast<char const *>(read_result->value.get()),
       read_result->size);
   EXPECT_EQ(text_view, "ABCDE");
}

TEST(MaboroutuStreamWritableDataSource, WriteWithInsufficientDataFails) {
   maboroutu::stream_writable_data_source<std::stringstream> src(
       std::string("0123456789"));
   std::array<std::byte, 2> small_payload{};
   // region.size(5) が data.size()(2) を超える。
   auto result = src.write(maboroutu::region{.offset = 0, .size = 5},
                           std::span<std::byte const>(small_payload.data(),
                                                       small_payload.size()));
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
}

} // namespace
