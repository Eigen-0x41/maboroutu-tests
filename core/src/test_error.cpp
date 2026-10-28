// mylib.error (maboroutu.error) のテスト。
//
// error_detail / basic_error はモジュール内部型（exportされていない）ため、
// 外部から参照可能なのは error<Code,Detail,DetailDeleter> / result<...> /
// resultable / make_unexpected のみである。
#include <expected>
#include <memory>
#include <type_traits>

#include <string_view>
#include <gtest/gtest.h>

import maboroutu.error;

namespace {

enum class my_errc { a, b, c };

// ---------------------------------------------------------------------
// サイズ保証（EBOにより Detail=monostate の場合はenumと同サイズになる）
// ---------------------------------------------------------------------
static_assert(sizeof(maboroutu::error<my_errc>) == sizeof(my_errc));
static_assert(sizeof(maboroutu::error<my_errc, int>) != sizeof(my_errc));

// ---------------------------------------------------------------------
// ムーブオンリー（コピー構築・コピー代入は不可）
// ---------------------------------------------------------------------
static_assert(!std::is_copy_constructible_v<maboroutu::error<my_errc>>);
static_assert(std::is_move_constructible_v<maboroutu::error<my_errc>>);
static_assert(!std::is_copy_assignable_v<maboroutu::error<my_errc>>);
static_assert(std::is_move_assignable_v<maboroutu::error<my_errc>>);

// ---------------------------------------------------------------------
// result<T, Code, Detail, DetailDeleter> == std::expected<T, error<...>>
// ---------------------------------------------------------------------
static_assert(std::is_same_v<maboroutu::result<int, my_errc>,
                             std::expected<int, maboroutu::error<my_errc>>>);

TEST(MaboroutuError, CodeReturnsConstructedValue) {
   maboroutu::error<my_errc> e(my_errc::b);
   EXPECT_EQ(e.code(), my_errc::b);
   EXPECT_TRUE(e == my_errc::b);
   EXPECT_FALSE(e == my_errc::a);
}

TEST(MaboroutuError, GetIfDetailIsNullWhenDetailIsMonostate) {
   maboroutu::error<my_errc> e(my_errc::a);
   EXPECT_EQ(e.get_if_detail(), nullptr);
}

TEST(MaboroutuError, DetailIsHeapAllocatedAndRetrievable) {
   maboroutu::error<my_errc, int> e(my_errc::c, std::make_unique<int>(42));
   EXPECT_EQ(e.code(), my_errc::c);
   ASSERT_NE(e.get_if_detail(), nullptr);
   EXPECT_EQ(*e.get_if_detail(), 42);
}

TEST(MaboroutuError, DefaultConstructedDetailIsNullptr) {
   // error<Code, Detail> は Detail 用の引数を渡さない限り、内部の
   // unique_ptr<Detail> は nullptr のままである（error_detail() ctor 参照）。
   maboroutu::error<my_errc, int> e(my_errc::a);
   EXPECT_EQ(e.get_if_detail(), nullptr);
}

TEST(MaboroutuError, ResultCanHoldSuccessValue) {
   maboroutu::result<int, my_errc> r = 10;
   ASSERT_TRUE(r.has_value());
   EXPECT_EQ(*r, 10);
}

TEST(MaboroutuError, MakeUnexpectedProducesFailureResult) {
   maboroutu::result<int, my_errc> r = maboroutu::make_unexpected(my_errc::a);
   ASSERT_FALSE(r.has_value());
   EXPECT_EQ(r.error().code(), my_errc::a);
   EXPECT_EQ(r.error().get_if_detail(), nullptr);
}

TEST(MaboroutuError, MakeUnexpectedWithCustomDetailAndDeleter) {
   struct no_op_deleter {
      auto operator()(int *) const noexcept -> void {
         // operator delete が使えない環境を想定した動作確認用の
         // no-op deleter（error.cppmの静的検証コード例に倣う）。
      }
   };
   int value = 7;
   auto u = maboroutu::make_unexpected<my_errc, int, no_op_deleter>(
       my_errc::b, &value);
   static_assert(
       std::is_same_v<decltype(u),
                      std::unexpected<maboroutu::error<my_errc, int, no_op_deleter>>>);
   EXPECT_EQ(u.error().code(), my_errc::b);
   ASSERT_NE(u.error().get_if_detail(), nullptr);
   EXPECT_EQ(*u.error().get_if_detail(), 7);
}

TEST(MaboroutuError, ResultableHoldsOnlyForVoidValueType) {
   // resultable<T, Code, Detail, DetailDeleter> は
   // result<void,...> と result<T,...> が同一の型である場合にのみ真になる、
   // すなわち T = void の場合にのみ成立する（4.1節コード参照）。
   static_assert(
       maboroutu::resultable<void, my_errc, std::monostate,
                             std::default_delete<std::monostate>>);
   static_assert(
       !maboroutu::resultable<int, my_errc, std::monostate,
                              std::default_delete<std::monostate>>);
}

} // namespace

// ---------------------------------------------------------------------
// error_message / message()
// ---------------------------------------------------------------------
namespace message_test {
enum class with_message { first, second };
enum class without_message { only };

constexpr auto error_message(with_message code) noexcept
    -> std::string_view {
   switch (code) {
   case with_message::first:
      return "first";
   case with_message::second:
      return "second";
   }
   return "unknown error";
}

static_assert(maboroutu::has_error_message<with_message>);
static_assert(!maboroutu::has_error_message<without_message>);

template <class E>
concept has_message = requires(E const &e) { e.message(); };
static_assert(has_message<maboroutu::error<with_message>>);
static_assert(has_message<maboroutu::error<with_message, int>>);
static_assert(!has_message<maboroutu::error<without_message>>);
static_assert(noexcept(std::declval<maboroutu::error<with_message> const &>()
                           .message()));
static_assert(maboroutu::error<with_message>(with_message::second).message() ==
              "second");
} // namespace message_test

TEST(MaboroutuError, MessageReturnsDescriptionOfCode) {
   maboroutu::error<message_test::with_message> e(
       message_test::with_message::first);
   EXPECT_EQ(e.message(), "first");
}

TEST(MaboroutuError, MessageIgnoresDetail) {
   maboroutu::error<message_test::with_message, int> e(
       message_test::with_message::second, std::make_unique<int>(1));
   EXPECT_EQ(e.message(), "second");
}

TEST(MaboroutuError, MessageForUnlistedValueFallsBack) {
   maboroutu::error<message_test::with_message> e(
       static_cast<message_test::with_message>(99));
   EXPECT_EQ(e.message(), "unknown error");
}
