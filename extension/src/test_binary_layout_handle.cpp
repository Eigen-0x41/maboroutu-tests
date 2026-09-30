// mylib.binary_layout_handle (maboroutu.binary_layout_handle) のテスト。
//
// NOTE（一次情報）: 本モジュールはlibrary_spec.mdにまだコンポーネント節が
// 無い新規モジュール。テストは映幻から提供された引き継ぎ資料
// 「maboroutu — GTest実装のための引き継ぎ資料」(3.4節・付録C・4章)を
// 一次情報として作成した。
//
// !!! 最重要 (引き継ぎ資料が明記する既知の回帰リスク) !!!
// 当初 finalize() が未確定ハンドルを検出して早期returnする際、
// まだ処理していない残りの保留パッチを未解決のまま放置していたため、
// 「finalize()のエラーを呼び出し側が正しくチェック・処理しても、
//  resolverが破棄される際に無関係なoffset_patchデストラクタのassert
//  でプログラムがクラッシュする」という問題があった(修正済み)。
// このため、FinalizeErrorLeavesResolverSafelyDestructible系のテストは、
// finalize()のエラー検知そのものだけでなく、その後にresolver/バッファが
// スコープを抜けて正常に破棄できる(プロセスがクラッシュしない)ことまでを
// 確認する必要がある(このテスト関数自体がクラッシュせず完了することが
// その確認になる)。
#include <array>
#include <cstddef>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.data_source;
import maboroutu.data_buffer;
import maboroutu.binary_convert;
import maboroutu.binary_layout;
import maboroutu.slot_map;
import maboroutu.binary_layout_handle;

namespace {

using ds_code_type =
    decltype(std::declval<maboroutu::data_source_result<int>>().error().code());
using buffer_code_type =
    decltype(std::declval<maboroutu::data_buffer_result<int>>().error().code());

// data_buffer concept を満たす、このファイル専用の in-memory モック
// (test_binary_layout.cpp の memory_buffer と同型)。
class memory_buffer {
   std::vector<std::byte> _data;

 public:
   template <class T> using result_type = maboroutu::data_source_result<T>;
   using view_type = std::span<std::byte>;

