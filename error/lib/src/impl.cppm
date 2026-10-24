module;
#include <cstdio>
#include <stdexcept>
#include <string>
export module error_impl;
import maboroutu.error;

export void test_error(bool enter) {
  std::fputs(enter ? "ENTER\n" : "SKIP\n", stderr);
  if (!enter)
    return;
  maboroutu::enter_fatal<std::out_of_range>("enter fatal.");
}

// 実行時に組み立てた文字列は、従来どおり（診断表示には出力されない）
// 引数として渡せることの確認（呼び出しはしない）。
[[maybe_unused]] void compile_check_runtime_message(std::string const &s) {
  if (s.empty())
    maboroutu::enter_fatal<std::out_of_range>(s);
}
