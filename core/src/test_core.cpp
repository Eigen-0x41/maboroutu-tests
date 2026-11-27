// mylib.core (maboroutu.core) のテスト。
//
// core は「それ自体は機能を持たない型・concept・定数」のみを置くという
// 設計方針のため、コンパイル時静的検証（static_assert）が中心となる。
// GTestのTESTケース内にstatic_assertを置くことで、
//   - CTest上で「core」というテストスイート名の下に見える
//   - 将来 core.cppm 側にランタイムの挙動が追加された場合の受け皿になる
// という利点を持たせている。
#include <bit>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

import maboroutu.core;

namespace {

// ---------------------------------------------------------------------
// basic_region / region (4.1.2節)
// ---------------------------------------------------------------------
TEST(MaboroutuCore, RegionIsAggregateWithOffsetAndSize) {
   maboroutu::region r{.offset = 10, .size = 20};
   EXPECT_EQ(r.offset, 10u);
   EXPECT_EQ(r.size, 20u);

   // region は basic_region<std::size_t> のエイリアスである。
   static_assert(std::is_same_v<maboroutu::region,
                                maboroutu::basic_region<std::size_t>>);

   // 他の整数型でも basic_region を直接使用できる。
   maboroutu::basic_region<std::uint32_t> r32{.offset = 1, .size = 2};
   EXPECT_EQ(r32.offset, 1u);
   EXPECT_EQ(r32.size, 2u);
}

// ---------------------------------------------------------------------
// dynamic_array / byte_array (4.1.3節)
// ---------------------------------------------------------------------
TEST(MaboroutuCore, ByteArrayOwnsExactlyRequestedSize) {
   constexpr std::size_t n = 16;
   maboroutu::byte_array arr{
       .value = std::make_unique<maboroutu::byte_array::value_type>(n),
       .size = n,
   };
   ASSERT_NE(arr.value.get(), nullptr);
   EXPECT_EQ(arr.size, n);

   // 書き込み・読み出しができること（所有権付きバッファとしての最小契約）。
   for (std::size_t i = 0; i < n; ++i) {
      arr.value[i] = static_cast<std::byte>(i);
   }
   for (std::size_t i = 0; i < n; ++i) {
      EXPECT_EQ(arr.value[i], static_cast<std::byte>(i));
   }

   static_assert(
       std::is_same_v<maboroutu::byte_array,
                      maboroutu::dynamic_array<std::byte>>);
}

TEST(MaboroutuCore, DynamicArrayWorksForNonByteElementType) {
   // dynamic_array<T> は byte_array 専用ではなく、任意の要素型に使える。
   maboroutu::dynamic_array<int> arr{
       .value = std::make_unique<maboroutu::dynamic_array<int>::value_type>(4),
       .size = 4,
   };
   arr.value[0] = 42;
   EXPECT_EQ(arr.value[0], 42);
   EXPECT_EQ(arr.size, 4u);
}

// ---------------------------------------------------------------------
// one_of / contains_duplicate (4.1.4節)
// ---------------------------------------------------------------------
static_assert(maboroutu::one_of<int, int, double>);
static_assert(maboroutu::one_of<double, int, double>);
static_assert(!maboroutu::one_of<char, int, double>);
// ArgsT... が空の場合、畳み込み式 (std::same_as<T, ArgsT> || ...) は
// || の単位元である false に評価される（一致対象が無いため偽）。
static_assert(!maboroutu::one_of<int>);

static_assert(!maboroutu::contains_duplicate<int, double, char>);
static_assert(maboroutu::contains_duplicate<int, double, int>);
static_assert(!maboroutu::contains_duplicate<>); // 空リストは重複なし扱い

TEST(MaboroutuCore, OneOfAndContainsDuplicateAreCompileTimeOnly) {
   // one_of / contains_duplicate は concept であり実行時の値を持たないため、
   // このTESTケース自体は「上記static_assert群がこの翻訳単位で
   // 評価されたこと」を示すためのプレースホルダーとして存在する。
   SUCCEED();
}

// ---------------------------------------------------------------------
// in_place_tag / in_place_tag_v / endian_in_place_tag (4.1.5節)
// ---------------------------------------------------------------------
enum class sample_kind { alpha, beta };

TEST(MaboroutuCore, InPlaceTagCarriesStaticEnumValue) {
   using alpha_tag = maboroutu::in_place_tag<sample_kind, sample_kind::alpha>;
   static_assert(std::is_same_v<alpha_tag::value_type, sample_kind>);
   static_assert(alpha_tag::value == sample_kind::alpha);

   alpha_tag tag{};
   EXPECT_EQ(decltype(tag)::value, sample_kind::alpha);

   // コピー・ムーブ構築は可能、代入は delete されている
   // (4.1.5節コード例参照)。
   static_assert(std::is_copy_constructible_v<alpha_tag>);
   static_assert(std::is_move_constructible_v<alpha_tag>);
   static_assert(!std::is_copy_assignable_v<alpha_tag>);
   static_assert(!std::is_move_assignable_v<alpha_tag>);
}

TEST(MaboroutuCore, InPlaceTagVSelectsOverloadStatically) {
   // in_place_tag_v<EnumV> を使った静的ディスパッチの典型例。
   struct dispatcher {
      static auto call(maboroutu::in_place_tag<sample_kind,
                                               sample_kind::alpha>) -> int {
         return 1;
      }
      static auto call(maboroutu::in_place_tag<sample_kind, sample_kind::beta>)
          -> int {
         return 2;
      }
   };
   EXPECT_EQ(dispatcher::call(
                 maboroutu::in_place_tag_v<sample_kind::alpha>),
             1);
   EXPECT_EQ(dispatcher::call(maboroutu::in_place_tag_v<sample_kind::beta>),
             2);
}

TEST(MaboroutuCore, EndianInPlaceTagIsInPlaceTagOfStdEndian) {
   using little_tag = maboroutu::endian_in_place_tag<std::endian::little>;
   static_assert(
       std::is_same_v<little_tag,
                      maboroutu::in_place_tag<std::endian, std::endian::little>>);
   static_assert(little_tag::value == std::endian::little);
}

// ---------------------------------------------------------------------
// invoke_within_read_lock_or_directly / invoke_within_write_lock_or_directly
// ---------------------------------------------------------------------
// read_lock/write_lock を持つ型（ロックの内側で func が呼ばれることを検証する）。
struct lockable_value {
   using value_type = int;
   int value = 0;
   int read_count = 0;
   int write_count = 0;
   bool locked = false;

