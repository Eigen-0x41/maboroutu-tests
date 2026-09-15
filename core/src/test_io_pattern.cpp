// mylib.io_pattern (maboroutu.io_pattern) のテスト。library_spec.md 4.5節。
//
// reader/writer/mapper は read_from/write_to/to_domain という「修飾なし
// 呼び出し」をrequires式内で要求するため、これらの自由関数はADL
// （引数依存の名前探索）で発見される必要がある。application_spec.md
// 2.1節・2.2節・4章・12章の方針（read_from/write_to/to_domainを
// Raw系の型と同一名前空間に配置する）にならい、本テストでも
// Raw型・Source型等と同一名前空間に自由関数を定義する。
#include <gtest/gtest.h>

import maboroutu.io_pattern;

namespace demo_raw {

// Raw層を模した最小限の型群。
struct raw_entry {
   int value = 0;
};
struct domain_entry {
   int value = 0;
};
struct source_stub {};
struct destination_stub {
   int last_written = 0;
};

// ADLで発見されるよう、raw_entry/source_stub等と同一名前空間に配置する。
[[nodiscard]] auto read_from(source_stub & /*src*/)
    -> maboroutu::io_pattern_result<raw_entry> {
   return raw_entry{.value = 42};
}

[[nodiscard]] auto write_to(raw_entry const &raw, destination_stub &dst)
    -> maboroutu::io_pattern_result<void> {
   dst.last_written = raw.value;
   return {};
}

[[nodiscard]] auto to_domain(raw_entry const &raw)
    -> maboroutu::io_pattern_result<domain_entry> {
   return domain_entry{.value = raw.value};
}

} // namespace demo_raw

namespace {

static_assert(
    maboroutu::reader<demo_raw::raw_entry, demo_raw::source_stub>);
static_assert(
    maboroutu::writer<demo_raw::raw_entry, demo_raw::destination_stub>);
static_assert(
    maboroutu::mapper<demo_raw::raw_entry, demo_raw::domain_entry>);

TEST(MaboroutuIoPattern, ReadFromIsFoundViaAdlAndReturnsRaw) {
   demo_raw::source_stub src;
   auto raw = read_from(src); // 修飾なし呼び出し。ADLでdemo_raw内を検索。
   ASSERT_TRUE(raw.has_value());
   EXPECT_EQ(raw->value, 42);
}

TEST(MaboroutuIoPattern, WriteToIsFoundViaAdlAndForwardsValue) {
   demo_raw::raw_entry raw{.value = 7};
   demo_raw::destination_stub dst;
   auto result = write_to(raw, dst);
   EXPECT_TRUE(result.has_value());
   EXPECT_EQ(dst.last_written, 7);
}

TEST(MaboroutuIoPattern, ToDomainIsFoundViaAdlAndConvertsRawToDomain) {
   demo_raw::raw_entry raw{.value = 9};
   auto domain = to_domain(raw);
   ASSERT_TRUE(domain.has_value());
   EXPECT_EQ(domain->value, 9);
}

TEST(MaboroutuIoPattern, EndToEndReadWriteMapPipeline) {
   // 2.1節・2.2節・4章の使用例（read_from -> to_domain / write_to）を
   // ひとつなぎにした最小のパイプライン検証。
   demo_raw::source_stub src;
   auto raw = read_from(src);
   ASSERT_TRUE(raw.has_value());

   auto domain = to_domain(*raw);
   ASSERT_TRUE(domain.has_value());
   EXPECT_EQ(domain->value, raw->value);

   demo_raw::destination_stub dst;
   auto write_result = write_to(*raw, dst);
   ASSERT_TRUE(write_result.has_value());
   EXPECT_EQ(dst.last_written, raw->value);
}

} // namespace
