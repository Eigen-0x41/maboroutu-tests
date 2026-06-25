#include <algorithm>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <ios>
#include <print>
#include <sstream>
#include <vector>

import maboroutu.binarystream;

int main() {
  std::println("test run: binarystream");

  using test1_type = uint32_t;
  using test2_type = uint32_t;
  using test3_type = uint16_t;

  const test1_type test1 = 0x1234;
  const std::array<test2_type, 2> test2{0x1234, 0x5678};
  const std::vector<test3_type> test3{
      0x1234,
      0x5678,
      0x9012,
  };

  {
    std::stringstream sst(std::ios_base::in | std::ios_base::out |
                          std::ios_base::binary);

    {
      maboroutu::obinarystream obst(sst.rdbuf());
      obst.write<std::endian::big>(test1);
      obst.write_array<std::endian::big>(test2);
      obst.write_vector<std::endian::big>(test3);
    }

    constexpr std::array<char, sizeof(uint32_t) + (sizeof(uint32_t) * 2) +
                                   (sizeof(uint16_t) * 3)>
        ref_1{'\x00', '\x00', '\x12', '\x34', '\x00', '\x00',
              '\x12', '\x34', '\x00', '\x00', '\x56', '\x78',
              '\x12', '\x34', '\x56', '\x78', '\x90', '\x12'};
    std::array<char, sizeof(size_t) * (1 + 2 + 3)> bin{};
    sst.sync();
    sst.read(bin.data(), bin.size());
    for (auto i = 0; i < std::min(ref_1.size(), bin.size()); i++) {
      EXPECT_EQ(ref_1[i], bin[i]);
    }

    maboroutu::ibinarystream ibst(sst.rdbuf());
    ibst.seekg(0, std::ios_base::beg);
    auto test1_r = ibst.read<std::endian::big, test1_type>();
    EXPECT_EQ(test1, test1_r);
    auto test2_r = ibst.read_array<std::endian::big, test2_type, 2>();
    for (auto i = 0; i < std::min(test2.size(), test2_r.size()); i++) {
      EXPECT_EQ(test2[i], test2_r[i]);
    }
    auto test3_r = ibst.read_vector<std::endian::big, test3_type>(3);
    for (auto i = 0; i < std::min(test3.size(), test3_r.size()); i++) {
      EXPECT_EQ(test3[i], test3_r[i]);
    }
  }

  {
    std::stringstream sst(std::ios_base::in | std::ios_base::out |
                          std::ios_base::binary);

    {
      maboroutu::obinarystream obst(sst.rdbuf());
      obst.write<std::endian::little>(test1);
      obst.write_array<std::endian::little>(test2);
      obst.write_vector<std::endian::little>(test3);
    }

    constexpr std::array<char, sizeof(uint32_t) + (sizeof(uint32_t) * 2) +
                                   (sizeof(uint16_t) * 3)>
        ref_1{'\x34', '\x12', '\x00', '\x00', '\x34', '\x12',
              '\x00', '\x00', '\x78', '\x56', '\x00', '\x00',
              '\x34', '\x12', '\x78', '\x56', '\x12', '\x90'};
    std::array<char, sizeof(size_t) * (1 + 2 + 3)> bin{};
    sst.sync();
    sst.read(bin.data(), bin.size());
    for (auto i = 0; i < std::min(ref_1.size(), bin.size()); i++) {
      EXPECT_EQ(ref_1[i], bin[i]);
    }

    maboroutu::ibinarystream ibst(sst.rdbuf());
    ibst.seekg(0, std::ios_base::beg);
    auto test1_r = ibst.read<std::endian::little, test1_type>();
    EXPECT_EQ(test1, test1_r);
    auto test2_r = ibst.read_array<std::endian::little, test2_type, 2>();
    for (auto i = 0; i < std::min(test2.size(), test2_r.size()); i++) {
      EXPECT_EQ(test2[i], test2_r[i]);
    }
    auto test3_r = ibst.read_vector<std::endian::little, test3_type>(3);
    for (auto i = 0; i < std::min(test3.size(), test3_r.size()); i++) {
      EXPECT_EQ(test3[i], test3_r[i]);
    }
  }

  {
    std::stringstream sst(std::ios_base::in | std::ios_base::out |
                          std::ios_base::binary);

    maboroutu::iobinarystream iobst(sst.rdbuf());
    {
      iobst.write<std::endian::big>(test1);
      iobst.write_array<std::endian::big>(test2);
      iobst.write_vector<std::endian::big>(test3);
    }

    constexpr std::array<char, sizeof(uint32_t) + (sizeof(uint32_t) * 2) +
                                   (sizeof(uint16_t) * 3)>
        ref_1{'\x00', '\x00', '\x12', '\x34', '\x00', '\x00',
              '\x12', '\x34', '\x00', '\x00', '\x56', '\x78',
              '\x12', '\x34', '\x56', '\x78', '\x90', '\x12'};
    std::array<char, sizeof(size_t) * (1 + 2 + 3)> bin{};
    sst.sync();
    sst.read(bin.data(), bin.size());
    for (auto i = 0; i < std::min(ref_1.size(), bin.size()); i++) {
      EXPECT_EQ(ref_1[i], bin[i]);
    }

    iobst.seekg(0, std::ios_base::beg);
    auto test1_r = iobst.read<std::endian::big, test1_type>();
    EXPECT_EQ(test1, test1_r);
    auto test2_r = iobst.read_array<std::endian::big, test2_type, 2>();
    for (auto i = 0; i < std::min(test2.size(), test2_r.size()); i++) {
      EXPECT_EQ(test2[i], test2_r[i]);
    }
    auto test3_r = iobst.read_vector<std::endian::big, test3_type>(3);
    for (auto i = 0; i < std::min(test3.size(), test3_r.size()); i++) {
      EXPECT_EQ(test3[i], test3_r[i]);
    }
  }

  {
    std::stringstream sst(std::ios_base::in | std::ios_base::out |
                          std::ios_base::binary);

    maboroutu::iobinarystream iobst(sst.rdbuf());
    {
      iobst.write<std::endian::little>(test1);
      iobst.write_array<std::endian::little>(test2);
      iobst.write_vector<std::endian::little>(test3);
    }

    constexpr std::array<char, sizeof(uint32_t) + (sizeof(uint32_t) * 2) +
                                   (sizeof(uint16_t) * 3)>
        ref_1{'\x34', '\x12', '\x00', '\x00', '\x34', '\x12',
              '\x00', '\x00', '\x78', '\x56', '\x00', '\x00',
              '\x34', '\x12', '\x78', '\x56', '\x12', '\x90'};
    std::array<char, sizeof(size_t) * (1 + 2 + 3)> bin{};
    sst.sync();
    sst.read(bin.data(), bin.size());
    for (auto i = 0; i < std::min(ref_1.size(), bin.size()); i++) {
      EXPECT_EQ(ref_1[i], bin[i]);
    }

    iobst.seekg(0, std::ios_base::beg);
    auto test1_r = iobst.read<std::endian::little, test1_type>();
    EXPECT_EQ(test1, test1_r);
    auto test2_r = iobst.read_array<std::endian::little, test2_type, 2>();
    for (auto i = 0; i < std::min(test2.size(), test2_r.size()); i++) {
      EXPECT_EQ(test2[i], test2_r[i]);
    }
    auto test3_r = iobst.read_vector<std::endian::little, test3_type>(3);
    for (auto i = 0; i < std::min(test3.size(), test3_r.size()); i++) {
      EXPECT_EQ(test3[i], test3_r[i]);
    }
  }

  return 0;
}
