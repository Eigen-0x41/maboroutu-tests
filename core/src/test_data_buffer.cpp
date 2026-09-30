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
#include <stdexcept>
#include <vector>

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
       maboroutu::null_data_buffer::view(
           maboroutu::region{.offset = 0, .size = 1});
   EXPECT_FALSE(view_result.has_value());
}

// =======================================================================
// segmented_span / basic_segmented_span
// =======================================================================
//
// segmented_span<T, UnitSize> は固定長ユニット（std::span<T, UnitSize>）の
// 配列を非所有で保持し、ユニット境界を跨いで連続したバイト列のように
// アクセスできる非所有ビュー型。data_buffer の view_type として使われる。
//
// 本セクションのテストは segmented_span<std::byte, 4> のみを扱う。
// UnitSize=4096 等のコンパイル時充足確認は data_buffer.cppm 内の
// static_assert が担う。
//
// NOTE(データレイアウト): フィクスチャでは2ユニット×4バイトを使い、
//   seg[0..3] → ユニット0 (0x10〜0x13)
//   seg[4..7] → ユニット1 (0x20〜0x23)
// というマッピングを前提としてテストの期待値を設定している。

// --- コンパイル時: 消費側からの concept 充足確認 -------------------------
static_assert(
    std::ranges::random_access_range<
        maboroutu::segmented_span<std::byte, 4>>,
    "segmented_span<std::byte,4> は random_access_range を満たす");
static_assert(
    std::same_as<
        std::ranges::range_value_t<maboroutu::segmented_span<std::byte, 4>>,
        std::byte>,
    "segmented_span<std::byte,4>::value_type は std::byte");
static_assert(
    std::random_access_iterator<
        maboroutu::segmented_span<std::byte, 4>::iterator>);
static_assert(
    std::random_access_iterator<
        maboroutu::segmented_span<std::byte, 4>::const_iterator>);

// --- テスト用ヘルパーフィクスチャ ----------------------------------------
// UnitSize=4 のユニット2つ（計8バイト）にわたる segmented_span を構築する。
// バッキングストレージ（_u0, _u1, _spans）はフィクスチャが所有し、
// テスト終了まで生存を保証する。
class SegmentedSpanTest : public ::testing::Test {
 protected:
   std::array<std::byte, 4> _u0{
       std::byte{0x10}, std::byte{0x11},
       std::byte{0x12}, std::byte{0x13}};
   std::array<std::byte, 4> _u1{
       std::byte{0x20}, std::byte{0x21},
       std::byte{0x22}, std::byte{0x23}};
   std::array<std::span<std::byte, 4>, 2> _spans;

   void SetUp() override {
      _spans[0] = std::span<std::byte, 4>{_u0};
      _spans[1] = std::span<std::byte, 4>{_u1};
   }

   [[nodiscard]] auto make_seg()
       -> maboroutu::segmented_span<std::byte, 4> {
      return maboroutu::segmented_span<std::byte, 4>{
          std::span<std::span<std::byte, 4>>{_spans}};
   }
};

// --- デフォルト構築: 空の segmented_span ---------------------------------
TEST(MaboroutuSegmentedSpan, DefaultConstructionIsEmpty) {
   maboroutu::segmented_span<std::byte, 4> seg{};
   EXPECT_EQ(seg.size(), 0u);
   EXPECT_TRUE(seg.empty());
   EXPECT_EQ(seg.begin(), seg.end());
   EXPECT_EQ(seg.cbegin(), seg.cend());
}

// --- size() がユニット総バイト数を返す ------------------------------------
TEST_F(SegmentedSpanTest, SizeReflectsTotalBytesAcrossAllUnits) {
   auto seg = make_seg();
   EXPECT_EQ(seg.size(), 8u); // 2ユニット × 4バイト
   EXPECT_FALSE(seg.empty());
}

