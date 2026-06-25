#include "maboroutu/error.hpp"
#include "maboroutu/maboroutudef.hpp"
#include "maboroutu/unique_timer.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <random>

static std::random_device::result_type master_seed;
static std::array<std::random_device::result_type, 128> rand_table;
static size_t rand_table_index;

static void gen_rand_table() {
  std::random_device random_device{};
  master_seed = random_device();

#pragma unroll 1
  for (auto &entry : rand_table) {
    entry = random_device();
  }
  rand_table_index = 0;
}

static auto get_probability() -> double {

  decltype(rand_table_index) const current_index = rand_table_index;
  rand_table_index = ++rand_table_index % rand_table.size();
  auto &random_access_table =
      rand_table[rand_table[rand_table_index] & 0b1111111];
  random_access_table = std::byteswap(random_access_table);
  rand_table[current_index] +=
      rand_table[rand_table_index] % ((random_access_table % 65537) + 73483);
  return static_cast<double>(rand_table[current_index]) /
         (std::random_device::max)();
}
static auto is_probability_in(double rate) noexcept -> bool {
  return get_probability() <= rate;
}

static auto target_func(double rate) noexcept -> maboroutu::ret_type<void> {
  if (is_probability_in(rate)) {
    return maboroutu::make_exception_error<std::runtime_error>(
        "exception occerd.");
  }
  return {};
}

static auto target_src_func(double rate) noexcept -> maboroutu::ret_type<void> {
  if (is_probability_in(rate)) {
    return maboroutu::make_exception_error<std::runtime_error>(
        std::source_location::current(), "exception occerd.");
  }
  return {};
}

struct tag0 {};
struct tag1 {};
struct tag2 {};

using error_type = maboroutu::error_tag<tag0, tag1, tag2>;

static auto target_tag_func(double rate) noexcept
    -> std::expected<void, error_type> {
  if (is_probability_in(rate)) {
    if (is_probability_in(0.50)) {
      return error_type::make_unexpected(tag0(), "exception occerd.");
    }
    return error_type::make_unexpected(tag1(), "exception occerd.");
  }
  return {};
}

static auto speedtest(size_t trial_count, double rate) {
  std::optional<maboroutu::timer::rap_type> rap;
  size_t successed = 0;
  size_t exceptioned = 0;

  {
    maboroutu::timer timer;
#pragma unroll 1
    for (auto count = 0; count < trial_count; count++) {
      auto result = target_func(rate);
      if (result.has_value()) {
        successed++;
      } else {
        exceptioned++;
      }
    }
    rap.emplace(timer.rap());
  }

  auto result = target_src_func(1.0);
  assert(result.or_else([](std::exception const *value) -> decltype(result) {
    std::println("tag exception example\n"
                 "what    : {}\n",
                 value->what());
    return {};
  }));

  std::println("normal exception\n."
               "trial      : {}\n"
               "successed  : {}\n"
               "exeptioned : {}\n"
               "rate       : {}\n"
               "real rate  : {}\n"
               "all time...\n{}\n",
               trial_count, successed, exceptioned, rate,
               static_cast<double>(exceptioned) /
                   static_cast<double>(successed + exceptioned),
               *rap);
}

static auto speedsrctest(size_t trial_count, double rate) {
  std::optional<maboroutu::timer::rap_type> rap;
  size_t successed = 0;
  size_t exceptioned = 0;

  {
    maboroutu::timer timer;
#pragma unroll 1
    for (auto count = 0; count < trial_count; count++) {
      auto result = target_src_func(rate);
      if (result.has_value()) {
        successed++;
      } else {
        exceptioned++;
      }
    }
    rap.emplace(timer.rap());
  }

  auto result = target_src_func(1.0);
  assert(result.or_else([](std::exception const *value) -> decltype(result) {
    std::println("tag exception example\n"
                 "what    : {}\n",
                 value->what());
    return {};
  }));

  std::println("add src exception\n."
               "trial      : {}\n"
               "successed  : {}\n"
               "exeptioned : {}\n"
               "rate       : {}\n"
               "real rate  : {}\n"
               "all time...\n{}\n",
               trial_count, successed, exceptioned, rate,
               static_cast<double>(exceptioned) /
                   static_cast<double>(successed + exceptioned),
               *rap);
}

static auto speedtagtest(size_t trial_count, double rate) {
  std::optional<maboroutu::timer::rap_type> rap;
  size_t successed = 0;
  size_t exceptioned = 0;

  {
    maboroutu::timer timer;
#pragma unroll 1
    for (auto count = 0; count < trial_count; count++) {
      auto result = target_tag_func(rate);
      if (result.has_value()) {
        successed++;
      } else {
        exceptioned++;
      }
    }
    rap.emplace(timer.rap());
  }

  auto result = target_tag_func(1.0);
  assert(result.or_else([](error_type &value) -> decltype(result) {
    std::println("tag exception example\n"
                 "what    : {}\n"
                 "is tag0 : {}\n"
                 "is tag1 : {}\n"
                 "is tag2 : {}\n",
                 value.what(), value == tag0(), value == tag1(),
                 value == tag2());
    return {};
  }));

  std::println("tag exception\n."
               "trial      : {}\n"
               "successed  : {}\n"
               "exeptioned : {}\n"
               "rate       : {}\n"
               "real rate  : {}\n"
               "all time...\n{}\n",
               trial_count, successed, exceptioned, rate,
               static_cast<double>(exceptioned) /
                   static_cast<double>(successed + exceptioned),
               *rap);
}

auto main(int argc, char **argv) -> int {
#if !defined(NDEBUG)
  static constexpr const size_t trial_count = 1'000'000;
#else
  static constexpr const size_t trial_count = 100'000'000;
#endif /*!defined(NDEBUG) */
  gen_rand_table();

  std::println("error code:\n{}\n\n", target_func(1.0).error()->what());
  std::println("error code:\n{}\n\n", target_src_func(1.0).error()->what());

  speedtest(trial_count, 0.00);
  speedtest(trial_count, 0.01);
  speedtest(trial_count, 0.99);
  speedtest(trial_count, 1.00);

  speedsrctest(trial_count, 0.00);
  speedsrctest(trial_count, 0.01);
  speedsrctest(trial_count, 0.99);
  speedsrctest(trial_count, 1.00);

  speedtagtest(trial_count, 0.00);
  speedtagtest(trial_count, 0.01);
  speedtagtest(trial_count, 0.99);
  speedtagtest(trial_count, 1.00);

  auto res = error_type::make_unexpected<tag2>("exp message.").error();
  auto result = error0_type::make_unexpected(res, res.what());
  std::println("error code:\n{}\n\n", result.error().what());

  return EXIT_SUCCESS;
}
