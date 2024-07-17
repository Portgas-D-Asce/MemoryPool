#ifndef MEMORYPOOL_DYNAMIC_FREE_LIST_H
#define MEMORYPOOL_DYNAMIC_FREE_LIST_H
#include "free_list.h"

class DynamicFreeList : public FreeList {
public:
    DynamicFreeList() : _low_water(0), _max_length(1), _length_overages(0) {}

    [[nodiscard]] size_t low_water() const {
        return _low_water;
    }

    void low_water(size_t low_water) {
        _low_water = low_water;
    }

    void clear_low_water() {
        _low_water = size();
    }

    [[nodiscard]] size_t max_length() const {
        return _max_length;
    }

    void max_length(size_t max_length) {
        _max_length = max_length;
    }

    void max_length_incr(size_t incr) {
        _max_length += incr;
    }

    void max_length_decr(size_t incr) {
        _max_length -= incr;
    }

    [[nodiscard]] size_t length_overages() const {
        return _length_overages;
    }

    void length_overages(size_t length_overages) {
        _length_overages = length_overages;
    }

    void length_overages_incr(size_t incr) {
        _length_overages += incr;
    }

    void length_overages_decr(size_t incr) {
        _length_overages -= incr;
    }

    void* pop();
    void pop_batch(void** batch, size_t n);
private:
    size_t _low_water;
    size_t _max_length;
    size_t _length_overages;
};

void* DynamicFreeList::pop() {
    if(size() - 1 < _low_water) {
        clear_low_water();
    }
    return FreeList::pop();
}

void DynamicFreeList::pop_batch(void **batch, size_t n) {
    FreeList::pop_batch(batch, n);
    if(size() < _low_water) {
        clear_low_water();
    }
}


#endif //MEMORYPOOL_DYNAMIC_FREE_LIST_H
