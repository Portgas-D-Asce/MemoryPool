#ifndef MEMORYPOOL_THREAD_CACHE_H
#define MEMORYPOOL_THREAD_CACHE_H
#include <memory>
#include <cassert>
#include <spdlog/spdlog.h>
#include "size_classes.h"
#include "size_map.h"
#include "singleton.h"
#include "dynamic_free_list.h"
#include "span.h"
#include "page_heap.h"
#include "size_map.h"
#include "central_cache.h"
#include "stats.h"


class ThreadCache {
public:
    void* alloc(size_t size_class);
    void dealloc(size_t size_class, void* ptr);
    ~ThreadCache();

    ThreadCache() : _total_bytes(0) {}
    ThreadCache(const ThreadCache&) = delete;
    ThreadCache(const ThreadCache&&) = delete;
    ThreadCache& operator=(const ThreadCache&) = delete;
    ThreadCache& operator=(const ThreadCache&&) = delete;
    static thread_local std::shared_ptr<ThreadCache> tc;
private:
    void* fetch_from_central_cache(size_t size_class);
    void return_to_central_cache(size_t size_class, size_t n);
    void list_too_long(size_t size_class);
private:
    constexpr static size_t _max_list_objects = 8192;
    constexpr static size_t _max_overages = 3;
    DynamicFreeList _lists[SizeMap::size_class_size];
    size_t _total_bytes;
    Stats _stats;
};

// 每个线程都会分配一个不同的 thread cache
thread_local std::shared_ptr<ThreadCache> ThreadCache::tc = std::make_shared<ThreadCache>();

// 从 central cache 中拉取一批 objects
void* ThreadCache::fetch_from_central_cache(const size_t size_class) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");

    auto& list = _lists[size_class];
    assert(list.empty() && "fetch from cc when list not empty {}");

    // 一批 fetch 多少个 object
    size_t batch_size = SizeMap::num_to_move(size_class);
    assert(batch_size <= SizeMap::max_move && "batch size too large");

    // fetch object
    void* batch[SizeMap::max_move];
    size_t cnt = SingleCentralCahce::get_instance().alloc(size_class, batch, batch_size);
    if(cnt == 0) {
        spdlog::warn("fetch from central cache failed!: {} 0/{}", size_class, batch_size);
        return nullptr;
    }

    if(cnt != batch_size) {
        spdlog::warn("fetch from central cache: {} {}/{}", size_class, cnt, batch_size);
    }

    _stats.fetched_incr(cnt);

    // 除了第一个 object 外，其它加入到 list 中
    if(cnt > 0) {
        _total_bytes += SizeMap::size(size_class) * cnt;
        list.push_batch(batch + 1, cnt - 1);
    }

    // 调整链表最大长度
    if(list.max_length() < batch_size) {
        list.max_length(list.max_length() + 1);
    } else {
        size_t temp = std::min(list.max_length() + batch_size, _max_list_objects);
        temp -= temp % batch_size;
        list.max_length(temp);
    }

    // 返回第一个 object
    return batch[0];
}

void* ThreadCache::alloc(const size_t size_class) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");

    void* object = nullptr;
    // list 中存在 object
    if(!_lists[size_class].empty()) {
        object = _lists[size_class].pop();
    } else {
        // 从 central cache 中拉取一批 object，并返回一个 object
        object = fetch_from_central_cache(size_class);
    }

    if(object) {
        _total_bytes -= SizeMap::size(size_class);
        _stats.allocated_incr();
    } else {
        spdlog::warn("allocated nullptr from tread cache size class {}", size_class);
    }

    return object;
}

void ThreadCache::return_to_central_cache(const size_t size_class, size_t n) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");
    if(n == 0) return;

    size_t batch_size = SizeMap::num_to_move(size_class);
    assert(batch_size <= SizeMap::max_move && "batch size too large");

    auto& list = _lists[size_class];
    if(list.size() < n) {
        spdlog::warn("return request_num({}) > list_num({})", n, list.size());
        n = list.size();
    }
    _total_bytes -= n * SizeMap::size(size_class);
    _stats.returned_incr(n);

    auto& cc = SingleCentralCahce::get_instance();
    void* batch[SizeMap::max_move];
    while(n >= batch_size) {
        list.pop_batch(batch, batch_size);
        cc.dealloc(size_class, batch, batch_size);
        n -= batch_size;
    }

    if(n > 0) {
        list.pop_batch(batch, n);
        cc.dealloc(size_class, batch, n);
    }
}

void ThreadCache::list_too_long(const size_t size_class) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");

    auto& list = _lists[size_class];
    // 归还一批 object 对象
    size_t batch_size = SizeMap::num_to_move(size_class);
    return_to_central_cache(size_class, std::min(list.size(), batch_size));

    // 调整链表最大长度
    if(list.max_length() < batch_size) {
        list.max_length_incr( 1);
    } else if(list.max_length() > batch_size) {
        list.length_overages_incr(1);
        if(list.length_overages() > _max_overages) {
            list.max_length_decr(batch_size);
            list.length_overages(0);
        }
    }
}

void ThreadCache::dealloc(const size_t size_class, void* ptr) {
    assert(size_class > 0 && size_class < SizeMap::size_class_size && "illegal size_class");

    if(ptr == nullptr) return;

    _total_bytes += SizeMap::size(size_class);

    auto& list = _lists[size_class];
    list.push(ptr);
    _stats.deallocated_incr();

    if(list.size() > list.max_length()) {
        spdlog::debug("too many idle objects {}: {}/{}", size_class, list.size(), list.max_length());
        list_too_long(size_class);
    }
}

ThreadCache::~ThreadCache() {
    spdlog::info("destroy thread cache start: ");
    size_t total = 0;
    for(size_t i = 1; i < SizeMap::size_class_size; ++i) {
        if(_lists[i].empty()) continue;
        spdlog::debug("size class {} returned {} objects", i, _lists[i].size());
        total += _lists[i].size();
        return_to_central_cache(i, _lists[i].size());
    }
    spdlog::debug("release {} objects totally.", total);
    spdlog::info("fetched objects: {}, returned objects: {}",
                 _stats.fetched(), _stats.returned());
    spdlog::info("allocated objects: {}, deallocated objects: {}",
                 _stats.allocated(), _stats.deallocated());
    spdlog::info("destroy thread cache end.");
}

#endif //MEMORYPOOL_THREAD_CACHE_H