   [[nodiscard]] auto size() const -> result_type<std::size_t> {
      return _data.size();
   }
   [[nodiscard]] auto read(maboroutu::region r) -> result_type<maboroutu::byte_array> {
      if (r.offset + r.size > _data.size()) {
         return std::unexpected(
             maboroutu::error<ds_code_type>(ds_code_type::out_of_range));
      }
      maboroutu::byte_array out{
          .value = std::make_unique<maboroutu::byte_array::value_type>(r.size),
          .size = r.size,
      };
      std::memcpy(out.value.get(), _data.data() + r.offset, r.size);
      return out;
   }
   auto write(maboroutu::region r, std::span<std::byte const> data)
       -> result_type<void> {
      if (r.offset + r.size > _data.size()) {
         _data.resize(r.offset + r.size);
      }
      std::memcpy(_data.data() + r.offset, data.data(), r.size);
      return {};
   }
   auto append(std::span<std::byte const> data)
       -> maboroutu::data_buffer_result<maboroutu::region> {
      auto const offset = _data.size();
      _data.insert(_data.end(), data.begin(), data.end());
      return maboroutu::region{.offset = offset, .size = data.size()};
   }
   auto grow(std::size_t n) -> maboroutu::data_buffer_result<maboroutu::region> {
      auto const offset = _data.size();
      _data.resize(_data.size() + n);
      return maboroutu::region{.offset = offset, .size = n};
   }
   auto view(maboroutu::region r) -> maboroutu::data_buffer_result<view_type> {
      if (r.offset + r.size > _data.size()) {
         return std::unexpected(
             maboroutu::error<buffer_code_type>(buffer_code_type::out_of_range));
      }
      return std::span<std::byte>{_data.data() + r.offset, r.size};
   }
};
static_assert(maboroutu::data_buffer<memory_buffer>);

enum class handle_id : std::size_t {};
using resolver_type =
    maboroutu::deferred_resolver<handle_id, 4, maboroutu::endian::big,
                                 memory_buffer>;

// ---------------------------------------------------------------------
// 正常系: 前方参照 -> 実配置 -> resolve_handle -> finalize -> 値一致
// ---------------------------------------------------------------------
TEST(MaboroutuBinaryLayoutHandle,
    ForwardReferenceResolvesToActualPlacementOffset) {
   memory_buffer buf;
   resolver_type resolver;

   // 1. まだ配置されていない対象へのハンドルを発行し、パッチを予約する
   //    (このときプレースホルダはbufの先頭 offset=0..3 に予約される)。
   auto handle = resolver.checkout();
   ASSERT_TRUE(resolver.defer_patch(buf, handle).has_value());

   // 2. 対象を実際に配置する。
   std::array<std::byte, 5> payload{std::byte{1}, std::byte{2}, std::byte{3},
                                    std::byte{4}, std::byte{5}};
   auto placed =
       buf.append(std::span<std::byte const>(payload.data(), payload.size()));
   ASSERT_TRUE(placed.has_value());

   // 3. ハンドルへ実際のオフセットを確定させる。
   resolver.resolve_handle(handle, placed->offset);

   // 4. 保留中の全パッチを解決する。
   auto finalize_result = resolver.finalize();
   ASSERT_TRUE(finalize_result.has_value());

   // 5. パッチ済みの値が実際の配置オフセットと一致することを確認する。
   auto read_result = maboroutu::read_uint<4, maboroutu::endian::big>(buf, 0);
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(*read_result, placed->offset);
}

// ---------------------------------------------------------------------
// resolve_handle()を呼び忘れたハンドルに対するfinalize() -> エラー、
// かつその後resolverが正常に破棄できる(★最重要、上記コメント参照)
// ---------------------------------------------------------------------
TEST(MaboroutuBinaryLayoutHandle,
    FinalizeErrorsOnUnresolvedHandleAndResolverSurvivesDestruction) {
   memory_buffer buf;
   resolver_type resolver;

   auto unresolved_handle = resolver.checkout();
   ASSERT_TRUE(resolver.defer_patch(buf, unresolved_handle).has_value());
   // resolve_handle() を意図的に呼ばない。

   auto finalize_result = resolver.finalize();
   ASSERT_FALSE(finalize_result.has_value());
   EXPECT_EQ(finalize_result.error().code(),
             ds_code_type::invalid_member_variable);

   // ここで resolver / buf がスコープを抜けて破棄される。修正前は
   // 未処理のまま残っていた offset_patch のデストラクタで assert が
   // 発火しプロセスがクラッシュしていた。このテスト関数自体が
   // クラッシュせず完了すること自体が回帰していないことの確認となる。
}

TEST(MaboroutuBinaryLayoutHandle,
    FinalizeErrorAmongMultiplePendingPatchesLeavesAllAbandonable) {
   // 複数ハンドル・複数保留パッチが混在する状態で、1つだけ未確定の場合。
   memory_buffer buf;
   resolver_type resolver;

   auto resolved_handle = resolver.checkout();
   auto unresolved_handle = resolver.checkout();

   ASSERT_TRUE(resolver.defer_patch(buf, resolved_handle).has_value());
   ASSERT_TRUE(resolver.defer_patch(buf, unresolved_handle).has_value());

   resolver.resolve_handle(resolved_handle, 100);
   // unresolved_handle は resolve_handle() しない。

   auto finalize_result = resolver.finalize();
   ASSERT_FALSE(finalize_result.has_value());
   // resolver / buf の破棄でクラッシュしないことを確認する
   // (前段のresolved_handle分パッチも含め、すべて安全に破棄される)。
}

// ---------------------------------------------------------------------
// resolve_handle() の二重呼び出しは assert で検出される
// ---------------------------------------------------------------------
#if defined(GTEST_HAS_DEATH_TEST) && !defined(NDEBUG)
TEST(MaboroutuBinaryLayoutHandleDeathTest,
    DoubleResolveHandleTriggersAssert) {
   EXPECT_DEATH(
       {
          resolver_type resolver;
          auto handle = resolver.checkout();
          resolver.resolve_handle(handle, 10);
          resolver.resolve_handle(handle, 20); // 二重確定
       },
       "");
}
#endif // GTEST_HAS_DEATH_TEST && !NDEBUG


// =====================================================================
// 以下は追加テスト
// =====================================================================

// --- write() が常に失敗する data_buffer モック ---------------------------
// finalize() の「write失敗時に残パッチを abandon する」パスを検証するために
// 使う（data_buffer.cppm の nullスタブは write が常に成功するため別途用意）。
class write_failing_buffer {
   std::vector<std::byte> _data;

 public:
   template <class T> using result_type = maboroutu::data_source_result<T>;
   using view_type = std::span<std::byte>;

