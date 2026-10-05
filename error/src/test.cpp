#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string_view>
import error_impl;

auto main() -> int {
  std::set_terminate([] {
    std::fputs("TERMINATE_HANDLER\n", stderr);
    std::_Exit(42);
  });
  std::signal(SIGABRT, [](int) {
    std::fputs("ABORT_SIGNAL\n", stderr);
    std::_Exit(43);
  });

  test_error(false);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
    test_error(true);
  } catch (std::out_of_range &e) {
    return std::string_view{e.what()} == "enter fatal." ? 0 : 1;
  } catch (...) {
    return 2; // 型が違う
  }
  return 3; // 送出されなかった
#else
  test_error(true);
  std::fputs("UNREACHABLE\n", stderr);
  return 4;
#endif
}
