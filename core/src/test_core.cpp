// mylib.core (maboroutu.core) のテスト。
// library_spec.md 4.1節。
//
// core は「それ自体は機能を持たない型・concept・定数」のみを置くという
// 設計方針のため、コンパイル時静的検証（static_assert）が中心となる。
// GTestのTESTケース内にstatic_assertを置くことで、
//   - CTest上で「core」というテストスイート名の下に見える
//   - 将来 core.cppm 側にランタイムの挙動が追加された場合の受け皿になる
// という利点を持たせている。
#include <bit>
#include <cstddef>
#include <memory>
#include <type_traits>

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

} // namespace