   [[nodiscard]] auto size() const -> result_type<std::size_t> {
      return _data.size();
   }
   [[nodiscard]] auto read(maboroutu::region r)
       -> result_type<maboroutu::byte_array> {
      if (r.offset + r.size > _data.size()) {
         return std::unexpected(
             maboroutu::error<ds_code_type>(ds_code_type::out_of_range));
      }
      maboroutu::byte_array out{
          .value =
              std::make_unique<maboroutu::byte_array::value_type>(r.size),
          .size = r.size,
      };
      std::memcpy(out.value.get(), _data.data() + r.offset, r.size);
      return out;
   }
   // write() は常に失敗する（テスト用）
   auto write(maboroutu::region, std::span<std::byte const>)
       -> result_type<void> {
      return std::unexpected(
          maboroutu::error<ds_code_type>(ds_code_type::out_of_range));
   }
   auto append(std::span<std::byte const> data)
       -> maboroutu::data_buffer_result<maboroutu::region> {
      auto const offset = _data.size();
      _data.insert(_data.end(), data.begin(), data.end());
      return maboroutu::region{.offset = offset, .size = data.size()};
   }
   auto grow(std::size_t n)
       -> maboroutu::data_buffer_result<maboroutu::region> {
      auto const offset = _data.size();
      _data.resize(_data.size() + n);
      return maboroutu::region{.offset = offset, .size = n};
   }
   auto view(maboroutu::region)
       -> maboroutu::data_buffer_result<view_type> {
      return std::unexpected(
          maboroutu::error<buffer_code_type>(
              buffer_code_type::out_of_range));
   }
};
static_assert(maboroutu::data_buffer<write_failing_buffer>);

// =====================================================================
// 正常系追加: 同一ハンドルに複数の defer_patch → 全て同じ値で解決
// =====================================================================
// 仕様: 1つのハンドルに対して複数箇所から defer_patch を呼べる。
// 期待動作: finalize() でそれぞれのプレースホルダが同一の確定値で埋まる。
TEST(MaboroutuBinaryLayoutHandle,
     SameHandleWithMultiplePatchesResolvesAllToSameValue) {
   memory_buffer buf;
   resolver_type resolver;

   // 1. ハンドルを発行し、同一ハンドルで2箇所パッチを予約する。
   auto handle = resolver.checkout();
   ASSERT_TRUE(resolver.defer_patch(buf, handle).has_value()); // 位置 0..3
   ASSERT_TRUE(resolver.defer_patch(buf, handle).has_value()); // 位置 4..7

   // 2. 対象を配置する（プレースホルダの後ろに実データ）。
   std::array<std::byte, 3> payload{std::byte{0xAA}, std::byte{0xBB},
                                    std::byte{0xCC}};
   auto placed =
       buf.append(std::span<std::byte const>(payload.data(), payload.size()));
   ASSERT_TRUE(placed.has_value());

   // 3. ハンドルを確定させ、finalize する。
   resolver.resolve_handle(handle, placed->offset);
   ASSERT_TRUE(resolver.finalize().has_value());

   // 4. 両パッチが同じ値で解決されているはず。
   auto r1 = maboroutu::read_uint<4, maboroutu::endian::big>(buf, 0);
   auto r2 = maboroutu::read_uint<4, maboroutu::endian::big>(buf, 4);
   ASSERT_TRUE(r1.has_value());
   ASSERT_TRUE(r2.has_value());
   EXPECT_EQ(*r1, static_cast<std::uint32_t>(placed->offset));
   EXPECT_EQ(*r2, static_cast<std::uint32_t>(placed->offset));
}

// =====================================================================
// 正常系追加: resolve_handle() を defer_patch() より先に呼ぶ（逆順解決）
// =====================================================================
// 対象オブジェクトが先に配置され（後方参照）、その後でパッチ箇所が
// 決まるケース。前方参照（先にパッチ予約→後で対象配置）の逆。
TEST(MaboroutuBinaryLayoutHandle,
     ResolveHandleBeforeDeferPatchWorksCorrectly) {
   memory_buffer buf;
   resolver_type resolver;

   // 1. 対象を先に配置する。
   std::array<std::byte, 5> payload{};
   auto placed =
       buf.append(std::span<std::byte const>(payload.data(), payload.size()));
   ASSERT_TRUE(placed.has_value());

   // 2. ハンドルを発行し、配置済みの確定値を先に登録する。
   auto handle = resolver.checkout();
   resolver.resolve_handle(handle, placed->offset); // defer_patch より前

   // 3. 後からパッチを予約する（この時点でバッファは 5+4=9 バイト）。
   ASSERT_TRUE(resolver.defer_patch(buf, handle).has_value());
   auto const patch_pos = placed->offset + placed->size; // 5

   // 4. finalize する。
   ASSERT_TRUE(resolver.finalize().has_value());

   // 5. パッチに placed->offset (=0) が書き込まれているはず。
   auto r = maboroutu::read_uint<4, maboroutu::endian::big>(buf, patch_pos);
   ASSERT_TRUE(r.has_value());
   EXPECT_EQ(*r, static_cast<std::uint32_t>(placed->offset));
}

// =====================================================================
// 正常系追加: 保留パッチが0件の状態で finalize() は成功する
// =====================================================================
TEST(MaboroutuBinaryLayoutHandle,
     FinalizeWithNoPendingPatchesSucceeds) {
   resolver_type resolver;
   auto result = resolver.finalize();
   EXPECT_TRUE(result.has_value());
}

// =====================================================================
// 正常系追加: 複数ハンドル・各1パッチが全て解決されて正しく読める
// =====================================================================
TEST(MaboroutuBinaryLayoutHandle,
     MultipleHandlesEachWithOnePatchFinalizeCorrectly) {
   memory_buffer buf;
   resolver_type resolver;

   auto h1 = resolver.checkout();
   auto h2 = resolver.checkout();

   // 両方のパッチを先に予約する（前方参照）。
   ASSERT_TRUE(resolver.defer_patch(buf, h1).has_value()); // 位置 0..3
   ASSERT_TRUE(resolver.defer_patch(buf, h2).has_value()); // 位置 4..7

   // 対象を順に配置する。
   std::array<std::byte, 2> p1{}, p2{};
   auto loc1 =
       buf.append(std::span<std::byte const>(p1.data(), p1.size()));
   auto loc2 =
       buf.append(std::span<std::byte const>(p2.data(), p2.size()));
   ASSERT_TRUE(loc1.has_value());
   ASSERT_TRUE(loc2.has_value());

   resolver.resolve_handle(h1, loc1->offset);
   resolver.resolve_handle(h2, loc2->offset);
   ASSERT_TRUE(resolver.finalize().has_value());

   // それぞれ異なるオフセット値が書き込まれているはず。
   auto r1 = maboroutu::read_uint<4, maboroutu::endian::big>(buf, 0);
   auto r2 = maboroutu::read_uint<4, maboroutu::endian::big>(buf, 4);
   ASSERT_TRUE(r1.has_value());
   ASSERT_TRUE(r2.has_value());
   EXPECT_EQ(*r1, static_cast<std::uint32_t>(loc1->offset));
   EXPECT_EQ(*r2, static_cast<std::uint32_t>(loc2->offset));
   EXPECT_NE(*r1, *r2); // 2つのパッチが別の値になっていること
}

// =====================================================================
// エラー系追加: finalize() の write 失敗時、残パッチを abandon する
// =====================================================================
// !!! 最重要（回帰リスク）: この動作の修正経緯 !!!
// deferred_resolver::finalize() は当初、パッチの resolve() が書き込み
// エラーで失敗した際、以降の保留パッチを未解決のまま放置していた。
// この状態でリゾルバが破棄されると、未解決の position_patch の
// デストラクタが assert で発火しプロセスがクラッシュした（修正済み）。
// 修正後: finalize() は write 失敗を検出したら即座に
// _abandon_from(失敗インデックス+1) を呼び、残パッチを abandon する。
// このテスト関数自体がクラッシュせず完了することが「回帰していない」
// ことの確認になる。
TEST(MaboroutuBinaryLayoutHandle,
     FinalizeWriteErrorAbandonsRemainingPatchesAndResolverSurvivesDestruction) {
   // write() が常に失敗するバッファを使う。
   write_failing_buffer buf;
   maboroutu::deferred_resolver<handle_id, 4, maboroutu::endian::big,
                                write_failing_buffer>
       resolver;

   auto h1 = resolver.checkout();
   auto h2 = resolver.checkout();

   // 2つのパッチを予約する（grow() は成功するので reserve_patch は通る）。
   ASSERT_TRUE(resolver.defer_patch(buf, h1).has_value());
   ASSERT_TRUE(resolver.defer_patch(buf, h2).has_value());

   resolver.resolve_handle(h1, 0);
   resolver.resolve_handle(h2, 4);

   // finalize() は最初のパッチの write() 失敗でエラーを返す。
   auto result = resolver.finalize();
   ASSERT_FALSE(result.has_value());
   EXPECT_EQ(result.error().code(), ds_code_type::out_of_range);

   // ここで resolver / buf がスコープを抜けて破棄される。
   // 修正前は h2 のパッチが未解決のまま残り、position_patch の
   // デストラクタ assert でクラッシュしていた。
   // このテスト自体がクラッシュせず完了することが再発防止の確認となる。
}

} // namespace
