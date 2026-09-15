// mylib.data_buffer (maboroutu.data_buffer) のテスト。
//
// NOTE（検証結果・要確認）: library_spec.md（v1.33、2.1節/3章/4章）には
// data_buffer という名称のコンポーネントの記載が見当たらない。アップロード
// された実装（core/src/data_buffer.cppm）には export module
// maboroutu.data_buffer として実在するため、本テストは実装のexport済み
// シグネチャを一次情報として作成したが、仕様書との対応関係（正式な仕様
// 節が存在するのか、未文書化の作業中コンポーネントなのか）は不明である。
// この食い違いは映幻に確認することを推奨する。
#include <array>
#include <cstddef>
#include <span>

#include <gtest/gtest.h>

import maboroutu.core;
import maboroutu.error;
import maboroutu.data_source;
import maboroutu.data_buffer;

namespace {

// data_buffer は writable_data_source を拡張した concept である
// (data_buffer.cppm 参照)。
static_assert(maboroutu::writable_data_source<maboroutu::null_data_buffer>);
static_assert(maboroutu::data_buffer<maboroutu::null_data_buffer>);

TEST(MaboroutuDataBuffer, InheritsWritableDataSourceBehaviour) {
   maboroutu::null_data_buffer buf;
   auto size_result = buf.size();
   ASSERT_TRUE(size_result.has_value());
   EXPECT_EQ(*size_result, 0u);

   auto read_result = buf.read(maboroutu::region{.offset = 0, .size = 4});
   ASSERT_TRUE(read_result.has_value());
   EXPECT_EQ(read_result->size, 4u);

   std::array<std::byte, 2> data{};
   auto write_result =
       buf.write(maboroutu::region{.offset = 0, .size = data.size()},
                std::span<std::byte const>(data.data(), data.size()));
   EXPECT_TRUE(write_result.has_value());
}

TEST(MaboroutuDataBuffer, AppendAndGrowSucceedOnNullStub) {
   std::array<std::byte, 3> data{};
   auto append_result =
       maboroutu::null_data_buffer::append(
           std::span<std::byte const>(data.data(), data.size()));
   ASSERT_TRUE(append_result.has_value());
   EXPECT_EQ(append_result->offset, 0u);
   EXPECT_EQ(append_result->size, 0u);

   auto grow_result = maboroutu::null_data_buffer::grow(8);
   ASSERT_TRUE(grow_result.has_value());
}

TEST(MaboroutuDataBuffer, ViewAlwaysFailsOnNullStub) {
   // null_data_buffer::view() は posix /dev/null 相当のスタブであり、
   // 常にエラーを返す実装になっている（data_buffer.cppm 参照）。
   auto view_result =
       maboroutu::null_data_buffer::view(maboroutu::region{.offset = 0, .size = 1});
   EXPECT_FALSE(view_result.has_value());
}

} // namespace
