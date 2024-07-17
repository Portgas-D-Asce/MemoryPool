#ifndef MEMORYPOOL_PAGE_HEAP_H
#define MEMORYPOOL_PAGE_HEAP_H
#include <list>
#include <iostream>
#include <cassert>
#include <spdlog/spdlog.h>
#include "size_classes.h"
#include "span.h"
#include "singleton.h"
#include "size_map.h"
#include "page_map.h"
#include "system_alloc.h"
#include "stats.h"

class PageHeap {
public:
    // central cache 获取一个 span
    Span* alloc(size_t size_class);
    // central cache 归还一个 span
    void dealloc(Span* span);

    friend std::default_delete<PageHeap>;
    friend Singleton<PageHeap>;
    PageHeap(const PageHeap&) = delete;
    PageHeap(const PageHeap&&) = delete;
    PageHeap& operator=(const PageHeap&) = delete;
    PageHeap& operator=(const PageHeap&&) = delete;
private:
    PageHeap() = default;
    ~PageHeap();

private:
    // 添加 span 到对应链表
    void add_to_list(Span* span);
    // 从所在链表上移除（要求先前在链表上）
    void remove_from_list(Span* span);
    // 从头构造一个 span
    Span* create_span(void* ptr, size_t num_page, Span::STATUS status = Span::STATUS::IDLE);
    // 彻底释放一个 span
    void destroy_span(Span* span);

    Span* carve(Span* span, size_t n);
    Span* find_from_large(size_t n);
    Span* fetch_from_system(size_t n);
    void return_to_system(Span* span);

    static size_t span_idx(Span* span);

private:
    constexpr static size_t _n = 1 << (20 - Page::SHIFT);
    // 前 n 个为 small span，最后一个为 large span
    SpanList _lists[_n + 1];
    mutable std::mutex _lock;
    Stats _stats;
};

size_t PageHeap::span_idx(Span* span) {
    return std::min(span->num_pages(), _n + 1) - 1;
}

void PageHeap::add_to_list(Span* span) {
    auto& list = _lists[span_idx(span)];
    list.prepend(span);
}

void PageHeap::remove_from_list(Span* span) {
    // 从链表上移除当前 span
    auto& list = _lists[span_idx(span)];
    list.remove(span);
}

Span* PageHeap::create_span(void* ptr, size_t num_page, Span::STATUS status) {
    // 构造一个 span 对象
    Span* span = new Span(ptr, num_page);
    // 在 page map 上登记
    SinglePageMap::get_instance().insert(span);

    // 要么以 idle 状态添加到对应链表，要么以不加入到链表，但必须标记为 using 状态
    if(status == Span::STATUS::IDLE) {
        add_to_list(span);
    } else {
        span->status(Span::STATUS::USING);
    }

    // 默认是 idle 状态
    return span;
}

void PageHeap::destroy_span(Span* span) {
    // 从 page map 上注销
    SinglePageMap::get_instance().erase(span);
    // 从所在链表上移除
    remove_from_list(span);
    // 释放自身空间
    delete span;
}

Span* PageHeap::carve(Span* span, size_t n) {
    // 将其从 span 链表上移除，并添加到新的 span 链表上
    assert(span->num_pages() >= n && "carved span is smaller than n");

    remove_from_list(span);
    // 恰好为 n page span，不涉及登记相关
    if(span->num_pages() == n) {
        span->status(Span::STATUS::USING);
        return span;
    }

    // 旧 span 更新其页数为剩余 page 数目，添加到新的 list
    // 不用重新登记，不用设置起始 page id，状态还是处于 idle 状态
    size_t num_page = span->num_pages() - n;
    span->num_pages(num_page);
    add_to_list(span);
    // spdlog::debug("rem span: {} {}", span->start_addr(), span->end_addr());

    // 后 n 个 page 归属于新 span，不需要添加到链表，但是要登记
    return create_span(span->page_addr(num_page), n, Span::STATUS::USING);
}

