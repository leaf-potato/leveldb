// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_ARENA_H_
#define STORAGE_LEVELDB_UTIL_ARENA_H_

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace leveldb {

class Arena {
public:
    Arena();

    // 管理内存资源, 不能拷贝, 避免出现内存泄露
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    ~Arena();

    // Return a pointer to a newly allocated memory block of "bytes" bytes.
    /**
     * 返回指向新分配内存地址的指针(分配新内存)
     * 1. 优先从当前内存块分配内存
     * 2. 当前内存块不够了, 申请新的内存块进行分配
     */
    char* Allocate(size_t bytes);

    // Allocate memory with the normal alignment guarantees provided by malloc.
    // 使用malloc内存对齐的方式分配内存
    char* AllocateAligned(size_t bytes);

    // Returns an estimate of the total memory usage of data allocated
    // by the arena.
    // 返回当前Arena数据分配的内存使用量, 使用memory_order_relaxed方式获取变量值
    size_t MemoryUsage() const {
        return memory_usage_.load(std::memory_order_relaxed);
    }

private:
    char* AllocateFallback(size_t bytes); // 回退分配

    /**
     * 指定大小分配新的内存块, 内存块大小由外层计算后直接给定
     */ 
    char* AllocateNewBlock(size_t block_bytes);

    // Allocation state
    // 记录当前的分配状态
    char* alloc_ptr_;               // 当前未分配的地址
    size_t alloc_bytes_remaining_;  // 当前内存块的空闲空间

    // Array of new[] allocated memory blocks
    // new出来的多个不固定大小的内存块
    std::vector<char*> blocks_;

    // Total memory usage of the arena.
    //
    // TODO(costan): This member is accessed via atomics, but the others are
    //               accessed without any locking. Is this OK?
    /**
     * Arena总共已使用的内存(向系统new申请的)
     * TODO: 这个成员是通过原子变量访问, 但其他成员是在没有任何锁的情况下访问的, 是否有问题？
     */
    std::atomic<size_t> memory_usage_;
};

inline char* Arena::Allocate(size_t bytes) {
    // The semantics of what to return are a bit messy if we allow
    // 0-byte allocations, so we disallow them here (we don't need
    // them for our internal use).
    /**
     * 如果我们允许0字节分配, 返回内容的语义会优点混乱, 所以在这儿我们不允许
     * TODO: 如果bytes为0, 直接返回nullptr会有问题吗？
     */
    assert(bytes > 0);
    if (bytes <= alloc_bytes_remaining_) { // 当前内存块的空间还够用, 直接返回
        char* result = alloc_ptr_;
        alloc_ptr_ += bytes;
        alloc_bytes_remaining_ -= bytes;
        return result;
    }
    return AllocateFallback(bytes); // 新申请内存块返回
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_ARENA_H_
