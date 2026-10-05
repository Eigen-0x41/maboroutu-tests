module;
#include <cstdio>
#include <stdexcept>
export module error_impl;
import maboroutu.error;

export void test_error(bool enter) {
  std::fputs(enter ? "ENTER\n" : "SKIP\n", stderr);
  if (!enter)
    return;
  maboroutu::enter_fatal<std::out_of_range>("enter fatal.");
}
