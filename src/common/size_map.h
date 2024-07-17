#ifndef MEMORYPOOL_SIZE_MAP_H
#define MEMORYPOOL_SIZE_MAP_H
#include <mutex>
#include <cassert>
#include "size_classes.h"

class SizeMap {
public:
    constexpr static size_t size_class_size = 85;
    constexpr static size_t max_move = 128;
private:
    constexpr static size_t _max_size = 262144;
    constexpr static size_t _large_size = 1024;
    constexpr static size_t _large_size_alignment = 128;
    constexpr static size_t _n = ((_max_size + _large_size_alignment - 1) >> 7) + 121;
    static size_t _size_class[_n];
public:
    // 返回任意字节对应的 size class，要求字节数不超过 max_size
    static size_t get_size_class(size_t n) {
        static std::once_flag flag;
        std::call_once(flag, []() {
            for(int i = 0, j = 0; i < size_class_size - 1; ++i) {
                for(unsigned int mx = size_classes[i].size; j <= mx; j += sizeof(void *)) {
                    _size_class[align(j)] = i;
                }
            }
        });
        return _size_class[align(n)];
    }

    // size_class 对应的 object 字节大小
    static size_t size(size_t size_class) {
        return size_classes[size_class].size;
    }

    // size_class 一次性申请/返还多少个 object，不会超过 max_move
    static size_t num_to_move(size_t size_class) {
        return size_classes[size_class].num_to_move;
    }

    // size_class 类型，一次申请多少页的 span 来切割成 objects
    static size_t pages(size_t size_class) {
        return size_classes[size_class].pages;
    }

    static size_t max_capacity(size_t size_class) {
        return size_classes[size_class].max_capacity;
    }
private:
    static size_t align(size_t n);
};

size_t SizeMap::align(size_t n) {
    if(n <= _large_size) {
        return (n + 7) >> 3;
    }
    if(n <= _max_size) {
        return (n + _large_size_alignment - 1 + (120 << 7)) >> 7;
    }

    assert(n <= _max_size && "larger than max thread cache size");
    return -1;
}

#endif //MEMORYPOOL_SIZE_MAP_H
