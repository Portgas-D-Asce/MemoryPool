#ifndef MEMORYPOOL_PAGE_MAP_H
#define MEMORYPOOL_PAGE_MAP_H
#include <map>
#include <mutex>
#include "span.h"
#include "singleton.h"

class PageMap {
public:
    void insert(Span* s);
    void erase(Span* s);
    Span* find_span(void* ptr) const;
    Span* find_prev(Span* span) const;
    Span* find_next(Span* span) const;

    friend std::default_delete<PageMap>;
    friend Singleton<PageMap>;
    PageMap(const PageMap&) = delete;
    PageMap(const PageMap&&) = delete;
    PageMap& operator=(const PageMap&) = delete;
    PageMap& operator=(const PageMap&&) = delete;
private:
    PageMap() = default;
    ~PageMap() = default;
private:
    std::map<void*, Span*> _mp;
    mutable std::mutex _lock;
};

Span* PageMap::find_span(void* ptr) const {
    std::lock_guard<std::mutex> lg(_lock);
    auto it = _mp.upper_bound(ptr);
    if(it == _mp.begin()) { return nullptr; }
    --it;
    auto span = it->second;
    if(ptr >= span->end_addr()) { return nullptr; }
    return span;
}

Span* PageMap::find_prev(Span* span) const {
    return find_span(static_cast<char*>(span->start_addr()) - 1);
}

Span* PageMap::find_next(Span* span) const {
    return find_span(span->end_addr());
}

void PageMap::insert(Span* s) {
    std::lock_guard<std::mutex> lg(_lock);
    _mp[s->start_addr()] = s;
}

void PageMap::erase(Span* s) {
    std::lock_guard<std::mutex> lg(_lock);
    _mp.erase(s->start_addr());
}

using SinglePageMap = Singleton<PageMap>;


#endif //MEMORYPOOL_PAGE_MAP_H
