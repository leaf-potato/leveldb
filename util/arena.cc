// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/arena.h"

namespace leveldb {

static const int kBlockSize = 4096; // 内存块大小为4KB

Arena::Arena()
    : alloc_ptr_(nullptr), alloc_bytes_remaining_(0), memory_usage_(0) {}

Arena::~Arena() {
    // 析构函数释放内存
    for (size_t i = 0; i < blocks_.size(); i++) {
        delete[] blocks_[i];
    }
}

char* Arena::AllocateFallback(size_t bytes) {
    if (bytes > kBlockSize / 4) {
        // Object is more than a quarter of our block size.  Allocate it
        // separately to avoid wasting too much space in leftover bytes.
        /**
         * 如果对象超过1/4块的大小, 单独分配避免浪费在当前块中浪费太多的内存空间
         * bytes > kBlockSize/4 说明alloc_bytes_remaining_ <= kBlockSize/4
         * 如果申请新的内存块, 那么将浪费至多kBlockSize/4的内存空间
         */
        char* result = AllocateNewBlock(bytes);
        return result;
    }

    // We waste the remaining space in the current block.
    // 直接舍弃当前块中的内存空间, 使用新的内存块
    alloc_ptr_ = AllocateNewBlock(kBlockSize);
    alloc_bytes_remaining_ = kBlockSize;

    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
}

char* Arena::AllocateAligned(size_t bytes) {
    // 对其字节, 通过sizeof(void*)判断机器的字节数, 至少使用8字节对齐
    const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
    
    // 对齐字节必须是2的指数(静态断言), 这样也能保证后续取模运算符的正确性
    static_assert((align & (align - 1)) == 0,
                  "Pointer size should be a power of 2");

    // 计算对齐需要的内存空间, uintptr_t与机器相关的指针类型
    size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align - 1); // 取模数
    size_t slop = (current_mod == 0 ? 0 : align - current_mod);

    size_t needed = bytes + slop; // 总内存空间: bytes + 对齐的内存空间
    char* result;

    if (needed <= alloc_bytes_remaining_) {
        result = alloc_ptr_ + slop; // 返回地址需要是对齐

        alloc_ptr_ += needed;
        alloc_bytes_remaining_ -= needed;
    } else {
        // AllocateFallback always returned aligned memory
        // AllocateFallback总是返回对齐的内存, 因为总是新分配出来的内存
        result = AllocateFallback(bytes);
    }

    // 断言地址是否对齐的, 模数为0
    assert((reinterpret_cast<uintptr_t>(result) & (align - 1)) == 0);
    return result;
}

char* Arena::AllocateNewBlock(size_t block_bytes) {
    /**
     * TODO: 不需要判空, 直接抛异常是否会有问题？
     */
    char* result = new char[block_bytes]; // 
    blocks_.push_back(result);

    /**
     * 更新已申请的内存空间, 至于这儿加上sizeof(char*)原因:
     * 申请内存的地址push到vector中, vector需要new出来的空间存储char*
     * 使用memory_order_relaxed方式存储值
     */
    memory_usage_.fetch_add(block_bytes + sizeof(char*),
                            std::memory_order_relaxed);
    return result;
}

}  // namespace leveldb
