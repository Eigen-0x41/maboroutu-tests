// mylib.console (maboroutu.console) のテスト。
//
// NOTE（検証結果・要確認）: library_spec.md には console
// というコンポーネントの記載が見当たらない。アップロードされた実装には
// core/src ではなく extension/src 配下の maboroutu.console として実在する
// ため、本テストは実装のexport済みシグネチャを一次情報として作成したが、
// 仕様書上の正式な位置づけ（coreかextensionか、あるいは仕様外のツール
// 実装か）は不明である。
//
// NOTE（検証済みの環境依存の制約）: console.cppm はexport moduleの中で
// `export template <> struct std::formatter<command_hander> ...`
// という std::formatter の明示的特殊化をexportしている。Clang 18
// (-std=c++23 -stdlib=libc++) で実機検証したところ、この特殊化は
// `import maboroutu.console;` した別の翻訳単位からは実際には到達不能で
// あり、`std::format("{}", handler)` を呼び出すと
// 「the supplied type is not formattable」というコンパイルエラーになる
// ことを確認した（`export` の有無・特殊化を module purview 内か
// グローバルモジュールフラグメント直後かに関わらず再現。最小再現例で
// 確認済み）。これはstd::formatter特殊化のモジュール越しの到達可能性に
// 関するClang/libc++側の制約である可能性が高く、GCC/libstdc++でも
// 同様の制約があるかは未検証。この制約が解消されるまで、本ファイルでは
// std::format(handler)を用いた検証を行わない（run()の動作検証のみ行う）。
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.console;

namespace {

using namespace std::string_view_literals;

TEST(MaboroutuConsole, RunDispatchesToRegisteredCommandWithSubspan) {
   maboroutu::command_hander::value_type commands;
   commands["hello"] = {
       .function = [](maboroutu::command_hander &,
                      maboroutu::command_hander::args_type args) {
          return static_cast<int>(args.size());
       },
       .description = "say hello",
   };

   std::vector<std::string_view> argv{"prog"sv, "hello"sv, "extra"sv};
   maboroutu::command_hander::args_type args(argv);
   maboroutu::command_hander handler(commands, args);

   auto result = handler.run(args);
   ASSERT_TRUE(result.has_value());
   // command_pos=1 ("hello") で発見され、args.subspan(1) = {"hello","extra"}
   // が渡される。
   EXPECT_EQ(*result, 2);
}

TEST(MaboroutuConsole, RunReturnsNulloptForUnknownCommand) {
   maboroutu::command_hander::value_type commands;
   std::vector<std::string_view> argv{"prog"sv, "unknown"sv};
   maboroutu::command_hander::args_type args(argv);
   maboroutu::command_hander handler(commands, args);

   EXPECT_FALSE(handler.run(args).has_value());
}

TEST(MaboroutuConsole, RunSkipsLeadingFlagArguments) {
   maboroutu::command_hander::value_type commands;
   bool called = false;
   commands["cmd"] = {
       .function = [&called](maboroutu::command_hander &,
                             maboroutu::command_hander::args_type) {
          called = true;
          return 0;
       },
       .description = "",
   };

   std::vector<std::string_view> argv{"prog"sv, "-v"sv, "cmd"sv};
   maboroutu::command_hander::args_type args(argv);
   maboroutu::command_hander handler(commands, args);

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

} // namespace
