// maboroutu.source_location_format のテスト。
#include <format>
#include <source_location>
#include <string>

#include <gtest/gtest.h>

import maboroutu.source_location_format;

namespace {

TEST(MaboroutuSourceLocationFormat, FormatsFileLineColumnAndFunctionName) {
   auto const loc = std::source_location::current();
   auto const formatted = std::format("{}", maboroutu::formatted_location{loc});

   EXPECT_EQ(formatted,
             std::string(loc.file_name()) + ": " + std::to_string(loc.line()) +
                 ", " + std::to_string(loc.column()) + ": " +
                 loc.function_name());
}

TEST(MaboroutuSourceLocationFormat, DefaultConstructionCapturesCallSite) {
   auto const a = std::format("{}", maboroutu::formatted_location{});
   auto const b = std::format("{}", maboroutu::formatted_location{});
   EXPECT_NE(a, b);
   EXPECT_NE(a.find(std::source_location::current().file_name()),
             std::string::npos);
}

} // namespace
