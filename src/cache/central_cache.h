#ifndef MEMORYPOOL_CENTRAL_CACHE_H
#define MEMORYPOOL_CENTRAL_CACHE_H
#include <mutex>
#include "span.h"
#include "size_classes.h"
#include "size_map.h"
#include "singleton.h"
#include "page_heap.h"
#include "page_map.h"
#include "stats.h"

class CentralCache {
public:
    // thread cache 是批量从 central cache 中申请 object
    size_t alloc(size_t size_class, void** batch, size_t n);
    // thread cache 也是批量向 central cache 归还 object 的
    void dealloc(size_t size_class, void** batch, size_t n);

    friend std::default_delete<CentralCache>;
    friend Singleton<CentralCache>;
    CentralCache(const CentralCache&) = delete;
    CentralCache(const CentralCache&&) = delete;
    CentralCache& operator=(const CentralCache&) = delete;
    CentralCache& operator=(const CentralCache&&) = delete;
private:
    CentralCache() = default;
    ~CentralCache();

    inline size_t fetch_from_page_heap(size_t size_class, void** batch, size_t n);
    inline void return_to_page_heap(size_t size_class, Span* span);
    // 从 span 链表中挨个拉取 object
    size_t fetch_objects(size_t size_class, void** batch, size_t n);

private:
    constexpr static size_t _n = SizeMap::size_class_size;
    SpanList _lists[_n];
    mutable std::mutex _locks[_n];
    Stats _stats;
};

size_t CentralCache::fetch_objects(const size_t size_class, void** batch, size_t n) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");

    auto& list = _lists[size_class];
    size_t total = 0;
    // 遍历所有 span，从其中取出 object
    // span 中没有元素时，从链表上移除 span，边遍历边删除
    while(!list.empty() && total != n) {
        auto span = list.first();
        size_t cnt = span->alloc(batch + total, n - total);
        if(span->empty()) {
            list.remove(span);
            spdlog::debug("all objects are allocated {}/{}", span->allocated(), span->total());
        }

        total += cnt;
    }
    return total;
}

size_t CentralCache::fetch_from_page_heap(const size_t size_class, void** batch, size_t n) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");

    size_t temp = 0, page_num = SizeMap::pages(size_class);
    // 总个数不够时，从 page heap 中申请足够的 object
    while(temp < n) {
        // 从 page heap 申请一个 n page span，只是一个基础的 span
        Span* span = SinglePageHeap::get_instance().alloc(page_num);
        if(span == nullptr) {
            spdlog::warn("fetch a nullptr span from page heap {}", size_class);
            break;
        }
        _stats.fetched_incr(span->num_pages());

        // 初始化 span 中的 free list 相关
        span->init_free_list(SizeMap::size(size_class));

        // 将 span 插入到链表中
        _lists[size_class].prepend(span);

        temp += span->total();
    }

    return fetch_objects(size_class, batch, n);
}

size_t CentralCache::alloc(const size_t size_class, void** batch, size_t n) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");
    if(n == 0) return 0;

    std::lock_guard<std::mutex> lg(_locks[size_class]);
    // 第一次拉取数据
    size_t total = fetch_objects(size_class, batch, n);
    if(total != n) {
        total += fetch_from_page_heap(size_class, batch + total, n - total);
        if(total != n) {
            spdlog::warn("fetch object in cc: request {} actual {}", n, total);
        }
    }

    _stats.allocated_incr(total);

    return total;
}

void CentralCache::return_to_page_heap(const size_t size_class, Span* span) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");
    if(span == nullptr) return;
    if(span->allocated() != 0) {
        spdlog::error("return span {}/{}", span->allocated(), span->total());
    }
    // assert(span->allocated() == 0 && "try returning a using span");

    _stats.returned_incr(span->num_pages());

    // 将 span 从链表上移除
    _lists[size_class].remove(span);

    // 归还 span 给 page heap
    SinglePageHeap::get_instance().dealloc(span);
}

void CentralCache::dealloc(const size_t size_class, void** batch, size_t n) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");
    if(n == 0) return;

    auto& pm = SinglePageMap::get_instance();

    std::lock_guard<std::mutex> lg(_locks[size_class]);
    for(size_t i = 0; i < n; ++i) {
        Span* span = pm.find_span(batch[i]);
        if(span == nullptr) {
            spdlog::error("can't find span when release {}", batch[i]);
            continue;
        }
        _stats.deallocated_incr();
        /* 如果归还 object 前，span 已经分配完所有元素了，则将其重新插入到链表 */
        if(span->empty()) {
            // spdlog::debug("span {} received first returned object {}, {}/{}",
            //              static_cast<void*>(span), batch[i], span->allocated(), span->total());
            _lists[size_class].prepend(span);
        }
        // 释放 span 中 object
        span->dealloc(batch[i]);
        /* 如果 span 中全部元素已归还，释放 span */
        if(span->full()) {
            // spdlog::debug("span {} received last returned object {}, {}/{}",
            //               static_cast<void*>(span), batch[i], span->allocated(), span->total());
            return_to_page_heap(size_class, span);
        }
    }
}

CentralCache::~CentralCache() {
    spdlog::info("destroy central cache start: ");
    for(size_t i = 1; i < SizeMap::size_class_size; ++i) {
        assert(_lists[i].empty() && "make sure all thread cache released");
    }

    spdlog::info("fetched pages: {}, returned pages: {}",
                 _stats.fetched(), _stats.returned());
    spdlog::info("allocated objects : {}, deallocated objects: {}",
                 _stats.allocated(), _stats.deallocated());

    spdlog::info("destroy central cache end.");
}

using SingleCentralCahce = Singleton<CentralCache>;


#endif //MEMORYPOOL_CENTRAL_CACHE_H
