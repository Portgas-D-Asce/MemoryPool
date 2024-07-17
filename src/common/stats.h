#ifndef MEMORYPOOL_STATS_H
#define MEMORYPOOL_STATS_H
#include <utility>
#include <atomic>

class Stats {
private:
    std::atomic<size_t> _fetched, _returned, _allocated, _deallocated;
public:
    Stats() : _fetched(0), _returned(0), _allocated(0), _deallocated(0) {}

    [[nodiscard]] size_t fetched() const {
        return _fetched.load(std::memory_order_relaxed);
    }

    void fetched(size_t fetched) {
        _fetched.store(fetched, std::memory_order_relaxed);
    }

    void fetched_incr(size_t incr = 1) {
        _fetched.fetch_add(incr, std::memory_order_relaxed);
    }

    [[nodiscard]] size_t returned() const {
        return _returned.load(std::memory_order_relaxed);
    }

    void returned(size_t returned) {
        _returned.store(returned, std::memory_order_relaxed);
    }

    void returned_incr(size_t incr = 1) {
        _returned.fetch_add(incr, std::memory_order_relaxed);
    }

    [[nodiscard]] size_t allocated() const {
        return _allocated.load(std::memory_order_relaxed);
    }

    void allocated(size_t allocated) {
        _allocated.store(allocated, std::memory_order_relaxed);
    }

    void allocated_incr(size_t incr = 1) {
        _allocated.fetch_add(incr, std::memory_order_relaxed);
    }

    [[nodiscard]] size_t deallocated() const {
        return _deallocated.load(std::memory_order_relaxed);
    }

    void deallocated(size_t deallocated) {
        _deallocated.store(deallocated, std::memory_order_relaxed);
    }

    void deallocated_incr(size_t incr = 1) {
        _deallocated.fetch_add(incr, std::memory_order_relaxed);
    }
};


#endif //MEMORYPOOL_STATS_H
