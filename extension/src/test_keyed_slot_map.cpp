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
#include <string>

#include <gtest/gtest.h>

import maboroutu.error;
import maboroutu.slot_map;
import maboroutu.keyed_slot_map;

namespace {

enum class idx_t : std::size_t {};
using underlying_map = maboroutu::slot_map<idx_t, int>;
using keyed_map = maboroutu::keyed_slot_map<std::string, underlying_map>;

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
             maboroutu::errc::keyed_slot_map::failed_to_add_key);
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
             maboroutu::errc::keyed_slot_map::key_was_not_contain);
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

} // namespace
