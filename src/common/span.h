#ifndef MEMORYPOOL_SPAN_H
#define MEMORYPOOL_SPAN_H
#include <utility>
#include <algorithm>
#include "free_list.h"
#include "page.h"
#include "intrusive_list.h"

class Span;
using SpanList = IntrusiveList<Span>;
// 不一定每个 span 都有一个 size class 与之对应
class Span : public SpanList::Elem {
public:
    /* span 可分为两种，但都必须在 page map 上登记：
     * 被 central cache 所拥有的 using span：释放 object 时需要根据 addr 找其所在 span
     * 被 page cache 所拥有的 idle span：释放 using span 时需要判断前后是否可以合并，要求 span 为 idle 状态
    */
    enum class STATUS{
        USING,
        IDLE
    };
private:
    Page _first_page;
    size_t _num_pages;
    STATUS _status;

    FreeList _list;
    size_t _allocated, _total;

public:
    Span(void* ptr, size_t num_pages) : _first_page(ptr),
        _num_pages(num_pages), _status(STATUS::IDLE), _list(), _allocated(0), _total(0) {}

    size_t alloc(void** batch, size_t n);
    void dealloc(void* ptr);
    void init_free_list(size_t size_obj);

    [[nodiscard]] bool empty() const {
        return _allocated == _total;
    }

    [[nodiscard]] bool full() const {
        return _allocated == 0;
    }

    [[nodiscard]] Page first_page() const {
        return _first_page;
    }

    void first_page(Page page) {
        _first_page = page;
    }

    [[nodiscard]] size_t num_pages() const {
        return _num_pages;
    }

    void num_pages(size_t num_page) {
        _num_pages = num_page;
    }

    void status(STATUS s) {
        _status = s;
    }

    [[nodiscard]] STATUS status() const {
        return _status;
    }

    [[nodiscard]] size_t allocated() const {
        return _allocated;
    }

    [[nodiscard]] size_t total() const {
        return _total;
    }

    [[nodiscard]] void* start_addr() const {
        return _first_page.start_addr();
    }

    [[nodiscard]] void* end_addr() const {
        return (_first_page + _num_pages).start_addr();
    }

    [[nodiscard]] void* page_addr(size_t n) const {
        return (_first_page + n).start_addr();
    }

    [[nodiscard]] size_t num_bytes() const {
        return _num_pages * Page::SIZE;
    }
};

size_t Span::alloc(void** batch, size_t n) {
    size_t cnt = std::min(n, _list.size());
    _list.pop_batch(batch, cnt);
    _allocated += cnt;
    return cnt;
}

void Span::dealloc(void* ptr) {
    _list.push(ptr);
    _allocated--;
}

void Span::init_free_list(size_t size_obj) {
    char* start = static_cast<char*>(start_addr());
    char* end = static_cast<char*>(end_addr());
    _allocated = 0;
    _total = (end - start) / size_obj;
    for(size_t i = 0; i < _total; ++i) {
        _list.push(start);
        start += size_obj;
    }
}

#endif //MEMORYPOOL_SPAN_H
