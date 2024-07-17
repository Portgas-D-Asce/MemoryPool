#ifndef MEMORYPOOL_PAGE_H
#define MEMORYPOOL_PAGE_H
#include <utility>

class Page {
public:
    constexpr static size_t SHIFT = 13;
    constexpr static size_t SIZE = 1 << 13;
private:
    size_t _id;
public:
    explicit Page(void* ptr) : _id(reinterpret_cast<size_t>(ptr) >> SHIFT) {}
    explicit Page(size_t id) : _id(id) {}

    [[nodiscard]] size_t id() const {
        return _id;
    }

    [[nodiscard]] void* start_addr() const {
        return reinterpret_cast<void*>(_id << SHIFT);
    }

    Page operator+(size_t n) const {
        return Page(_id + n);
    }
};


#endif //MEMORYPOOL_PAGE_H