   template <class F>
   constexpr auto read_lock(F &&func) noexcept(
       std::is_nothrow_invocable_v<F &&, int const &>)
       -> std::invoke_result_t<F &&, int const &> {
      ++read_count;
      locked = true;
      struct release {
         bool &flag;
         ~release() { flag = false; }
      } guard{locked};
      return std::invoke(std::forward<F>(func), std::as_const(value));
   }
   template <class F>
   constexpr auto write_lock(F &&func) noexcept(
       std::is_nothrow_invocable_v<F &&, int &>)
       -> std::invoke_result_t<F &&, int &> {
      ++write_count;
      locked = true;
      struct release {
         bool &flag;
         ~release() { flag = false; }
      } guard{locked};
      return std::invoke(std::forward<F>(func), value);
   }
};

struct plain_value {
   int value = 0;
};

TEST(MaboroutuCore, InvokeWithinReadLockCallsInsideLock) {
   lockable_value v;
   bool was_locked = false;
   maboroutu::invoke_within_read_lock_or_directly(v, [&](int const &) {
      was_locked = v.locked;
   });
   EXPECT_TRUE(was_locked);
   EXPECT_FALSE(v.locked);
   EXPECT_EQ(v.read_count, 1);
   EXPECT_EQ(v.write_count, 0);
}

TEST(MaboroutuCore, InvokeWithinWriteLockCallsInsideLock) {
   lockable_value v;
   bool was_locked = false;
   maboroutu::invoke_within_write_lock_or_directly(v, [&](int &x) {
      was_locked = v.locked;
      x = 3;
   });
   EXPECT_TRUE(was_locked);
   EXPECT_FALSE(v.locked);
   EXPECT_EQ(v.write_count, 1);
   EXPECT_EQ(v.read_count, 0);
   EXPECT_EQ(v.value, 3);
}

TEST(MaboroutuCore, InvokeDirectlyWhenNotLockable) {
   plain_value p;
   maboroutu::invoke_within_write_lock_or_directly(
       p, [](plain_value &x) { x.value = 4; });
   EXPECT_EQ(p.value, 4);
   EXPECT_EQ(maboroutu::invoke_within_read_lock_or_directly(
                 p, [](plain_value const &x) { return x.value; }),
             4);
}

// std::invoke の規則に従い、メンバポインタも渡せる。
TEST(MaboroutuCore, InvokeDirectlyAcceptsMemberPointer) {
   plain_value p{6};
   EXPECT_EQ(maboroutu::invoke_within_read_lock_or_directly(
                 p, &plain_value::value),
             6);
   lockable_value l;
   l.value = 8;
   EXPECT_EQ(maboroutu::invoke_within_read_lock_or_directly(
                 l, [](int const &v) { return v; }),
             8);
}

// const の対象には書き込み版を使えない。
template <class T, class F>
concept can_write_or_directly = requires(T &v, F &&f) {
   maboroutu::invoke_within_write_lock_or_directly(v, std::forward<F>(f));
};
static_assert(can_write_or_directly<plain_value, void (*)(plain_value &)>);
static_assert(!can_write_or_directly<plain_value const, void (*)(plain_value &)>);
static_assert(can_write_or_directly<lockable_value, void (*)(int &)>);

TEST(MaboroutuCore, InvokeWithinLockReturnsValueAndReference) {
   lockable_value l;
   plain_value p;
   int &r = maboroutu::invoke_within_write_lock_or_directly(
       l, [](int &x) -> int & { return x; });
   EXPECT_EQ(&r, &l.value);
   int &q = maboroutu::invoke_within_write_lock_or_directly(
       p, [](plain_value &x) -> int & { return x.value; });
   EXPECT_EQ(&q, &p.value);
   EXPECT_EQ(maboroutu::invoke_within_read_lock_or_directly(
                 l, [](int const &x) { return x + 1; }),
             1);
}

TEST(MaboroutuCore, InvokeWithinLockNoexceptPropagation) {
   lockable_value l;
   plain_value p;
   auto nr = [](int const &) noexcept {};
   auto mr = [](int const &) {};
   auto nw = [](plain_value &) noexcept {};
   auto mw = [](plain_value &) {};
   static_assert(noexcept(maboroutu::invoke_within_read_lock_or_directly(l, nr)));
   static_assert(!noexcept(maboroutu::invoke_within_read_lock_or_directly(l, mr)));
   static_assert(noexcept(maboroutu::invoke_within_write_lock_or_directly(p, nw)));
   static_assert(!noexcept(maboroutu::invoke_within_write_lock_or_directly(p, mw)));
}

} // namespace
