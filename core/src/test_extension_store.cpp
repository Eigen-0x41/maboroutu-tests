// mylib.extension_store (maboroutu.extension_store) のテスト。
// library_spec.md 4.2節・6章。
//
// =====================================================================
// !!! 重要 (検証済みの実装バグ・ビルドブロッカー、計4件) !!!
// =====================================================================
// アップロードされたソース (core/src/extension_store.cppm) を Clang 18
// (-std=c++23 -stdlib=libc++) で実機検証したところ、以下4件の独立した
// コンパイルエラーを確認した。本ファイルはこれら4件すべてが修正された
// 状態を前提に書かれている（修正版で実際にビルド・パスすることを確認
// 済み）。未修正の場合、本ファイルはビルドが通らない。
//
//  (a) local_typeid::cmp<T>() のフォールバックオーバーロードに
//      return文が無い:
//        template <class T> static consteval auto cmp() -> size_t {
//           static_assert(false, "is not entry type!");
//        }                                    // <- ここでreturn文が無い
//      GCC14 / Clang18 いずれでも
//      "no return statement in 'constexpr' function returning non-void"
//      という不変のコンパイルエラーになる（static_assert(false)の後に
//      到達しないコードだが、非void関数として構文的に整合していない
//      ため、コンパイラ依存ではなく規格上の問題）。
//      示唆される最小修正: static_assert の直後に `return 0;` を追加。
//
//  (b) extension_store 内の静的検証で非staticメンバ関数を
//      インスタンス無しに呼んでいる:
//        static_assert(value_type::size() == id::size(), "is match size.");
//      value_type は std::array<...> であり、size() は非static
//      メンバ関数のため `value_type::size()` は
//      "call to non-static member function without an object argument"
//      というコンパイルエラーになる。
//      示唆される最小修正:
//        static_assert(std::tuple_size_v<value_type> == id::size(), ...);
//
//  (c) library_spec.md 4.2節・application_spec.md 6章が transform_each
//      という独立した名前で説明しているメソッドが、実装では
//      visit_each という同名で（かつ既存のconst版visit_each
//      と全く同一のrequires節のまま）宣言されている:
//        template <class Visitor>
//           requires(std::invocable<Visitor &, const Types &> && ...)
//        auto visit_each(this const self_type &self, Visitor &&vis)
//            -> self_type { ... }
//      同一クラス内に「同じ制約・同じ引数列・戻り値型だけが違う」
//      オーバーロードが2つ存在する状態になり、const版visit_each
//      を実際に呼び出す箇所すべてで
//      "call to member function 'visit_each' is ambiguous" /
//      "similar constraint expressions not considered equivalent;
//       constraint expressions cannot be considered equivalent unless
//       they originate from the same concept"
//      という実行時ではなく呼び出し時のコンパイルエラーになる
//      （両者が同一の名前付きconceptからではなくインラインの
//      requires式から生成されているため、コンパイラは2つの制約式を
//      「同値」と証明できず、曖昧な呼び出しとして扱う）。
//      示唆される最小修正: 3つ目のオーバーロードを
//      transform_each へ改名し、仕様書通りの追加制約
//      `(std::constructible_from<Types,
//        std::invoke_result_t<Visitor &, const Types &>> && ...)`
//      を付与する（この追加によりconst版visit_each
//      とtransform_each はシグネチャ・制約の両方で明確に区別される）。
//
//  (d) 上記(c)の修正後（メソッド名を transform_each に変更した後）でも、
//      その本体にある
//        ret_value.template set<alt_t::element_type>(vis(*alt));
//      は alt_t::element_type が依存名であるにもかかわらず
//      typename が付与されておらず、
//      "missing 'typename' prior to dependent type name
//       'alt_t::element_type'" というコンパイルエラーになる。
//      示唆される最小修正:
//        ret_value.template set<typename alt_t::element_type>(vis(*alt));
//
// これら4件はいずれも特定のコンパイラの癖ではなく規格上の要請
// （非void関数の分岐網羅・非staticメンバ関数呼び出し・制約式の同値性・
//  依存名のtypename修飾）に起因するため、GCC/Clang/MSVCのいずれでも
// 再現すると考えられる（GCC14 / Clang18で実機確認済み。MSVCは未検証）。
// =====================================================================
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

// application_spec.md 6章「4. 全列挙処理」で示されている overloaded
// パターン（ライブラリ非提供、アプリ側で定義する前例）をそのままテストにも
// 適用する。
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
   // (4.2節「設計上の要点」参照)。
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
   // (4.2節「設計上の要点: transform_each」参照)。
   EXPECT_EQ(transformed.get_if<beta_info>(), nullptr);

   // 元のstoreは変更されない（複製であることの確認）。
   EXPECT_EQ(store.get_if<alpha_info>()->value, 3);
}

} // namespace
