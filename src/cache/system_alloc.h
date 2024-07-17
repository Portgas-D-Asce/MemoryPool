#ifndef MEMORYPOOL_SYSTEM_ALLOC_H
#define MEMORYPOOL_SYSTEM_ALLOC_H
#include <utility>
#include <algorithm>
#include <sys/mman.h>
#include <spdlog/spdlog.h>
#include "size_classes.h"
#include "singleton.h"
#include "page.h"
#include "stats.h"

class SystemAlloc {
public:
    std::pair<void*, size_t> alloc(size_t n, size_t align);
    bool dealloc(void* ptr, size_t n);

    friend std::default_delete<SystemAlloc>;
    friend Singleton<SystemAlloc>;
    SystemAlloc(const SystemAlloc&) = delete;
    SystemAlloc(const SystemAlloc&&) = delete;
    SystemAlloc& operator=(const SystemAlloc&) = delete;
    SystemAlloc& operator=(const SystemAlloc&&) = delete;
private:
    SystemAlloc() = default;
    ~SystemAlloc();

    std::pair<void*, size_t> alloc_from_region(size_t n, size_t align);
    std::pair<void*, size_t> alloc_from_new_region(size_t n, size_t align);

    static void* mmap_align(size_t n, size_t align, bool flag = false);
    static size_t round_down(size_t n, size_t align) { return n & ~(align - 1); }
    static size_t round_up(size_t n, size_t align) { return round_down(n + align - 1, align); }

private:
    constexpr static size_t min_system_alloc = 2ll << 20;
    constexpr static size_t min_mmap_alloc = 1ll << 30;
    constexpr static size_t max_mmap_alloc = 1ll << 46;
    std::pair<size_t, size_t> _region;
    Stats _stats;
};

std::pair<void*, size_t> SystemAlloc::alloc_from_region(size_t n, size_t align) {
    auto [start, end] = _region;
    // 从 region memory 后边取 n 字节，并对起始地址对齐
    size_t res = (end - n) & ~(align - 1);
    // 起始地址超出 region 范围
    if(res < start) return {nullptr, 0};
    void* ptr = reinterpret_cast<void*>(res);
    size_t actual = end - res;
    if(mprotect(ptr, actual, PROT_READ | PROT_WRITE) != 0) {
        return {nullptr, 0};
    }
    // 更新 region memory 后端
    _region.second = res;

    _stats.allocated_incr(actual);

    return {ptr, actual};
}

std::pair<void*, size_t> SystemAlloc::alloc_from_new_region(size_t n, size_t align) {
    spdlog::info("region memory is not enough, create a new region");
    void* ptr = mmap_align(min_mmap_alloc, min_mmap_alloc);
    if(!ptr) {
        spdlog::warn("mmap new region memory failed");
        return {nullptr, 0};
    }
    auto start = reinterpret_cast<size_t>(ptr);
    _region = {start, start + min_mmap_alloc};

    spdlog::info("mmap new region succeed");

    return alloc_from_region(n, align);
}

void* SystemAlloc::mmap_align(size_t n, size_t align, bool flag) {
    n += align - 1;
    int prot = flag ? (PROT_READ | PROT_WRITE) : PROT_NONE;
    char* ptr = static_cast<char*>(mmap(nullptr, n, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    // 兼容 ptr == nullptr 情况
    return ptr + (align - reinterpret_cast<size_t>(ptr) % align) % align;
}

// 向系统申请 align 对齐的 n 字节数据
std::pair<void*, size_t> SystemAlloc::alloc(size_t n, size_t align) {
    // 按照最小系统申请对齐
    align = std::max(align, min_system_alloc);
    n = round_up(align, min_system_alloc);

    // 超过最大内存/对齐限制
    if(n > max_mmap_alloc || align > max_mmap_alloc) {
        spdlog::warn("allocate memory is too large");
        return {nullptr, 0};
    }

    // 申请超大内存
    if(n > min_mmap_alloc || align > min_mmap_alloc) {
        void* ptr = mmap_align(n, align, true);
        if(ptr != nullptr) {
            _stats.allocated_incr(n);
            spdlog::info("allocated a super memory by mmap {} {}", n, align);
            return {ptr, n};
        }

        spdlog::warn("allocated a super memory by mmap failed: {} {}", n, align);
        return {nullptr, 0};
    }

    // 申请小内存
    // 第一次尝试
    if(_region.first) {
        auto [ptr, actual] = alloc_from_region(n, align);
        if(ptr) return {ptr, actual};
    }

    return alloc_from_new_region(n, align);
}

// 向系统归还从 ptr 开始的 n 字节数据
bool SystemAlloc::dealloc(void* ptr, size_t n) {
    auto new_start = reinterpret_cast<size_t>(ptr);
    size_t new_end = new_start + n;
    spdlog::debug("old dealloc region: [{0}, {1})", new_start, new_end);
    new_start = round_up(new_start, Page::SIZE);
    new_end = round_down(new_end, Page::SIZE);
    spdlog::debug("new dealloc region: [{0}, {1})", new_start, new_end);
    if(new_end <= new_start) return false;

    _stats.deallocated_incr(new_end - new_start);

    ssize_t res = -1;
    do {
        res = madvise(reinterpret_cast<void*>(new_start), new_end - new_start, MADV_DONTNEED);
    } while(res == -1 && errno == EAGAIN);

    return res == 0;
}

SystemAlloc::~SystemAlloc() {
    spdlog::info("destroy system alloc start: ");

    spdlog::info("fetched pages: {}, returned pages: {}",
                 _stats.fetched(), _stats.returned());
    spdlog::info("allocated bytes: {}, deallocated bytes: {}",
                 _stats.allocated(), _stats.deallocated());
    spdlog::info("destroy system alloc end.");
}

using SingleSystemAlloc = Singleton<SystemAlloc>;

#endif //MEMORYPOOL_SYSTEM_ALLOC_H
