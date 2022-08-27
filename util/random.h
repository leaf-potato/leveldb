// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_RANDOM_H_
#define STORAGE_LEVELDB_UTIL_RANDOM_H_

#include <cstdint>

namespace leveldb {

// A very simple random number generator.  Not especially good at
// generating truly random bits, but good enough for our needs in this
// package.
/**
 * 一个非常简单的随机数生成器, 不擅长生成真随机数(伪随机数)但也足够满足当前的需求
 * 使用Lehmer random number generator随机数生成算法, 计算公式如下:
 * X(i+1) = (A * X(i)) % M
 * MINSTD参数设置比较通用: M=2^31-1(梅森素数) A=16807(2^31-1的原根模)
 * 参考维基百科: https://en.wikipedia.org/wiki/Lehmer_random_number_generator
 */
class Random {
private:
    uint32_t seed_; // 随机数种子

public:
    /**
     * 构造函数确认随机数种子, 随机数的生成顺序与种子相关
     * 0x7fffffffu 表示32位除最高位其余都为1的无符号整数
     * TODO: 为什么要排除最高位？
     * Answer: 采用MINSTD参数的Lehmer RNG算法要求
     * 
     * 避免不合法的种子: 根据算法要求seed_是[1, M-1]范围内的数, 因此seed_不能为0和M, 需要排除掉
     * 反证法:
     * 1. 当seed_为0时, 生成的随机数总是0
     * 2. 当seed_为M时, X(1) = (A * X(0)) % M = 0 生成的随机数也总是0
     */
    explicit Random(uint32_t s) : seed_(s & 0x7fffffffu) {
        // Avoid bad seeds.
        if (seed_ == 0 || seed_ == 2147483647L) {
            seed_ = 1;
        }
    }

    uint32_t Next() { // 认为返回的数是随机的
        static const uint32_t M = 2147483647L;  // 2^31-1
        static const uint64_t A = 16807;        // bits 14, 8, 7, 5, 2, 1, 0

        // We are computing
        //       seed_ = (seed_ * A) % M,    where M = 2^31-1
        //
        // seed_ must not be zero or M, or else all subsequent computed values
        // will be zero or M respectively.  For all other values, seed_ will end
        // up cycling through every number in [1,M-1]

        /**
         * 我们将进行如下计算:
         *      seed_ = (seed_ * A) % M
         * seed_不能为0或者M, 否则后续所有计算的值将分别为0或者M
         * 对于所有其他值, seed_最终将循环遍历[1, M-1]中的每个数字
         */
        uint64_t product = seed_ * A; // seed_最大为 2^31 - 2, seed_ * A会超过uint32_t的表示范围

        // Compute (product % M) using the fact that ((x << 31) % M) == x.
        /**
         * 使用位运算避免64位的除法运算, 因为有如下公式成立:
         * (x << 31) % M = (x * (M + 1)) % M
         *               = (x * M ) % M + x % M
         *               = 0 + x % M
         *               = x % M
         * 所以:
         * proudct % M = ((product >> 31) << 31 + (product & M)) % M
         *             = (((product >> 31) << 31) % M + (product & M) % M) % M // 运算符展开
         *             = ((product >> 31) % M  + (product & M) % M) % M        // 公式替换简化
         *             = ((product >> 31) + (product & M)) % M
         * 
         * product >> 31 最大值为 ((2^31 - 2) * 16807) >> 31 远小于 2^31
         * product & M 最大值为 M => 2^31 - 2
         * 因此uint32能存下 (product >> 31) + (product & M)的值
         */
        seed_ = static_cast<uint32_t>((product >> 31) + (product & M));

        // The first reduction may overflow by 1 bit, so we may need to
        // repeat.  mod == M is not possible; using > allows the faster
        // sign-bit-based test.
        /**
         * product >> 31 小于 M 并且 product & M 小于 M 
         * 但求和之后可能大于M, 因此需要再进行一次取模, 直接使用 > 判断即可(M为质数, seed_不可能为M)
         * 或者: seed_ = (seed_ >> 31) + (seed_ & M)
         */
        if (seed_ > M) {
            seed_ -= M;
        }
        return seed_;
    }

    // Returns a uniformly distributed value in the range [0..n-1]
    // REQUIRES: n > 0
    /**
     * 返回[0...n)范围内的均匀分布值(即[0...n)的值以1/n的概率返回)
     * 要求: n > 0 n是int类型, 肯定在M范围内
     * TODO: 是否应该加个assert, assert(n > 0)
     *
     * range为int32_t而非uint32_t类型的原因:
     * 随机数能生成的最大值为M, int32_t能表示的最大正数为M 刚好range <= M
     * 如果range为uint32_t类型, 那么range可能大于M, 不能保证[0...range]范围内的数都是均匀的
     */
    uint32_t Uniform(int n) { return Next() % n; }

    // Randomly returns true ~"1/n" of the time, and false otherwise.
    // REQUIRES: n > 0
    /**
     * 1/n的概率返回true, 其余情况返回false
     * 要求: n > 0
     *
     * range为int32_t而非uint32_t类型的原因同上
     * TODO: 是否应该加个assert, assert(n > 0)
     */
    bool OneIn(int n) { return (Next() % n) == 0; }

    // Skewed: pick "base" uniformly from range [0,max_log] and then
    // return "base" random bits.  The effect is to pick a number in the
    // range [0,2^max_log-1] with exponential bias towards smaller numbers.
    /**
     * 偏态分布: 从[0, max_log]中随机选择"base" 然后随机返回[0, 2^base)内的数值
     * 效果是随机返回[0, 2^max_log)范围里的数值, 概率偏向较小的数字(2^0 => 2^max_log-1生成概率依次降低)
     * 举例: max_log为3, 那么随机生成7的概率为1/8 * 1/4 = 1/32
     * 生成0的概率为: 1/4 * (1 + 1/2 + 1/4 + 1/8) = 15/32
     *
     * 因为2^max_log值必须在2^31-1范围内, 因此max_log最大取值30
     * TODO: 是否应该加个assert, assert(max_log < 31)
     */
    uint32_t Skewed(int max_log) { return Uniform(1 << Uniform(max_log + 1)); }
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_RANDOM_H_
