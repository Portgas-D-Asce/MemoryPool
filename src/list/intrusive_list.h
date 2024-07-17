#ifndef MEMORYPOOL_INTRUSIVE_LIST_H
#define MEMORYPOOL_INTRUSIVE_LIST_H
#include <utility>

template<typename T>
class IntrusiveList {
public:
    class Iter;
    class Elem {
    private:
        friend class Iter;
        friend class IntrusiveList<T>;
        Elem* _prev;
        Elem* _next;
    protected:
        Elem() : _prev(nullptr), _next(nullptr) {}
        bool remove() {
            Elem* next = _next;
            Elem* prev = _prev;
            prev->_next = next;
            next->_prev = prev;
            // _prev = _next = nullptr;
            return next == prev;
        }

        void prepend(Elem* item) {
            Elem* prev = _prev;
            item->_prev = prev;
            item->_next = this;
            prev->_next = item;
            _prev = item;
        }

        void append(Elem* item) {
            Elem* next = _next;
            item->_next = next;
            item->_prev = this;
            next->_prev = item;
            _next = item;
        }
    };

    class Iter {
    private:
        friend class IntrusiveList;
        Elem* _elem;
        explicit Iter(Elem* elem) : _elem(elem) {}
    public:
        Iter& operator++() {
            _elem = _elem->_next;
            return *this;
        }

        Iter& operator--() {
            _elem = _elem->_prev;
            return *this;
        }

        bool operator!=(Iter other) const { return _elem != other._elem; }
        bool operator==(Iter other) const { return _elem != other._elem; }
        T& operator*() const { return *static_cast<T*>(_elem); }
        T* operator->() const { return static_cast<T*>(_elem); }
    };

public:
    IntrusiveList() { _dummy._prev = _dummy._next = &_dummy; }

    [[nodiscard]] bool empty() const { return _dummy._next == &_dummy; }

    [[nodiscard]] size_t size() const {
        size_t sz = 0;
        for(Elem* e = _dummy._next; e != &_dummy; e = e->_next) {
            sz++;
        }
        return sz;
    }

    T* first() const {
        return static_cast<T*>(_dummy._next);
    }

    T* last() const {
        return static_cast<T*>(_dummy._prev);
    }

    void prepend(T* item) {
        _dummy.append(item);
    }

    void append(T* item) {
        _dummy.prepend(item);
    }

    bool remove(T* item) {
        return item->remove();
    }

    Iter begin() const {
        return Iter(_dummy._next);
    }

    Iter end() {
        return Iter(&_dummy);
    }
private:
    Elem _dummy;
};


#endif //MEMORYPOOL_INTRUSIVE_LIST_H