// --- operator[] でユニット境界を跨いで読める ------------------------------
TEST_F(SegmentedSpanTest, BracketOperatorAccessesExpectedBytes) {
   auto seg = make_seg();
   EXPECT_EQ(std::to_integer<unsigned>(seg[0]), 0x10u);
   EXPECT_EQ(std::to_integer<unsigned>(seg[3]), 0x13u);
   // ユニット境界跨ぎ: インデックス4 → ユニット1の先頭
   EXPECT_EQ(std::to_integer<unsigned>(seg[4]), 0x20u);
   EXPECT_EQ(std::to_integer<unsigned>(seg[7]), 0x23u);
}

// --- operator[] による書き込みがバッキングストレージに反映される -----------
TEST_F(SegmentedSpanTest, BracketOperatorWriteReflectsInBackingStorage) {
   auto seg = make_seg();
   seg[2] = std::byte{0xFF};
   // _u0[2] が変化しているはず
   EXPECT_EQ(std::to_integer<unsigned>(_u0[2]), 0xFFu);
   // seg 経由でも同じ値が読める
   EXPECT_EQ(std::to_integer<unsigned>(seg[2]), 0xFFu);
}

// --- at() の正常系 --------------------------------------------------------
TEST_F(SegmentedSpanTest, AtReadsExpectedBytes) {
   auto seg = make_seg();
   EXPECT_EQ(std::to_integer<unsigned>(seg.at(0)), 0x10u);
   EXPECT_EQ(std::to_integer<unsigned>(seg.at(4)), 0x20u);
   EXPECT_NO_THROW(seg.at(7)); // 最後の有効インデックス
}

// --- at() は OOB で std::out_of_range を投げる ----------------------------
TEST_F(SegmentedSpanTest, AtThrowsOutOfRangeOnOOBAccess) {
   auto seg = make_seg();
   EXPECT_THROW(seg.at(8), std::out_of_range);
}

// --- segments() がユニットスパンの列を返す --------------------------------
TEST_F(SegmentedSpanTest, SegmentsAccessorReturnsUnderlyingUnitSpans) {
   auto seg = make_seg();
   auto segs = seg.segments();
   ASSERT_EQ(segs.size(), 2u);
   EXPECT_EQ(segs[0].size(), 4u);
   EXPECT_EQ(segs[1].size(), 4u);
   // バッキングストレージと同じ内容を参照する
   EXPECT_EQ(std::to_integer<unsigned>(segs[0][0]), 0x10u);
   EXPECT_EQ(std::to_integer<unsigned>(segs[1][0]), 0x20u);
}

// --- range-for で全バイトを走査できる ------------------------------------
TEST_F(SegmentedSpanTest, RangeForIteratesAllBytesInOrder) {
   auto seg = make_seg();
   std::vector<unsigned> values;
   for (auto b : seg) {
      values.push_back(std::to_integer<unsigned>(b));
   }
   ASSERT_EQ(values.size(), 8u);
   EXPECT_EQ(values[0], 0x10u);
   EXPECT_EQ(values[3], 0x13u);
   EXPECT_EQ(values[4], 0x20u);
   EXPECT_EQ(values[7], 0x23u);
}

// --- begin/end の距離が size() と一致する --------------------------------
TEST_F(SegmentedSpanTest, BeginEndDistanceEqualsSize) {
   auto seg = make_seg();
   auto const dist = seg.end() - seg.begin();
   EXPECT_EQ(dist, static_cast<std::ptrdiff_t>(seg.size()));
}

// --- イテレータ算術: +n, -n, +=, -= --------------------------------------
TEST_F(SegmentedSpanTest, IteratorArithmeticWorksCorrectly) {
   auto seg = make_seg();

   // begin() + 5 はユニット1の2バイト目 (0x21) を指す
   auto it_plus = seg.begin() + 5;
   EXPECT_EQ(std::to_integer<unsigned>(*it_plus), 0x21u);

   // end() - 1 は最終バイト (0x23) を指す
   auto it_last = seg.end() - 1;
   EXPECT_EQ(std::to_integer<unsigned>(*it_last), 0x23u);

   // += でインプレース前進
   auto it = seg.begin();
   it += 4;
   EXPECT_EQ(std::to_integer<unsigned>(*it), 0x20u);

   // -= でインプレース後退
   it -= 1;
   EXPECT_EQ(std::to_integer<unsigned>(*it), 0x13u);
}

