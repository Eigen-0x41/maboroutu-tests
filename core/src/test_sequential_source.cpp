// mylib.sequential_source (maboroutu.sequential_source) のテスト。
//
// 型消去ラッパー（sequential_source_handle / writable_sequential_source_handle）
// 経由の count() を実際に呼び出す実行時テストを含む（count() の実装ミスの
// 再発防止。下記
// HandleCountReflectsActualAdvancedBytes_NullSequentialSource /
// HandleCountCallsCountNotSize_RegressionGuard 参照）。
// 両ハンドルは、別プロジェクトから import するだけで参照できるよう export
// されている必要がある。
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

TEST(MaboroutuSequentialSource, NullSequentialSourceReadsOnlyEmptyArray) {
   maboroutu::null_sequential_source src;
   EXPECT_EQ(src.count(), 0u);
   auto result = src.read(0);
   ASSERT_TRUE(result.has_value());
   EXPECT_EQ(result->size, 0u);
   EXPECT_FALSE(src.read(6).has_value());
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
   EXPECT_TRUE(handle.read(0).has_value());
   EXPECT_FALSE(handle.read(4).has_value());
}

TEST(MaboroutuSequentialSource,
    WritableHandleCountReflectsActualAdvancedBytes_NullWritableSequentialSource) {
   maboroutu::writable_sequential_source_handle handle{
       maboroutu::null_writable_sequential_source{}};
   EXPECT_EQ(handle.count(), 0u);
   EXPECT_TRUE(handle.read(0).has_value());
   EXPECT_FALSE(handle.read(3).has_value());
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
   [[nodiscard]] auto read_into(std::size_t n, std::span<std::byte>)
       -> result_type<void> {
      advanced += n;
      return {};
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

TEST(MaboroutuSequentialSourceErrc, EnumeratorsAreReachableByName) {
   using code = maboroutu::errc::sequential_source;
   maboroutu::error<code> e(code::invalid_member_variable);
   EXPECT_EQ(e.code(), code::invalid_member_variable);
   EXPECT_FALSE(e.message().empty());
   EXPECT_NE(maboroutu::errc::error_message(code::out_of_range),
             maboroutu::errc::error_message(code::operation_failure));
}

TEST(MaboroutuSequentialSource, NullSourcesReadIntoOnlyZeroSize) {
   std::array<std::byte, 4> dst{};
   maboroutu::null_sequential_source a;
   EXPECT_TRUE(a.read_into(0, std::span<std::byte>(dst)).has_value());
   EXPECT_FALSE(a.read_into(1, std::span<std::byte>(dst)).has_value());
   maboroutu::null_writable_sequential_source b;
   EXPECT_TRUE(b.read_into(0, std::span<std::byte>(dst)).has_value());
   EXPECT_FALSE(b.read_into(1, std::span<std::byte>(dst)).has_value());
}

TEST(MaboroutuSequentialSource, HandleDelegatesReadInto) {
   std::array<std::byte, 4> dst{};
   maboroutu::sequential_source_handle handle{decoy_size_fake_sequential_source{}};
   ASSERT_TRUE(handle.read_into(3, std::span<std::byte>(dst)).has_value());
   EXPECT_EQ(handle.count(), 3u);
   maboroutu::writable_sequential_source_handle whandle{
       maboroutu::null_writable_sequential_source{}};
   EXPECT_TRUE(whandle.read_into(0, std::span<std::byte>(dst)).has_value());
   EXPECT_FALSE(whandle.read_into(2, std::span<std::byte>(dst)).has_value());
}
