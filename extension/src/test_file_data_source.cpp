// maboroutu.file_data_source のテスト。
//
// NOTE: モジュール自体が `#if __STDC_HOSTED__ != 0` でガードされている
// (freestanding環境では既定で無効)。本テストはhosted環境を前提とする。
// 一時ファイルは、一時ディレクトリに一意な名前で作り、テストごとに削除する。
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.data_source;
import maboroutu.file_data_source;

namespace {

namespace fs = std::filesystem;

using ds_code_type =
    decltype(std::declval<maboroutu::data_source_result<int>>().error().code());

// ---------------------------------------------------------------------
// concept充足・move-only性
// ---------------------------------------------------------------------
static_assert(maboroutu::data_source<maboroutu::file_data_source>);
static_assert(!maboroutu::writable_data_source<maboroutu::file_data_source>);
static_assert(
    maboroutu::writable_data_source<maboroutu::file_writable_data_source>);
static_assert(!std::is_copy_constructible_v<maboroutu::file_data_source>);
static_assert(!std::is_default_constructible_v<maboroutu::file_data_source>);
// FILE* からは構築できない（生成は open() のみ）。
static_assert(!std::is_constructible_v<maboroutu::file_data_source, std::FILE *>);

// 内容を持つ一時ファイル。破棄時に削除する。
class temp_file {
 public:
   explicit temp_file(std::string_view content) {
      static std::atomic<unsigned> counter{0};
      std::random_device device;
      _path = fs::temp_directory_path() /
              ("maboroutu_fds_" + std::to_string(device()) + "_" +
               std::to_string(counter++) + ".bin");
      std::ofstream out(_path, std::ios::binary | std::ios::trunc);
      out.write(content.data(), static_cast<std::streamsize>(content.size()));
   }
   temp_file(temp_file const &) = delete;
   auto operator=(temp_file const &) -> temp_file & = delete;
   ~temp_file() {
      std::error_code ec;
      fs::remove(_path, ec);
   }
   [[nodiscard]] auto path() const -> fs::path const & { return _path; }

 private:
   fs::path _path;
};

auto as_text(maboroutu::byte_array const &bytes) -> std::string_view {
   return {reinterpret_cast<char const *>(bytes.value.get()), bytes.size};
}

// ---------------------------------------------------------------------
// open()
// ---------------------------------------------------------------------
TEST(MaboroutuFileDataSource, OpenMissingFileFails) {
   auto missing = fs::temp_directory_path() / "maboroutu_fds_does_not_exist.bin";
   auto result = maboroutu::file_data_source::open(missing);
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::operation_failure);
}

TEST(MaboroutuFileDataSource, OpenDirectoryFails) {
   // 通常のファイルではない。POSIXでは fopen が成功するため invalid_operation、
   // Windowsでは fopen が失敗するため operation_failure となる。
   auto result = maboroutu::file_data_source::open(fs::temp_directory_path());
   ASSERT_FALSE(result.has_value());
   auto const code = result.error().code();
   EXPECT_TRUE(code == ds_code_type::invalid_operation ||
               code == ds_code_type::operation_failure);
}

// ---------------------------------------------------------------------
// file_data_source
// ---------------------------------------------------------------------
TEST(MaboroutuFileDataSource, SizeReturnsFileLength) {
   temp_file file("hello world");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   auto size_result = src->size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 11u);
}

TEST(MaboroutuFileDataSource, SizeOfEmptyFileIsZero) {
   temp_file file("");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   auto size_result = src->size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 0u);
}

TEST(MaboroutuFileDataSource, ReadReturnsRequestedRegion) {
   temp_file file("hello world");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   auto result = src->read(maboroutu::region{.offset = 6, .size = 5});
   ASSERT_TRUE(result.has_value());
   ASSERT_EQ(result->size, 5u);
   EXPECT_EQ(as_text(*result), "world");
}