// --- イテレータ operator[] (ランダムアクセス) ----------------------------
TEST_F(SegmentedSpanTest, IteratorSubscriptOperator) {
   auto seg = make_seg();
   auto it = seg.begin();
   // it[6] は begin() + 6 の要素 (0x22) を返す
   EXPECT_EQ(std::to_integer<unsigned>(it[6]), 0x22u);
   // 非先頭イテレータからの相対アクセス
   auto it2 = seg.begin() + 2;
   // it2[2] = *(it2+2) = *(begin()+4) → ユニット1の先頭 = 0x20
   EXPECT_EQ(std::to_integer<unsigned>(it2[2]), 0x20u);
}

// --- 後置 ++ / -- ---------------------------------------------------------
TEST_F(SegmentedSpanTest, IteratorPostIncrementAndDecrement) {
   auto seg = make_seg();
   auto it = seg.begin();
   // 後置 ++ は元の値を返し、it を前進させる
   auto prev = it++;
   EXPECT_EQ(std::to_integer<unsigned>(*prev), 0x10u);
   EXPECT_EQ(std::to_integer<unsigned>(*it), 0x11u);
   // 後置 -- は元の値を返し、it を後退させる
   auto cur = it--;
   EXPECT_EQ(std::to_integer<unsigned>(*cur), 0x11u);
   EXPECT_EQ(std::to_integer<unsigned>(*it), 0x10u);
}

// --- イテレータ比較演算 ---------------------------------------------------
TEST_F(SegmentedSpanTest, IteratorComparisonOperators) {
   auto seg = make_seg();
   auto b = seg.begin();
   auto e = seg.end();
   auto m = seg.begin() + 4; // 中間点

   EXPECT_TRUE(b == b);
   EXPECT_FALSE(b == e);
   EXPECT_TRUE(b != e);
   EXPECT_TRUE(b < e);
   EXPECT_FALSE(e < b);
   EXPECT_TRUE(b <= b);
   EXPECT_TRUE(b <= e);
   EXPECT_TRUE(e > b);
   EXPECT_TRUE(e >= e);
   EXPECT_TRUE(b < m);
   EXPECT_TRUE(m < e);
}

// --- const イテレータ (cbegin/cend) で読み取りアクセス -------------------
TEST_F(SegmentedSpanTest, ConstIteratorReadAccess) {
   auto seg = make_seg();
   auto const &cseg = seg;
   std::size_t count = 0;
   for (auto it = cseg.cbegin(); it != cseg.cend(); ++it) {
      ++count;
      static_cast<void>(std::to_integer<unsigned>(*it));
   }
   EXPECT_EQ(count, 8u);
}

// --- non-const から const イテレータへの変換 ----------------------------
TEST_F(SegmentedSpanTest, NonConstIteratorConvertsToConstIterator) {
   auto seg = make_seg();
   maboroutu::segmented_span<std::byte, 4>::iterator it = seg.begin();
   maboroutu::segmented_span<std::byte, 4>::const_iterator cit = it;
   EXPECT_EQ(std::to_integer<unsigned>(*cit), 0x10u);
}

#if defined(GTEST_HAS_DEATH_TEST) && !defined(NDEBUG)
// --- operator[] OOB はデバッグビルドで assert を発火させる ---------------
// NOTE: at() は throw で検出するが、operator[] は assert（UB 寄りの
// 事前条件チェック）であり、デバッグビルド前提。
TEST(MaboroutuSegmentedSpanDeathTest,
     OperatorBracketOutOfRangeTriggersAssert) {
   std::array<std::byte, 4> u0{};
   std::array<std::span<std::byte, 4>, 1> spans{std::span<std::byte, 4>{u0}};
   maboroutu::segmented_span<std::byte, 4> seg{
       std::span<std::span<std::byte, 4>>{spans}};
   ASSERT_EQ(seg.size(), 4u);
   EXPECT_DEATH({ static_cast<void>(seg[4]); }, "");
}
#endif // GTEST_HAS_DEATH_TEST && !NDEBUG

} // namespace
