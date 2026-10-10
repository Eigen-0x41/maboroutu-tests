// mylib.keyed_slot_map (maboroutu.keyed_slot_map) のテスト。
//
// NOTE（検証結果・要確認）: library_spec.md には `mylib.keyed_slot_map` の
// コンポーネント節がまだ存在しない（8章 v1.24の未確定事項として
// 「独立モジュールとして提供する方針が決まっている」と記載されているのみ）。
// 会話ログ上の記憶（Claude側メモリ）では「モジュール名は
// mylib.keyed_slot_map に確定」とあるが、これは映幻とClaude間の合意事項
// であり、library_spec.md 自体にはまだ反映されていないと見られる。
// 本テストは実装（keyed_slot_map.cppm）のexport済みシグネチャを一次情報
// として作成した。
#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

import maboroutu.error;
import maboroutu.slot_map;
import maboroutu.keyed_slot_map;

namespace {

enum class idx_t : std::size_t {};
using underlying_map = maboroutu::slot_map<idx_t, int>;
using keyed_map = maboroutu::keyed_slot_map<std::string, underlying_map>;
// errc::keyed_slot_map は非exportのため、名前を綴らずに型から導出する。
using code_type = decltype(std::declval<maboroutu::keyed_slot_map_result<int>>().error().code());

TEST(MaboroutuKeyedSlotMap, RoutedEmplaceThenContainsAndRoutedAt) {
   underlying_map slots;
   keyed_map keyed(slots);

   auto result = keyed.routed_emplace(std::string("alpha"), 42);
   ASSERT_TRUE(result.has_value());

   EXPECT_TRUE(keyed.contains(std::string("alpha")));
   EXPECT_EQ(keyed.routed_at(std::string("alpha")), 42);
}

TEST(MaboroutuKeyedSlotMap, RoutedEmplaceWithDuplicateKeyFails) {
   underlying_map slots;
   keyed_map keyed(slots);

   ASSERT_TRUE(keyed.routed_emplace(std::string("alpha"), 1).has_value());
   auto second = keyed.routed_emplace(std::string("alpha"), 2);
   ASSERT_FALSE(second.has_value());
   EXPECT_EQ(second.error().code(),
             code_type::failed_to_add_key);
   // 最初に格納した値は変化しない。
   EXPECT_EQ(keyed.routed_at(std::string("alpha")), 1);
}

TEST(MaboroutuKeyedSlotMap, RequireRoutedExistDistinguishesMissingKey) {
   underlying_map slots;
   keyed_map keyed(slots);
   ASSERT_TRUE(keyed.routed_emplace(std::string("alpha"), 1).has_value());

   EXPECT_TRUE(keyed.require_routed_exist(std::string("alpha")).has_value());

   auto missing = keyed.require_routed_exist(std::string("beta"));
   ASSERT_FALSE(missing.has_value());
   EXPECT_EQ(missing.error().code(),
             code_type::key_was_not_contain);
}

TEST(MaboroutuKeyedSlotMap, RoutedAtWithMissingKeyThrowsOutOfRange) {
   underlying_map slots;
   keyed_map keyed(slots);
   ASSERT_TRUE(keyed.routed_emplace(std::string("alpha"), 1).has_value());

   EXPECT_THROW(static_cast<void>(keyed.routed_at(std::string("beta"))),
                std::out_of_range);
}

TEST(MaboroutuKeyedSlotMap, RoutedEraseRemovesKeyAndUnderlyingSlot) {
   underlying_map slots;
   keyed_map keyed(slots);
   ASSERT_TRUE(keyed.routed_emplace(std::string("alpha"), 1).has_value());

   EXPECT_TRUE(keyed.routed_erase(std::string("alpha")));
   EXPECT_FALSE(keyed.contains(std::string("alpha")));
   // 存在しないキーへのerase()はfalseを返す（例外を投げない）。
   EXPECT_FALSE(keyed.routed_erase(std::string("alpha")));
}

TEST(MaboroutuKeyedSlotMap, RoutedAccessInvokesFunctionWithUnderlyingSlotMap) {
   underlying_map slots;
   keyed_map keyed(slots);
   ASSERT_TRUE(keyed.routed_emplace(std::string("alpha"), 10).has_value());

   auto doubled = keyed.routed_access(
       [](underlying_map &map, idx_t index) { return map[index] * 2; },
       std::string("alpha"));
   EXPECT_EQ(doubled, 20);
}

TEST(MaboroutuKeyedSlotMap, ConstOverloadsAreReadOnly) {
   underlying_map slots;
   keyed_map keyed(slots);
   ASSERT_TRUE(keyed.routed_emplace(std::string("alpha"), 5).has_value());

   keyed_map const &const_keyed = keyed;
   EXPECT_EQ(const_keyed.routed_at(std::string("alpha")), 5);
   EXPECT_TRUE(const_keyed.contains(std::string("alpha")));
}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)

struct throwing_value {
   explicit throwing_value(bool fail) {
      if (fail) {
         throw std::runtime_error("constructor failed");
      }
   }
};

using throwing_map = maboroutu::slot_map<idx_t, throwing_value>;
using throwing_keyed_map =
    maboroutu::keyed_slot_map<std::string, throwing_map>;
using throwing_code_type =
    maboroutu::keyed_slot_map_result<void>::error_type::code_type;

TEST(MaboroutuKeyedSlotMap, RoutedEmplaceRecoversWhenConstructorThrows) {
   throwing_map slots;
   throwing_keyed_map keyed(slots);

   auto failed = keyed.routed_emplace(std::string("alpha"), true);
   ASSERT_FALSE(failed.has_value());
   EXPECT_EQ(failed.error().code(), throwing_code_type::failed_to_constructed);

   // keyの登録が取り消されている。
   EXPECT_FALSE(keyed.contains(std::string("alpha")));
   // checkout()した予約がcancel()でフリーリストへ戻っている。
   EXPECT_EQ(slots.size(), 0U);
   EXPECT_EQ(slots.free_size(), 1U);

   // 同じkeyで再登録でき、戻したスロットが再利用される。
   auto retry = keyed.routed_emplace(std::string("alpha"), false);
   ASSERT_TRUE(retry.has_value());
   EXPECT_TRUE(keyed.contains(std::string("alpha")));
   EXPECT_EQ(slots.size(), 1U);
   EXPECT_EQ(slots.free_size(), 0U);
}

#endif

} // namespace