TEST(MaboroutuFileDataSource, ReadIsIndependentOfPreviousPosition) {
   temp_file file("0123456789");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   auto first = src->read(maboroutu::region{.offset = 7, .size = 3});
   ASSERT_TRUE(first.has_value());
   EXPECT_EQ(as_text(*first), "789");
   auto second = src->read(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(second.has_value());
   EXPECT_EQ(as_text(*second), "012");
   // size() の後でも、位置に依存しない。
   ASSERT_TRUE(src->size().has_value());
   auto third = src->read(maboroutu::region{.offset = 3, .size = 4});
   ASSERT_TRUE(third.has_value());
   EXPECT_EQ(as_text(*third), "3456");
}

TEST(MaboroutuFileDataSource, ReadZeroSizeAtEndSucceeds) {
   temp_file file("abc");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   auto result = src->read(maboroutu::region{.offset = 3, .size = 0});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(result->size, 0u);
}

TEST(MaboroutuFileDataSource, ReadBeyondEndFailsWithoutShortRead) {
   temp_file file("short");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   auto result = src->read(maboroutu::region{.offset = 0, .size = 100});
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
}

// 確保前に範囲を検証すること（回帰テスト）。
TEST(MaboroutuFileDataSource, ReadWithHugeSizeFailsBeforeAllocation) {
   temp_file file("short");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   auto result =
       src->read(maboroutu::region{.offset = 0, .size = std::size_t{1} << 40});
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
}

TEST(MaboroutuFileDataSource, ReadWithRegionOverflowFails) {
   temp_file file("short");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   auto result =
       src->read(maboroutu::region{.offset = 1, .size = ~std::size_t{0}});
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);
}

TEST(MaboroutuFileDataSource, ReadAfterFailedReadStillWorks) {
   temp_file file("hello world");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   ASSERT_FALSE(
       src->read(maboroutu::region{.offset = 0, .size = 100}).has_value());
   auto result = src->read(maboroutu::region{.offset = 6, .size = 5});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(as_text(*result), "world");
}

TEST(MaboroutuFileDataSource, ReadIntoWritesOnlyRequestedPrefix) {
   temp_file file("hello world");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   std::array<std::byte, 8> dst{};
   dst.fill(std::byte{0xAA});
   auto result =
       src->read_into(maboroutu::region{.offset = 6, .size = 5}, dst);
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(std::memcmp(dst.data(), "world", 5), 0);
   for (std::size_t i = 5; i < dst.size(); ++i) {
      EXPECT_EQ(dst[i], std::byte{0xAA});
   }
}

TEST(MaboroutuFileDataSource, ReadIntoBeyondEndFailsAndReadingContinues) {
   temp_file file("hello world");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   std::array<std::byte, 16> dst{};
   auto failed =
       src->read_into(maboroutu::region{.offset = 6, .size = 16}, dst);
   ASSERT_FALSE(failed.has_value());
   EXPECT_EQ(failed.error().code(), ds_code_type::out_of_range);
   auto ok = src->read_into(maboroutu::region{.offset = 0, .size = 5}, dst);
   ASSERT_TRUE(ok.has_value());
   EXPECT_EQ(std::memcmp(dst.data(), "hello", 5), 0);
}

TEST(MaboroutuFileDataSource, ReadIntoWithOffsetBeyondEndFails) {
   temp_file file("abc");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   std::array<std::byte, 4> dst{};
   auto failed =
       src->read_into(maboroutu::region{.offset = 1000, .size = 4}, dst);
   ASSERT_FALSE(failed.has_value());
   EXPECT_EQ(failed.error().code(), ds_code_type::out_of_range);
}

// offset が処理系のオフセット型で表せない場合も、read() と同じく out_of_range。
TEST(MaboroutuFileDataSource, UnrepresentableOffsetIsOutOfRange) {
   temp_file file("abc");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   std::array<std::byte, 4> dst{};
   maboroutu::region const reg{.offset = ~std::size_t{0} - 8, .size = 4};
   auto into = src->read_into(reg, dst);
   ASSERT_FALSE(into.has_value());
   EXPECT_EQ(into.error().code(), ds_code_type::out_of_range);
   auto read = src->read(reg);
   ASSERT_FALSE(read.has_value());
   EXPECT_EQ(read.error().code(), ds_code_type::out_of_range);
}

