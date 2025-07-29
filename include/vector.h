//
// Created by Linus on 11.07.25.
//

#ifndef VECTOR_H
#define VECTOR_H
#include "../kernel/memory/heap.h"

template<typename T>
class Vector {
public:
    Vector(size_t initial_capacity = 4)
        : capacity(initial_capacity), length(0) {
        data = static_cast<T*>(malloc(sizeof(T) * capacity));
    }

    ~Vector() {
        for (size_t i = 0; i < length; ++i)
            data[i].~T();
        free(data);
    }

    void clear() {
        for (size_t i = 0; i < length; ++i)
            data[i].~T();
        length = 0;
    }

    void push_back(const T& value) {
        if (length >= capacity)
            resize(capacity * 2);
        new (&data[length]) T(value); // placement new
        length++;
    }

    T& operator[](size_t index) {
        return data[index];
    }

    const T& operator[](size_t index) const {
        return data[index];
    }

    size_t size() const {
        return length;
    }

private:
    T* data;
    size_t capacity;
    size_t length;

    void resize(size_t new_capacity) {
        T* new_data = static_cast<T*>(malloc(sizeof(T) * new_capacity));
        for (size_t i = 0; i < length; ++i)
            new (&new_data[i]) T(data[i]); // copy-construct
        for (size_t i = 0; i < length; ++i)
            data[i].~T();
        free(data);
        data = new_data;
        capacity = new_capacity;
    }
};

#endif // VECTOR_H
