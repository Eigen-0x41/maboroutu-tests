// mylib.extension_store (maboroutu.extension_store) のテスト。
#include <type_traits>

#include <gtest/gtest.h>

import maboroutu.extension_store;

namespace {

struct alpha_info {
   int value = 0;
   auto operator==(alpha_info const &) const -> bool = default;
};
struct beta_info {
   int value = 0;
   auto operator==(beta_info const &) const -> bool = default;
};

// overloaded パターン（ライブラリ非提供、アプリ側で定義する前例）を
// テストでも使用する。
template <class... Ts> struct overloaded : Ts... {
   using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

using store_type = maboroutu::extension_store<alpha_info, beta_info>;

TEST(MaboroutuExtensionStore, GetIfReturnsNullptrWhenUnset) {
   store_type store;
   EXPECT_EQ(store.get_if<alpha_info>(), nullptr);
   EXPECT_EQ(store.get_if<beta_info>(), nullptr);
}

TEST(MaboroutuExtensionStore, SetThenGetIfReturnsStoredValue) {
   store_type store;
   store.set<alpha_info>(alpha_info{.value = 1});
   ASSERT_NE(store.get_if<alpha_info>(), nullptr);
   EXPECT_EQ(store.get_if<alpha_info>()->value, 1);
   // 未設定の型は影響を受けない。
   EXPECT_EQ(store.get_if<beta_info>(), nullptr);
}

TEST(MaboroutuExtensionStore, EraseRemovesOnlyTheSpecifiedType) {
   store_type store;
   store.set<alpha_info>(alpha_info{.value = 1});
   store.set<beta_info>(beta_info{.value = 2});
   store.erase<alpha_info>();
   EXPECT_EQ(store.get_if<alpha_info>(), nullptr);
   ASSERT_NE(store.get_if<beta_info>(), nullptr);
   EXPECT_EQ(store.get_if<beta_info>()->value, 2);
}

TEST(MaboroutuExtensionStore, VisitEachMutatesSetSlotsOnly) {
   store_type store;
   store.set<alpha_info>(alpha_info{.value = 1});
   // beta_info は未設定のまま。
   store.visit_each(overloaded{
       [](alpha_info &a) { a.value += 10; },
       [](beta_info &b) { b.value += 10; },
   });
   ASSERT_NE(store.get_if<alpha_info>(), nullptr);
   EXPECT_EQ(store.get_if<alpha_info>()->value, 11);
   // 未設定スロットは走査対象から自動的に除外される
   EXPECT_EQ(store.get_if<beta_info>(), nullptr);
}

TEST(MaboroutuExtensionStore, ConstVisitEachIsReadOnly) {
   store_type store;
   store.set<alpha_info>(alpha_info{.value = 5});
   store.set<beta_info>(beta_info{.value = 7});

   store_type const &const_store = store;
   int sum = 0;
   const_store.visit_each(overloaded{
       [&sum](alpha_info const &a) { sum += a.value; },
       [&sum](beta_info const &b) { sum += b.value; },
   });
   EXPECT_EQ(sum, 12);
   // 読み取り専用であり、元の値は変化しない。
   EXPECT_EQ(store.get_if<alpha_info>()->value, 5);
}

TEST(MaboroutuExtensionStore, TransformEachProducesIndependentCopy) {
   store_type store;
   store.set<alpha_info>(alpha_info{.value = 3});
   // beta_info は未設定のまま。

   store_type const &const_store = store;
   auto transformed = const_store.transform_each(overloaded{
       [](alpha_info const &a) { return alpha_info{.value = a.value * 10}; },
       [](beta_info const &b) { return beta_info{.value = b.value * 10}; },
   });

   ASSERT_NE(transformed.get_if<alpha_info>(), nullptr);
   EXPECT_EQ(transformed.get_if<alpha_info>()->value, 30);
   // 未設定だったスロットは変換後も未設定のまま
   EXPECT_EQ(transformed.get_if<beta_info>(), nullptr);

   // 元のstoreは変更されない（複製であることの確認）。
   EXPECT_EQ(store.get_if<alpha_info>()->value, 3);
}

} // namespace
