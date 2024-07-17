#ifndef MEMORYPOOL_FREE_LIST_H
#define MEMORYPOOL_FREE_LIST_H
#include <utility>

class FreeList {
private:
    void* _list;
    size_t _n;
public:
    FreeList() : _list(nullptr), _n(0) {}

    [[nodiscard]] size_t size() const {
        return _n;
    }

    bool empty() {
        return _list == nullptr;
    }

    void push(void* ptr) {
        sll_push(&_list, ptr);
        _n++;
    }

    void* pop() {
        --_n;
        return sll_pop(&_list);
    }

    void push_batch(void** batch, size_t n) {
        for(size_t i = 0; i < n - 1; ++i) {
            sll_set_next(batch[i], batch[i + 1]);
        }

        sll_set_next(batch[n - 1], _list);
        _list = batch[0];
        _n += n;
    }

    void pop_batch(void **batch, size_t n) {
        for(size_t i = 0; i < n; ++i) {
            batch[i] = _list;
            _list = sll_next(_list);
        }
        _n -= n;
    }

private:
    static inline void* sll_next(void *ptr) {
        return *(reinterpret_cast<void**>(ptr));
    }

    static inline void sll_set_next(void* ptr, void* next) {
        *(reinterpret_cast<void**>(ptr)) = next;
    }

    static inline void sll_push(void** list, void* ptr) {
        sll_set_next(ptr, *list);
        *list = ptr;
    }

    static inline void* sll_pop(void** list) {
        void* res = *list;
        void* next = sll_next(*list);
        *list = next;
        return res;
    }
};


#endif //MEMORYPOOL_FREE_LIST_H
