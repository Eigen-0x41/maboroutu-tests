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
   auto view(maboroutu::region r)
       -> maboroutu::data_buffer_result<std::span<std::byte>> {
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

} // namespace
