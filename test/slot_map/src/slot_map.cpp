#include <cstdlib>
#include <deque>
#include <exception>
#include <gtest/gtest.h>
#include <print>
#include <stdexcept>
#include <string>

import maboroutu.slot_map;

enum class slot_map_key : size_t {};

int main() {
  std::println("test run: slot_map");

  try {
    std::deque<std::string> test{};
    maboroutu::slot_map<slot_map_key, std::string> a{};

    auto ind0 = a.insert(std::string("test0"));
    auto ind1 = a.insert(std::string("test1"));
    auto ind2 = a.emplace("test2");
    auto ind3 = a.emplace("test3");
    auto ind4 = a.insert(std::string("test4"));

    EXPECT_EQ(static_cast<size_t>(ind0), 0);
    EXPECT_EQ(static_cast<size_t>(ind1), 1);
    EXPECT_EQ(static_cast<size_t>(ind2), 2);
    EXPECT_EQ(static_cast<size_t>(ind3), 3);
    EXPECT_EQ(static_cast<size_t>(ind4), 4);

    EXPECT_EQ(a[ind0], "test0");
    EXPECT_EQ(a[ind1], "test1");
    EXPECT_EQ(a[ind2], "test2");
    EXPECT_EQ(a[ind3], "test3");
    EXPECT_EQ(a[ind4], "test4");

    EXPECT_EQ(a.size(), 5);

    a.erase(ind3);
    a.erase(ind1);

    EXPECT_EQ(a.size(), 3);

    EXPECT_TRUE(a.contains(ind0));
    EXPECT_FALSE(a.contains(ind1));
    EXPECT_TRUE(a.contains(ind2));
    EXPECT_FALSE(a.contains(ind3));
    EXPECT_TRUE(a.contains(ind4));

    EXPECT_EQ(a.at(ind0), "test0");
    EXPECT_THROW(a.at(ind1), std::out_of_range);
    EXPECT_EQ(a.at(ind2), "test2");
    EXPECT_THROW(a.at(ind3), std::out_of_range);
    EXPECT_EQ(a.at(ind4), "test4");

    auto ind5 = a.emplace("test5");
    a.erase(ind2);
    EXPECT_EQ(a.size(), 3);

    auto ind6 = a.insert(std::string("test6"));
    auto ind7 = a.insert(std::string("test7"));

    EXPECT_EQ(a.size(), 5);

    // 再利用される値は決まっていませんが、
    // 現在の実装だとこのようになるはず。
    EXPECT_EQ(ind5, ind1);
    EXPECT_EQ(ind6, ind2);
    EXPECT_EQ(ind7, ind3);

    EXPECT_TRUE(a.contains(ind0));
    EXPECT_TRUE(a.contains(ind1)); // 未定義動作
    EXPECT_TRUE(a.contains(ind2)); // 未定義動作
    EXPECT_TRUE(a.contains(ind3)); // 未定義動作
    EXPECT_TRUE(a.contains(ind4));
    EXPECT_TRUE(a.contains(ind5));
    EXPECT_TRUE(a.contains(ind6));
    EXPECT_TRUE(a.contains(ind7));

  } catch (std::exception ec) {
    std::println("error : {}", ec.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
