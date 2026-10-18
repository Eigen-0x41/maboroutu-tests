// maboroutu.console のテスト。
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.console;

namespace {

using namespace std::string_view_literals;

TEST(MaboroutuConsole, RunDispatchesToRegisteredCommandWithSubspan) {
   maboroutu::command_handler::value_type commands;
   commands["hello"] = {
       .function = [](maboroutu::command_handler &,
                      maboroutu::command_handler::args_type args) {
          return static_cast<int>(args.size());
       },
       .description = "say hello",
   };

   std::vector<std::string_view> argv{"prog"sv, "hello"sv, "extra"sv};
   maboroutu::command_handler::args_type args(argv);
   maboroutu::command_handler handler(commands, args);

   auto result = handler.run(args);
   ASSERT_TRUE(result.has_value());
   // command_pos=1 ("hello") で発見され、args.subspan(1) = {"hello","extra"}
   // が渡される。
   EXPECT_EQ(*result, 2);
}

TEST(MaboroutuConsole, RunReturnsNulloptForUnknownCommand) {
   maboroutu::command_handler::value_type commands;
   std::vector<std::string_view> argv{"prog"sv, "unknown"sv};
   maboroutu::command_handler::args_type args(argv);
   maboroutu::command_handler handler(commands, args);

   EXPECT_FALSE(handler.run(args).has_value());
}

TEST(MaboroutuConsole, RunSkipsLeadingFlagArguments) {
   maboroutu::command_handler::value_type commands;
   bool called = false;
   commands["cmd"] = {
       .function = [&called](maboroutu::command_handler &,
                             maboroutu::command_handler::args_type) {
          called = true;
          return 0;
       },
       .description = "",
   };

   std::vector<std::string_view> argv{"prog"sv, "-v"sv, "cmd"sv};
   maboroutu::command_handler::args_type args(argv);
   maboroutu::command_handler handler(commands, args);

   auto result = handler.run(args);
   ASSERT_TRUE(result.has_value());
   EXPECT_TRUE(called);
}

TEST(MaboroutuConsole, MakeArgsWrapWrapsArgcArgv) {
   char prog[] = "prog";
   char arg1[] = "foo";
   char *argv[] = {prog, arg1};
   auto wrapped = maboroutu::make_args_wrap(2, argv);
   ASSERT_EQ(wrapped.size(), 2u);
   EXPECT_EQ(wrapped[0], "prog");
   EXPECT_EQ(wrapped[1], "foo");
}

TEST(MaboroutuConsole, FormatterListsCommandNamesAndFirstDescriptionLine) {
   maboroutu::command_handler::value_type commands;
   commands["hello"] = {
       .function = [](maboroutu::command_handler &,
                      maboroutu::command_handler::args_type) { return 0; },
       .description = "say hello\nsecond line",
   };

   std::vector<std::string_view> argv{"prog"sv};
   maboroutu::command_handler::args_type args(argv);
   maboroutu::command_handler handler(commands, args);

   auto const text = std::format("{}", handler);
   EXPECT_NE(text.find("commands"), std::string::npos);
   EXPECT_NE(text.find("hello : say hello"), std::string::npos);
   EXPECT_NE(text.find("second line"), std::string::npos);
}

} // namespace