TEST(MaboroutuFileDataSource, WorksThroughDataSourceHandle) {
   temp_file file("hello world");
   auto opened = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(opened.has_value());
   maboroutu::data_source_handle handle(std::move(*opened));
   auto size_result = handle.size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 11u);
   auto result = handle.read(maboroutu::region{.offset = 0, .size = 5});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(as_text(*result), "hello");
   std::array<std::byte, 5> dst{};
   ASSERT_TRUE(
       handle.read_into(maboroutu::region{.offset = 6, .size = 5}, dst)
           .has_value());
   EXPECT_EQ(std::memcmp(dst.data(), "world", 5), 0);
   // FILE は複製できない。
   EXPECT_FALSE(handle.try_copy().has_value());
}

TEST(MaboroutuFileDataSource, MoveTransfersOwnership) {
   temp_file file("abc");
   auto a = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(a.has_value());
   auto *const raw = a->get();
   ASSERT_NE(raw, nullptr);
   maboroutu::file_data_source b(std::move(*a));
   EXPECT_EQ(b.get(), raw);
   auto result = b.read(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(as_text(*result), "abc");
}

TEST(MaboroutuFileDataSource, ReleaseHandsOwnershipBack) {
   temp_file file("abc");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   auto *const raw = src->get();
   std::FILE *released = src->release();
   EXPECT_EQ(released, raw);
   EXPECT_EQ(src->get(), nullptr);
   // release 後は、呼び出し側が閉じる責務を持つ。
   EXPECT_EQ(std::fclose(released), 0);
}

#if defined(__unix__) || defined(__APPLE__)
// FILE のエラー状態は、以降の操作を invalid_member_variable にする。
// 読み取り専用の FILE への書き込みは失敗し、エラー指示子が立つ。
TEST(MaboroutuFileDataSource, ErrorStateMakesLaterOperationsFail) {
   temp_file file("abc");
   auto src = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(src.has_value());
   EXPECT_EQ(std::fwrite("x", 1, 1, src->get()), 0u);
   ASSERT_NE(std::ferror(src->get()), 0);

   auto size_result = src->size();
   ASSERT_FALSE(size_result.has_value());
   EXPECT_EQ(size_result.error().code(), ds_code_type::invalid_member_variable);

   std::array<std::byte, 1> dst{};
   auto into = src->read_into(maboroutu::region{.offset = 0, .size = 1}, dst);
   ASSERT_FALSE(into.has_value());
   EXPECT_EQ(into.error().code(), ds_code_type::invalid_member_variable);
}
#endif

// ---------------------------------------------------------------------
// file_writable_data_source
// ---------------------------------------------------------------------
TEST(MaboroutuFileWritableDataSource, OpenExistingMissingFileFails) {
   auto missing = fs::temp_directory_path() / "maboroutu_fds_does_not_exist.bin";
   auto result = maboroutu::file_writable_data_source::open(
       missing, maboroutu::file_open_mode::existing);
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::operation_failure);
}

TEST(MaboroutuFileWritableDataSource, TruncateEmptiesTheFile) {
   temp_file file("0123456789");
   auto src = maboroutu::file_writable_data_source::open(
       file.path(), maboroutu::file_open_mode::truncate);
   ASSERT_TRUE(src.has_value());
   auto size_result = src->size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 0u);
}

TEST(MaboroutuFileWritableDataSource, ExistingKeepsContent) {
   temp_file file("0123456789");
   auto src = maboroutu::file_writable_data_source::open(
       file.path(), maboroutu::file_open_mode::existing);
   ASSERT_TRUE(src.has_value());
   auto result = src->read(maboroutu::region{.offset = 0, .size = 10});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(as_text(*result), "0123456789");
}

TEST(MaboroutuFileWritableDataSource, WriteThenReadRoundTrip) {
   temp_file file("0123456789");
   auto src = maboroutu::file_writable_data_source::open(
       file.path(), maboroutu::file_open_mode::existing);
   ASSERT_TRUE(src.has_value());
   std::array<std::byte, 3> data{std::byte{'a'}, std::byte{'b'},
                                 std::byte{'c'}};
   ASSERT_TRUE(
       src->write(maboroutu::region{.offset = 2, .size = 3}, data).has_value());
   auto result = src->read(maboroutu::region{.offset = 0, .size = 10});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(as_text(*result), "01abc56789");
}

