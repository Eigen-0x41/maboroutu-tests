// mylib.slot_map (maboroutu.slot_map) のテスト。
// library_spec.md 4.7節・6章「slot_mapの検証」。
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.slot_map;

namespace {

// 4.7節 v1.25 推奨規約: IndexT の基底型は std::size_t とする。
enum class idx_t : std::size_t {};

// ---------------------------------------------------------------------
// コンパイル時制約の検証: 符号付き基底型・非enum型はインスタンス化できない
// ---------------------------------------------------------------------
// NOTE（検証結果）: maboroutu::slot_map<IndexT, T> は
// basic_slot_map<IndexT, T, make_deque> のエイリアスだが、make_deque
// 自体はモジュール内部型で export されていないため、外部の翻訳単位からは
// basic_slot_map<...> を直接指定した否定的コンパイル時テスト
// (static_assert(!requires{ typename basic_slot_map<...>; })) を書けない。
// また slot_map<BadIndexT, T> エイリアス経由で同じ検証を試みると、
// Clang 18 (-std=c++23 -stdlib=libc++) では requires式の型要件
// (typename T;) が期待通りSFINAEの範囲内で失敗として吸収されず、
// 「constraints not satisfied」という回復不能なハードエラーとして
// 報告されることを確認した（basic_slot_mapのrequires節自体は正しく
// signed_idx_t/intを拒否している —
// 拒否そのものは正常に機能しており、問題はrequires式内での
// SFINAE吸収がエイリアス越しにうまく働かない点のみ）。
// このため、本ファイルではこの否定的コンパイル時テストを行わない。
// library_spec.md 6章が要求する「符号付き基底型のenumや非enum型を渡した
// 場合にインスタンス化できないことをstatic_assert(!requires{...})で検証
// する」というコンパイル時ネガティブテストは、ライブラリ本体側の
// テスト（basic_slot_mapへの直接アクセスが可能な同一モジュール内）で
// 実施されることを前提とする。

// ---------------------------------------------------------------------
// checkout()+construct_at() (2ステップ) と insert() (1ステップ) が
// 同じ状態になること
// ---------------------------------------------------------------------
TEST(MaboroutuSlotMap, CheckoutConstructAtMatchesInsertState) {
   maboroutu::slot_map<idx_t, int> two_step;
   auto key = two_step.checkout();
   auto constructed_key = two_step.construct_at(key, 123);
   EXPECT_EQ(constructed_key, key);

   maboroutu::slot_map<idx_t, int> one_step;
   auto insert_key = one_step.insert(123);

   EXPECT_EQ(two_step.size(), one_step.size());
   EXPECT_EQ(two_step.free_size(), one_step.free_size());
   EXPECT_EQ(two_step[key], one_step[insert_key]);

   // begin()からの走査結果も一致する（要素が1つなので単純比較で足りる）。
   EXPECT_EQ((*two_step.begin()).second, (*one_step.begin()).second);
}

TEST(MaboroutuSlotMap, ConstructAtOnAlreadyConstructedSlotReturnsNpos) {
   maboroutu::slot_map<idx_t, int> map;
   auto key = map.checkout();
   map.construct_at(key, 1);
   auto second = map.construct_at(key, 2);
   constexpr auto npos = maboroutu::slot_map<idx_t, int>::npos;
   EXPECT_EQ(second, npos);
   // 既存の値は上書きされない。
   EXPECT_EQ(map[key], 1);
}

// ---------------------------------------------------------------------
// cancel(): construct_at()を呼ばずにcheckout()済みスロットを戻す
// ---------------------------------------------------------------------
TEST(MaboroutuSlotMap, CancelReturnsSlotToFreeListWithoutConstructing) {
   maboroutu::slot_map<idx_t, int> map;
   auto key = map.checkout();
   EXPECT_EQ(map.free_size(), 0u); // checkout済み・未construct分は
                                   // free_size にカウントされない
   map.cancel(key);
   EXPECT_EQ(map.free_size(), 1u);
   EXPECT_EQ(map.size(), 0u);

   // キャンセル後、同じスロットが再利用される。
   auto reused = map.checkout();
   EXPECT_EQ(reused, key);
}

// ---------------------------------------------------------------------
// erase()後、同一スロットがcheckout()/insert()で再利用される
// ---------------------------------------------------------------------
TEST(MaboroutuSlotMap, EraseThenReinsertReusesSlot) {
   maboroutu::slot_map<idx_t, int> map;
   auto key = map.insert(10);
   map.erase(key);
   EXPECT_FALSE(map.contains(key));

   auto new_key = map.insert(20);
   EXPECT_EQ(new_key, key);
   EXPECT_EQ(map[new_key], 20);
}

TEST(MaboroutuSlotMap, ContainsIsFalseForNposAndOutOfRangeKeys) {
   maboroutu::slot_map<idx_t, int> map;
   EXPECT_FALSE(map.contains(maboroutu::slot_map<idx_t, int>::npos));
   EXPECT_FALSE(map.contains(static_cast<idx_t>(999)));
}

TEST(MaboroutuSlotMap, AtThrowsOutOfRangeForMissingKey) {
   maboroutu::slot_map<idx_t, int> map;
   EXPECT_THROW(map.at(static_cast<idx_t>(0)), std::out_of_range);
}

// ---------------------------------------------------------------------
// move-only型の格納
// ---------------------------------------------------------------------
TEST(MaboroutuSlotMap, StoresMoveOnlyValueType) {
   maboroutu::slot_map<idx_t, std::unique_ptr<int>> map;
   auto key = map.emplace(std::make_unique<int>(55));
   ASSERT_NE(map[key], nullptr);
   EXPECT_EQ(*map[key], 55);
}

// ---------------------------------------------------------------------
// get_if(): 自由関数 (std::variant::get_if 前例)
// ---------------------------------------------------------------------
TEST(MaboroutuSlotMap, GetIfReturnsPointerOrNullptr) {
   maboroutu::slot_map<idx_t, int> map;
   auto key = map.insert(7);
   ASSERT_NE(maboroutu::get_if(map, key), nullptr);
   EXPECT_EQ(*maboroutu::get_if(map, key), 7);

   map.erase(key);
   EXPECT_EQ(maboroutu::get_if(map, key), nullptr);
}

// ---------------------------------------------------------------------
// inplace_slot_map: 固定長版。全スロット使用後の境界外アクセス検証
// (v1.16修正の再発防止、6章記載の検証項目)
// ---------------------------------------------------------------------
TEST(MaboroutuSlotMap, InplaceSlotMapThrowsOutOfRangeWhenExhausted) {
   constexpr std::size_t capacity = 4;
   maboroutu::inplace_slot_map<idx_t, int, capacity> map;

   for (std::size_t i = 0; i < capacity; ++i) {
      map.insert(static_cast<int>(i));
   }
   EXPECT_EQ(map.size(), capacity);
   EXPECT_EQ(map.free_size(), 0u);

   EXPECT_THROW(map.checkout(), std::out_of_range);
}

TEST(MaboroutuSlotMap, InplaceSlotMapReusesSlotsAfterErase) {
   constexpr std::size_t capacity = 3;
   maboroutu::inplace_slot_map<idx_t, int, capacity> map;
   std::vector<idx_t> keys;
   for (std::size_t i = 0; i < capacity; ++i) {
      keys.push_back(map.insert(static_cast<int>(i)));
   }
   map.erase(keys[1]);
   auto reused = map.insert(99);
   EXPECT_EQ(reused, keys[1]);
   EXPECT_EQ(map[reused], 99);
}

// ---------------------------------------------------------------------
// イテレータ: 構築済み要素のみを走査する双方向イテレータ
// ---------------------------------------------------------------------
TEST(MaboroutuSlotMap, IteratorVisitsOnlyConstructedElements) {
   maboroutu::slot_map<idx_t, int> map;
   auto k1 = map.insert(1);
   auto k2 = map.insert(2);
   auto k3 = map.insert(3);
   map.erase(k2);

   int sum = 0;
   std::size_t count = 0;
   for (auto ite = map.begin(); ite != map.end(); ++ite) {
      sum += (*ite).second;
      ++count;
   }
   EXPECT_EQ(count, 2u);
   EXPECT_EQ(sum, 1 + 3);
   (void)k1;
   (void)k3;
}

} // namespace
