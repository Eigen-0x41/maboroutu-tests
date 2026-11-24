// maboroutu.ilink_node のコンセプトのテスト。
// NOTE: ilink_node_traits の insert/erase 系は現状のソースではインスタンス化
// できないため（0.0.3.10）、ここではコンセプトのみ検証する。
#include <cstddef>

#include <gtest/gtest.h>

import maboroutu.ilink_node;

namespace {

using namespace maboroutu;

struct bi_inode {
   std::size_t next = 0;
   std::size_t prev = 0;
};
struct fw_inode {
   std::size_t next = 0;
};
struct no_next {
   std::size_t prev = 0;
};
struct int_next {
   int next = 0;
};
struct u32_next {
   unsigned next = 0;
};
struct bi_inode_wrong_prev {
   std::size_t next = 0;
   int prev = 0;
};

static_assert(ilink_nodable<bi_inode>);
static_assert(ilink_nodable<fw_inode>);
static_assert(!ilink_nodable<no_next>);
static_assert(!ilink_nodable<int_next>);
static_assert(!ilink_nodable<u32_next>);

static_assert(bidirectional_ilink_nodable<bi_inode>);
static_assert(!bidirectional_ilink_nodable<fw_inode>);
static_assert(!bidirectional_ilink_nodable<bi_inode_wrong_prev>);

static_assert(forward_ilink_nodable<fw_inode>);
static_assert(!forward_ilink_nodable<bi_inode>);
static_assert(!forward_ilink_nodable<int_next>);

} // namespace

TEST(MaboroutuIlinkNode, Concepts) {
   // 検証は上記のstatic_assertで完結する。
   SUCCEED();
}