// 書き込みはバッファに残ることがあるが、size() は書き込みを反映する。
TEST(MaboroutuFileWritableDataSource, SizeReflectsBufferedWrite) {
   temp_file file("abc");
   auto src = maboroutu::file_writable_data_source::open(
       file.path(), maboroutu::file_open_mode::existing);
   ASSERT_TRUE(src.has_value());
   std::array<std::byte, 2> data{std::byte{'d'}, std::byte{'e'}};
   ASSERT_TRUE(
       src->write(maboroutu::region{.offset = 3, .size = 2}, data).has_value());
   auto size_result = src->size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 5u);
   auto result = src->read(maboroutu::region{.offset = 0, .size = 5});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(as_text(*result), "abcde");
}

// 書き込み、読み込み、size()、書き込み、size() の順でも、正しい。
TEST(MaboroutuFileWritableDataSource, SizeAfterInterleavedOperations) {
   temp_file file("");
   auto src = maboroutu::file_writable_data_source::open(
       file.path(), maboroutu::file_open_mode::existing);
   ASSERT_TRUE(src.has_value());
   std::array<std::byte, 4> data{std::byte{'w'}, std::byte{'x'},
                                 std::byte{'y'}, std::byte{'z'}};
   ASSERT_TRUE(
       src->write(maboroutu::region{.offset = 0, .size = 4}, data).has_value());
   std::array<std::byte, 4> dst{};
   ASSERT_TRUE(
       src->read_into(maboroutu::region{.offset = 0, .size = 4}, dst)
           .has_value());
   EXPECT_EQ(std::memcmp(dst.data(), "wxyz", 4), 0);
   ASSERT_EQ(src->size().value_or(0), 4u);
   ASSERT_TRUE(
       src->write(maboroutu::region{.offset = 4, .size = 4}, data).has_value());
   ASSERT_EQ(src->size().value_or(0), 8u);
}

TEST(MaboroutuFileWritableDataSource, WriteZeroSizeSucceedsWithoutChange) {
   temp_file file("abc");
   auto src = maboroutu::file_writable_data_source::open(
       file.path(), maboroutu::file_open_mode::existing);
   ASSERT_TRUE(src.has_value());
   ASSERT_TRUE(src->write(maboroutu::region{.offset = 1, .size = 0}, {})
                   .has_value());
   auto result = src->read(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(as_text(*result), "abc");
}

TEST(MaboroutuFileWritableDataSource, WorksThroughWritableHandle) {
   temp_file file("hello world");
   auto opened = maboroutu::file_writable_data_source::open(
       file.path(), maboroutu::file_open_mode::existing);
   ASSERT_TRUE(opened.has_value());
   maboroutu::writable_data_source_handle handle(std::move(*opened));
   std::array<std::byte, 5> data{std::byte{'W'}, std::byte{'O'},
                                 std::byte{'R'}, std::byte{'L'},
                                 std::byte{'D'}};
   ASSERT_TRUE(
       handle.write(maboroutu::region{.offset = 6, .size = 5}, data)
           .has_value());
   auto result = handle.read(maboroutu::region{.offset = 0, .size = 11});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(as_text(*result), "hello WORLD");
}

// ファイルに書き込んだ内容は、閉じた後に、別の読み取りで確認できる。
TEST(MaboroutuFileWritableDataSource, WrittenDataIsPersistedAfterClose) {
   temp_file file("");
   {
      auto src = maboroutu::file_writable_data_source::open(
          file.path(), maboroutu::file_open_mode::truncate);
      ASSERT_TRUE(src.has_value());
      std::array<std::byte, 3> data{std::byte{'x'}, std::byte{'y'},
                                    std::byte{'z'}};
      ASSERT_TRUE(
          src->write(maboroutu::region{.offset = 0, .size = 3}, data)
              .has_value());
   }
   auto reader = maboroutu::file_data_source::open(file.path());
   ASSERT_TRUE(reader.has_value());
   auto result = reader->read(maboroutu::region{.offset = 0, .size = 3});
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(as_text(*result), "xyz");
}

} // namespace