Span* PageHeap::fetch_from_system(size_t n) {
    // 从 system 中申请一块空间，返回空间可能远大于请求字节数，请求失败则直接返回
    auto& sa = SingleSystemAlloc::get_instance();
    auto [ptr, actual] = sa.alloc(n * Page::SIZE, Page::SIZE);
    // spdlog::debug("fetch from system: {} {}", ptr, actual);

    if(!ptr) {
        spdlog::warn("fetch from system failed: {} pages", n);
        return nullptr;
    }

    assert(actual % Page::SIZE == 0 && "system alloc not align with page size!");

    _stats.fetched_incr(actual);

    // 构造一个 span 接收申请到的空间，并将其加入到对应 span list 中
    return create_span(ptr, actual / Page::SIZE);
}

void PageHeap::return_to_system(Span* span) {
    if(span == nullptr) {
        return;
    }

    _stats.returned_incr(span->num_bytes());

    void* start = span->start_addr();
    size_t bytes = span->num_bytes();
    destroy_span(span);

    SingleSystemAlloc::get_instance().dealloc(start, bytes);


}

Span* PageHeap::find_from_large(size_t n) {
    auto& list = _lists[_n];
    if(list.empty()) return nullptr;
    return list.first();
}

Span* PageHeap::alloc(size_t n) {
    Span* span = nullptr;
    std::lock_guard<std::mutex> lg(_lock);
    // 从 small span 中找出一个 n page span
    for(size_t i = n; i < _n; ++i) {
        if(!_lists[i].empty()) {
            span = _lists[i].first();
            break;
        }
    }

    // 从 large span 中找出一个 n page span
    if(span == nullptr) {
        span = find_from_large(n);
    }

    // 找不到 从 system 中申请一个
    if(span == nullptr) {
        span = fetch_from_system(n);
    }

    if(span == nullptr) {
        return nullptr;
    }

    _stats.allocated_incr(n);

    return carve(span, n);
}

void PageHeap::dealloc(Span* span) {
    assert(span->status() == Span::STATUS::USING && "span must be using before dealloc!");

    _stats.deallocated_incr(span->num_pages());

    auto& pm = Singleton<PageMap>::get_instance();
    /* span 不在链表上，状态为 using，先标记为空闲状态，解除登记*/
    std::lock_guard<std::mutex> lg(_lock);
    span->status(Span::STATUS::IDLE);
    pm.erase(span);
    // spdlog::debug("release span: {} {}", span->start_addr(), span->end_addr());

    Span* prev = pm.find_prev(span);
    // 前一个字节是否属于某个 span，属于且 span 处于 idle 状态，则合并
    if(prev != nullptr && prev->status() == Span::STATUS::IDLE) {
        // 将前一个 span 合并到当前 span
        span->first_page(prev->first_page());
        span->num_pages(span->num_pages() + prev->num_pages());

        // 合并后移除 prev (从链表上移除 + 注销登记 + 释放自身内存)
        destroy_span(prev);
    }

    Span* next = pm.find_next(span);
    // 后一个字节是否属于某个 span，属于且 span 处于 idle 状态，则合并
    if(next != nullptr && next->status() == Span::STATUS::IDLE) {
        // 将后一个 span 合并到当前 span
        span->num_pages(span->num_pages() + next->num_pages());

        // 合并后移除 next (从链表上移除 + 注销登记 + 释放自身内存)
        destroy_span(next);
    }

    // 回收到链表，并在 page map 上重新登记
    add_to_list(span);
    pm.insert(span);
}

PageHeap::~PageHeap() {
    spdlog::info("destroy page heap start: ");
    size_t total = 0;
    for(size_t i = 1; i <= _n; ++i) {
        auto& list = _lists[i];
        if(list.empty()) continue;
        total += list.size();
        spdlog::debug("{} page span: {}", i, list.size());
        while(!list.empty()) {
            spdlog::debug("{} page span: {}", i, list.first()->num_pages());
            return_to_system(list.first());
        }
    }
    spdlog::debug("release {} spans totally.", total);

    spdlog::info("fetched bytes: {}, returned bytes: {}",
                 _stats.fetched(), _stats.returned());
    spdlog::info("allocated pages: {}, deallocated pages: {}",
                 _stats.allocated(), _stats.deallocated());
    spdlog::info("destroy page heap end.");
}

using SinglePageHeap = Singleton<PageHeap>;


#endif //MEMORYPOOL_PAGE_HEAP_H
