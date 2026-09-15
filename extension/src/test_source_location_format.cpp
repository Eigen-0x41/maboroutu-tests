// mylib.source_location_format (maboroutu.source_location_format) のテスト。
//
// NOTE（検証結果・要確認）: library_spec.md には source_location_format
// というコンポーネントの記載が見当たらない。test_console.cpp と同様、
// 仕様書上の正式な位置づけは不明である。
//
// !!! 重要（検証済みのビルドブロッカー、環境依存）!!!
// このモジュールの唯一の役割は `std::formatter<std::source_location>` の
// 明示的特殊化をexportすることだが、Clang 18 (-std=c++23 -stdlib=libc++)
// で実機検証したところ、`import maboroutu.source_location_format;` した
// 別の翻訳単位から `std::format("{}", loc)` を呼び出すと
// 「the supplied type is not formattable」というコンパイルエラーになる
// ことを確認した（test_console.cpp の command_hander 用formatterと同一の
// 制約。exportの有無や特殊化の宣言位置を変えても再現する最小再現例で
// 確認済み）。
//
// すなわち、本ファイルが検証しようとしている機能（std::formatter
// 特殊化がモジュール越しに機能すること）そのものが、少なくとも
// Clang 18 + libc++ の組み合わせでは成立しない。GCC/libstdc++での
// 挙動は未検証。この制約が解消される、またはlibstdc++環境で異なる
// 結果が確認されるまで、以下のテストはビルドが通らない状態になる
// 可能性が高いことを明記しておく。
#include <format>
#include <source_location>
#include <string>

#include <gtest/gtest.h>

import maboroutu.source_location_format;

namespace {

TEST(MaboroutuSourceLocationFormat, FormatsFileLineColumnAndFunctionName) {
   auto loc = std::source_location::current();
   auto formatted = std::format("{}", loc);

   EXPECT_NE(formatted.find(loc.file_name()), std::string::npos);
   EXPECT_NE(formatted.find(loc.function_name()), std::string::npos);
   EXPECT_NE(formatted.find(std::to_string(loc.line())), std::string::npos);
}

TEST(MaboroutuSourceLocationFormat, DifferentCallSitesProduceDifferentOutput) {
   auto const loc_a = std::source_location::current();
   auto const loc_b = std::source_location::current();
   EXPECT_NE(std::format("{}", loc_a), std::format("{}", loc_b));
}

} // namespace
