//
// Created by Linus on 11.07.25.
//

#ifndef VECTOR_H
#define VECTOR_H
#include "../kernel/memory/heap.h"
#include "../kernel/include/memory.h"
template<typename T>
class Vector {
public:
    Vector(size_t initial_capacity = 4)
        : capacity(initial_capacity), length(0) {
        _data = static_cast<T*>(kernel::memory::malloc(sizeof(T) * capacity));
    }

    ~Vector() {
        for (size_t i = 0; i < length; ++i)
            _data[i].~T();
        kernel::memory::free(_data);
    }

    void clear() {
        for (size_t i = 0; i < length; ++i)
            _data[i].~T();
        length = 0;
    }

    void push_back(const T& value) {
        if (length >= capacity)
            resize(capacity * 2);
        new (&_data[length]) T(value); // placement new
        length++;
    }

    T* data() {
        return _data;
    }

    const T* data() const {
        return _data;
    }

    T& operator[](size_t index) {
        return _data[index];
    }

    const T& operator[](size_t index) const {
        return _data[index];
    }

    size_t size() const {
        return length;
    }

private:
    T* _data;
    size_t capacity;
    size_t length;

    void resize(size_t new_capacity) {
        T* new_data = static_cast<T*>(kernel::memory::malloc(sizeof(T) * new_capacity));
        for (size_t i = 0; i < length; ++i)
            new (&new_data[i]) T(_data[i]); // copy-construct
        for (size_t i = 0; i < length; ++i)
            _data[i].~T();
        kernel::memory::free(_data);
        _data = new_data;
        capacity = new_capacity;
    }
};

#endif // VECTOR_H
