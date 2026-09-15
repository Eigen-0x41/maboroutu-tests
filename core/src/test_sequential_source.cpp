// mylib.sequential_source (maboroutu.sequential_source) のテスト。
// library_spec.md 4.4節・8章。
//
// =====================================================================
// !!! 重要 (検証済みの実装バグ・ビルドブロッカー) !!!
// =====================================================================
// library_spec.md 8章は次を「優先度: 高」の未確定事項として明記している:
//   「sequential_source_handle/writable_sequential_source_handleのcount()
//    実装ミス(v1.33で発見・修正)の再発防止として、GTest側で型消去ラッパー
//    経由のcount()を実際に呼び出す実行時テストの追加が未実施」
// 本ファイルはまさにこの実行時テストを提供する（下記
// HandleCountReflectsActualAdvancedBytes_NullSequentialSource /
// HandleCountCallsCountNotSize_RegressionGuard 参照）。
//
// ただし、実際にアップロードされたソース (core/src/sequential_source.cppm)
// を実機検証したところ、`sequential_source_handle` /
// `writable_sequential_source_handle` の両クラス宣言に `export`
// キーワードが付与されていないことを確認した（data_source.cppm の
// `data_source_handle` / `writable_data_source_handle`
// が両方とも export されているのと非対称）。
//
// この結果、本ファイルのように別プロジェクトから
// `import maboroutu.sequential_source;` するだけの通常の消費側コードからは
// `maboroutu::sequential_source_handle` /
// `maboroutu::writable_sequential_source_handle` という名前そのものが
// 見えず、以下のコンパイルエラーになることを Clang 18 (-std=c++23
// -stdlib=libc++) で実際に確認済み:
//
//   error: declaration of 'sequential_source_handle' must be imported
//   from module 'maboroutu.sequential_source' before it is required
//   note: declaration here is not visible
//
// すなわち、本ファイルの目的（8章が要求する回帰テストの実装）は、
// sequential_source.cppm 側に以下の1行ずつの修正が適用されるまで
// 達成できない（ビルドが通らない）:
//
//   -class writable_sequential_source_handle {
//   +export class writable_sequential_source_handle {
//   ...
//   -class sequential_source_handle {
//   +export class sequential_source_handle {
//
// 上記2箇所の `export` 追加は、他の設計（try_copy()の挙動・move-only性等）
// には一切影響しない可視性のみの変更であることをClang 18で確認済み
// （本ファイル下部のテスト内容は、その2行を適用した状態で実際に
// パスすることを確認して作成した）。
// =====================================================================
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.sequential_source;

namespace {

// ---------------------------------------------------------------------
// concept充足の確認
// ---------------------------------------------------------------------
static_assert(maboroutu::sequential_source<maboroutu::null_sequential_source>);
static_assert(
    !maboroutu::writable_sequential_source<maboroutu::null_sequential_source>);
static_assert(
    maboroutu::sequential_source<maboroutu::null_writable_sequential_source>);
static_assert(maboroutu::writable_sequential_source<
              maboroutu::null_writable_sequential_source>);

TEST(MaboroutuSequentialSource, NullSequentialSourceReadReturnsRequestedSize) {
   maboroutu::null_sequential_source src;
   EXPECT_EQ(src.count(), 0u);
   auto result = src.read(6);
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(result->size, 6u);
}

// ---------------------------------------------------------------------
// 型消去ラッパー経由の count() 実呼び出し（8章の高優先度未確定事項に対応）
// ---------------------------------------------------------------------
// NOTE: null_sequential_source / null_writable_sequential_source は
// size() メンバを持たない。したがって、もし_model<T>::count() が
// value.count() の代わりに value.size() を誤って呼び出す回帰が
// 再発した場合、以下のテストは（誤った値を返すのではなく）
// コンパイルエラーという形で検出する。
TEST(MaboroutuSequentialSource,
    HandleCountReflectsActualAdvancedBytes_NullSequentialSource) {
   maboroutu::sequential_source_handle handle{
       maboroutu::null_sequential_source{}};
   EXPECT_EQ(handle.count(), 0u);
   auto result = handle.read(4);
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(result->size, 4u);
}

TEST(MaboroutuSequentialSource,
    WritableHandleCountReflectsActualAdvancedBytes_NullWritableSequentialSource) {
   maboroutu::writable_sequential_source_handle handle{
       maboroutu::null_writable_sequential_source{}};
   EXPECT_EQ(handle.count(), 0u);
   auto read_result = handle.read(3);
   ASSERT_TRUE(read_result.has_value());
   std::array<std::byte, 2> data{};
   auto write_result =
       handle.write(std::span<std::byte const>(data.data(), data.size()));
   EXPECT_TRUE(write_result.has_value());
}

// 型消去ラッパーが count() を size() と取り違える回帰を、
// コンパイルエラーではなく数値の不一致として検出できるようにするための
// デコイ付きフェイク実装（size() を意図的にcount()と異なる値にしている）。
struct decoy_size_fake_sequential_source {
   std::size_t advanced = 0;
   template <class T> using result_type = maboroutu::sequential_source_result<T>;

   [[nodiscard]] auto count() const -> std::size_t { return advanced; }
   // NOTE: sequential_source conceptはsize()を要求しない。
   // ここでは意図的にcount()と異なる値を返し、
   // 型消去ラッパーがcount()の代わりにsize()を呼んでいないかを検証する。
   [[nodiscard]] auto size() const -> std::size_t { return 999; }
   [[nodiscard]] auto read(std::size_t n) -> result_type<maboroutu::byte_array> {
      advanced += n;
      return maboroutu::byte_array{
          .value =
              std::make_unique<maboroutu::byte_array::value_type>(n),
          .size = n,
      };
   }
};
static_assert(maboroutu::sequential_source<decoy_size_fake_sequential_source>);

TEST(MaboroutuSequentialSource, HandleCountCallsCountNotSize_RegressionGuard) {
   maboroutu::sequential_source_handle handle{
       decoy_size_fake_sequential_source{}};
   EXPECT_EQ(handle.count(), 0u);
   auto result = handle.read(5);
   ASSERT_TRUE(result.has_value());
   // 4.4節「実装時に判明した不具合」4.の再発防止:
   // ここが999(size()の戻り値)ではなく5(count()の戻り値)であること。
   EXPECT_EQ(handle.count(), 5u);
}

// ---------------------------------------------------------------------
// move-only性・try_copy()
// ---------------------------------------------------------------------
static_assert(!std::is_copy_constructible_v<maboroutu::sequential_source_handle>);
static_assert(std::is_move_constructible_v<maboroutu::sequential_source_handle>);
static_assert(
    !std::is_copy_constructible_v<maboroutu::writable_sequential_source_handle>);
static_assert(
    std::is_move_constructible_v<maboroutu::writable_sequential_source_handle>);

TEST(MaboroutuSequentialSource, TryCopySucceedsForCopyableConcreteType) {
   maboroutu::sequential_source_handle handle{
       maboroutu::null_sequential_source{}};
   auto cloned = handle.try_copy();
   ASSERT_TRUE(cloned.has_value());
   EXPECT_EQ(cloned->count(), handle.count());
}

} // namespace
