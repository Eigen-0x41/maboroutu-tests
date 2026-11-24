// maboroutu.link_node のコンセプトのテスト。
// NOTE: link_node_traits の insert/erase 系は現状のソースではインスタンス化
// できないため（0.0.3.10）、ここではコンセプトのみ検証する。
#include <cstddef>

#include <gtest/gtest.h>

import maboroutu.link_node;

namespace {

using namespace maboroutu;

struct bi_node {
   bi_node *next = nullptr;
   bi_node *prev = nullptr;
};
struct fw_node {
   fw_node *next = nullptr;
};
struct no_next {
   bi_node *prev = nullptr;
};
struct wrong_next_type {
   int next = 0;
};
struct other_node_next {
   bi_node *next = nullptr;
};
struct const_next {
   const_next const *next = nullptr;
};
struct bi_node_wrong_prev {
   bi_node_wrong_prev *next = nullptr;
   int prev = 0;
};

static_assert(link_nodable<bi_node, bi_node>);
static_assert(link_nodable<fw_node, fw_node>);
static_assert(!link_nodable<no_next, bi_node>);
static_assert(!link_nodable<wrong_next_type, wrong_next_type>);
static_assert(!link_nodable<other_node_next, fw_node>);
static_assert(link_nodable<other_node_next, bi_node>);
static_assert(!link_nodable<const_next, const_next>);

static_assert(bidirectional_link_nodable<bi_node, bi_node>);
static_assert(!bidirectional_link_nodable<fw_node, fw_node>);
static_assert(!bidirectional_link_nodable<bi_node_wrong_prev, bi_node_wrong_prev>);

static_assert(forward_link_nodable<fw_node, fw_node>);
static_assert(!forward_link_nodable<bi_node, bi_node>);
static_assert(!forward_link_nodable<wrong_next_type, wrong_next_type>);

} // namespace

TEST(MaboroutuLinkNode, Concepts) {
   // 検証は上記のstatic_assertで完結する。
   SUCCEED();
}
