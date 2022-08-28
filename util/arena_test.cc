// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/arena.h"

#include "gtest/gtest.h"
#include "util/random.h"

namespace leveldb {

TEST(ArenaTest, Empty) { Arena arena; }

TEST(ArenaTest, Simple) {
    std::vector<std::pair<size_t, char*>> allocated;
    Arena arena;

    const int N = 100000;
    size_t bytes = 0; // 计数器
    Random rnd(301); // 使用301随机数种子初始化Random

    for (int i = 0; i < N; i++) {
        size_t s; // 随机指定需要分配的内存空间
        if (i % (N / 10) == 0) { // 对10000取模为0(10000、20000...100000), 1/10的概率 >= 10000
            s = i;
        } else {
            /**
             * 1/4000的概率在6000范围中随机均匀生成1个数
             * 3999/4000的概率如下生成随机数:
             * 1. 1/10的概率在100范围内生成1个数
             * 2. 9/10的概率在20范围内生成1个数
             */
            s = rnd.OneIn(4000)
                    ? rnd.Uniform(6000)
                    : (rnd.OneIn(10) ? rnd.Uniform(100) : rnd.Uniform(20));
        }

        if (s == 0) { // 不允许分配大小为0的空间
            // Our arena disallows size 0 allocations.
            s = 1;
        }

        char* r;
        if (rnd.OneIn(10)) { // 1/10的概率对齐分配
            r = arena.AllocateAligned(s);
        } else { // 9/10的概率不对齐分配
            r = arena.Allocate(s);
        }

        for (size_t b = 0; b < s; b++) { // 对分配的内存进行填充
            // Fill the "i"th allocation with a known bit pattern
            r[b] = i % 256;
        }

        bytes += s; // 计数已经分配的内存
        allocated.push_back(std::make_pair(s, r));

        ASSERT_GE(arena.MemoryUsage(), bytes); // 对齐/舍弃 new的内存空间肯定 >= 申请的内存空间

        if (i > N / 10) { // 内存的浪费率不能高于0.1
            ASSERT_LE(arena.MemoryUsage(), bytes * 1.10);
        }
    }

    for (size_t i = 0; i < allocated.size(); i++) { // 遍历已经分配的内存空间
        size_t num_bytes = allocated[i].first;
        const char* p = allocated[i].second;
        for (size_t b = 0; b < num_bytes; b++) {
            // Check the "i"th allocation for the known bit pattern
            // 检验填充的内容是否正确
            ASSERT_EQ(int(p[b]) & 0xff, i % 256);
        }
    }
}

}  // namespace leveldb

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
