// maboroutu.result_format のテスト。
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

import maboroutu.result_format;
import maboroutu.core;
import maboroutu.error;
import maboroutu.binary_convert;
import maboroutu.memory_data_buffer;

namespace {

TEST(MaboroutuResultFormat, FormatsValueOfSuccessfulRead) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 4> raw{std::byte{0x78}, std::byte{0x56},
                                std::byte{0x34}, std::byte{0x12}};
   ASSERT_TRUE(buf.append(std::span<std::byte const>(raw)).has_value());

   auto const value =
       maboroutu::read_value<maboroutu::endian::little, std::uint32_t>(buf, 0);
   EXPECT_EQ(std::format("{}", maboroutu::show(value)), "305419896");
   // 書式指定は、値の型の書式へ渡される。
   EXPECT_EQ(std::format("{:#x}", maboroutu::show(value)), "0x12345678");
}

TEST(MaboroutuResultFormat, FormatsErrorMessageOfFailedRead) {
   maboroutu::memory_data_buffer buf;
   auto const value =
       maboroutu::read_value<maboroutu::endian::little, std::uint32_t>(buf, 0);
   ASSERT_FALSE(value.has_value());
   EXPECT_EQ(std::format("{}", maboroutu::show(value)),
             "error: data_source: region is out of range");
}

TEST(MaboroutuResultFormat, FormatsVoidResult) {
   maboroutu::memory_data_buffer buf;
   std::array<std::byte, 1> one{std::byte{1}};
   auto const ok = buf.append(std::span<std::byte const>(one));
   (void)ok;
   auto const written = maboroutu::write_value<maboroutu::endian::little>(
       buf, std::size_t{0}, std::uint8_t{7});
   EXPECT_EQ(std::format("{}", maboroutu::show(written)), "ok");
}

TEST(MaboroutuResultFormat, FormatsBytesAsHex) {
   std::array<std::byte, 4> raw{std::byte{0xde}, std::byte{0xad},
                                std::byte{0xbe}, std::byte{0xef}};
   EXPECT_EQ(std::format("{}", maboroutu::show(
                                   std::span<std::byte const>(raw))),
             "de ad be ef");
   EXPECT_EQ(std::format("{}", maboroutu::show(std::span<std::byte>(raw))),
             "de ad be ef");
   EXPECT_EQ(std::format("{}", maboroutu::show(std::span<std::byte const>{})),
             "");
}

} // namespace
