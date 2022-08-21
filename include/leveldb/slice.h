// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Slice is a simple structure containing a pointer into some external
// storage and a size.  The user of a Slice must ensure that the slice
// is not used after the corresponding external storage has been
// deallocated.
//
// Multiple threads can invoke const methods on a Slice without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Slice must use
// external synchronization.

#ifndef STORAGE_LEVELDB_INCLUDE_SLICE_H_
#define STORAGE_LEVELDB_INCLUDE_SLICE_H_

#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>

#include "leveldb/export.h"

namespace leveldb {

class LEVELDB_EXPORT Slice {
public:
    /**
     * Slice兼容C++中三种字符串表示方式进行构造:
     * 1. 首地址 + 长度: 能表示任意二进制字节数据
     * 2. C++标准库中的string字符串类
     * 3. C式字符串: 以'\0'结尾, 非二进制安全
     */
    // Create an empty slice.
    Slice() : data_(""), size_(0) {}

    // Create a slice that refers to d[0,n-1].
    // 创建空的Slice, 此时data_的值并非赋值为nullptr
    Slice(const char* d, size_t n) : data_(d), size_(n) {}

    /**
     * TODO: 使用explict避免隐式转换是否更好, 显示转换后赋值 
     */
    // Create a slice that refers to the contents of "s"
    Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}

    // Create a slice that refers to s[0,strlen(s)-1]
    Slice(const char* s) : data_(s), size_(strlen(s)) {}

    // Intentionally copyable. 默认拷贝和赋值
    Slice(const Slice&) = default;
    Slice& operator=(const Slice&) = default;

    // Return a pointer to the beginning of the referenced data
    const char* data() const { return data_; }

    // Return the length (in bytes) of the referenced data
    size_t size() const { return size_; }

    // Return true iff the length of the referenced data is zero
    bool empty() const { return size_ == 0; }

    // Return the ith byte in the referenced data.
    // REQUIRES: n < size() 使用者保证
    char operator[](size_t n) const {
        assert(n < size());
        return data_[n];
    }

    // Change this slice to refer to an empty array
    void clear() {
        data_ = "";
        size_ = 0;
    }

    // Drop the first "n" bytes from this slice.
    void remove_prefix(size_t n) {
        assert(n <= size());    // 使用者保证
        data_ += n;
        size_ -= n;
    }

    // Return a string that contains the copy of the referenced data.
    std::string ToString() const { return std::string(data_, size_); }

    // Three-way comparison.  Returns value:
    //   <  0 iff "*this" <  "b",
    //   == 0 iff "*this" == "b",
    //   >  0 iff "*this" >  "b"
    int compare(const Slice& b) const;

    // Return true iff "x" is a prefix of "*this"
    bool starts_with(const Slice& x) const {
        return ((size_ >= x.size_) && (memcmp(data_, x.data_, x.size_) == 0));
    }

private:
    const char* data_;  // 数据的起始地址
    size_t size_;       // 数据的长度 
};

/**
 * 重载==和!=运算符函数, 非类的成员函数:
 * 1. 判断两个Slice是否相等: 二进制字节依次进行比较
 * 2. 判断两个Slice是否不等: 判断相等的结果取反
 * TODO: operator==和operator!=为什么不实现为类的成员函数
 */
inline bool operator==(const Slice& x, const Slice& y) {
    return ((x.size() == y.size()) &&
            (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const Slice& x, const Slice& y) { return !(x == y); }

/**
 * 对两个Slice按字典序进行比较:
 * 1. < 0 if "*this" < "b"
 * 2. == 0 if "*this" == "b"
 * 3. > 0 if "*this" > "b"
 */
inline int Slice::compare(const Slice& b) const {
    const size_t min_len = (size_ < b.size_) ? size_ : b.size_; // 先按公共长度进行比较
    int r = memcmp(data_, b.data_, min_len);
    if (r == 0) { // 公共内容相等, 按长度进行比较(长度相等延续memcmp的返回值)
        if (size_ < b.size_)
            r = -1;
        else if (size_ > b.size_)
            r = +1;
    }
    return r;
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_SLICE_H_
