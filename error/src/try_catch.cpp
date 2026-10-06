#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string_view>
import error_impl;
import maboroutu.error;

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
  return maboroutu::invoke_or_recover(
      [] -> int {
        test_error(true);
        return EXIT_FAILURE;
      },
      [] -> int { return EXIT_SUCCESS; });
}
