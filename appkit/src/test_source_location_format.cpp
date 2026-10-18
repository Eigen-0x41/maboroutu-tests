// maboroutu.source_location_format のテスト。
#include <format>
#include <source_location>
#include <string>

#include <gtest/gtest.h>

import maboroutu.source_location_format;

namespace {

// 数値を、幅3で右寄せした文字列にする（空白埋め）。
std::string pad3(unsigned n) {
   auto s = std::to_string(n);
   return std::string(s.size() < 3 ? 3 - s.size() : 0, ' ') + s;
}

TEST(MaboroutuSourceLocationFormat, FormatsFileLineColumnAndFunctionName) {
   auto const loc = std::source_location::current();
   auto const formatted = std::format("{}", maboroutu::formatted_location{loc});

   EXPECT_EQ(formatted, std::string(loc.file_name()) + ": " + pad3(loc.line()) +
                            ", " + pad3(loc.column()) + ": " +
                            loc.function_name());
}

TEST(MaboroutuSourceLocationFormat, LineAndColumnAreRightAlignedToWidthThree) {
   // この呼び出し位置の行番号・列番号は3桁未満である前提（ファイル冒頭付近）。
   auto const loc = std::source_location::current();
   ASSERT_LT(loc.line(), 100u);
   ASSERT_LT(loc.column(), 100u);
   auto const formatted = std::format("{}", maboroutu::formatted_location{loc});
   // 3桁未満の値は、先頭が空白で埋められる。
   EXPECT_NE(formatted.find(": " + pad3(loc.line()) + ", " + pad3(loc.column()) +
                            ": "),
             std::string::npos);
   EXPECT_EQ(pad3(loc.line()).front(), ' ');
}

TEST(MaboroutuSourceLocationFormat, DefaultConstructionCapturesCallSite) {
   auto const a = std::format("{}", maboroutu::formatted_location{});
   auto const b = std::format("{}", maboroutu::formatted_location{});
   EXPECT_NE(a, b);
   EXPECT_NE(a.find(std::source_location::current().file_name()),
             std::string::npos);
}

} // namespace
