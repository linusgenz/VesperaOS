//
// Created by Linus on 11.07.25.
//

#ifndef VECTOR_H
#define VECTOR_H
#include "../kernel/memory/heap.h"
#include "../kernel/include/memory.h"
#include "../kernel/utils/panic.h"
#include <cstddef>
#include <type_traits>
#include <cstdint>
#include <log.h>

extern bool flag;

template<typename T>
class Vector {
public:
    explicit Vector(size_t initial_capacity = 4)
        : _data(nullptr), capacity(0), length(0) {
        if (initial_capacity == 0) initial_capacity = 4;
        capacity = initial_capacity;
        _data = static_cast<T*>(kernel::memory::malloc(sizeof(T) * capacity));

        if (!_data) {
            panic("Vector malloc failed");
        }
    }

    Vector(const Vector&) = delete;
    Vector& operator=(const Vector&) = delete;

    Vector(Vector&& other) noexcept
        : _data(other._data), capacity(other.capacity), length(other.length) {
        other._data = nullptr;
        other.capacity = 0;
        other.length = 0;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            destroy();
            _data = other._data;
            capacity = other.capacity;
            length = other.length;
            other._data = nullptr;
            other.capacity = 0;
            other.length = 0;
        }
        return *this;
    }

    ~Vector() {
        destroy();
    }

    void clear() {
        if constexpr (!std::is_trivially_destructible<T>::value) {
            for (size_t i = 0; i < length; ++i) {
                _data[i].~T();
            }
        }
        length = 0;
    }

    void push_back(const T& value) {
        if (length >= capacity) {
            resize(capacity ? capacity * 2 : 4);
        }

        if constexpr (std::is_trivially_copyable<T>::value) {
            _data[length] = value;
        } else {
            new (&_data[length]) T(value);
        }
        ++length;
    }

    T* data() { return _data; }
    const T* data() const { return _data; }

    T& operator[](size_t index) {
        return _data[index];
    }
    const T& operator[](size_t index) const {
        return _data[index];
    }

    T& back() {
        if (length == 0) panic("Vector::back() called on empty vector");
        return _data[length - 1];
    }

    const T& back() const {
        if (length == 0) panic("Vector::back() called on empty vector");
        return _data[length - 1];
    }

    size_t size() const { return length; }
    bool empty() const { return length == 0; }

    // Iterator support (raw pointers)
    T* begin() { return _data; }
    T* end()   { return _data + length; }
    const T* begin() const { return _data; }
    const T* end()   const { return _data + length; }
    const T* cbegin() const { return _data; }
    const T* cend()   const { return _data + length; }

private:
    T* _data;
    size_t capacity;
    size_t length;

    void resize(size_t new_capacity) {
        if (new_capacity <= capacity) return;
        T* new_data = static_cast<T*>(kernel::memory::malloc(sizeof(T) * new_capacity));
        if (!new_data) panic("Vector resize malloc failed");

        if constexpr (std::is_trivially_copyable<T>::value) {
            // triviale Typen: einfache Zuweisung
            for (size_t i = 0; i < length; ++i) {
                new_data[i] = _data[i];
            }
        } else {
            for (size_t i = 0; i < length; ++i) {
                new (&new_data[i]) T(_data[i]); // copy-construct
            }
            for (size_t i = 0; i < length; ++i) {
                _data[i].~T();
            }
        }

        kernel::memory::free(_data);
        _data = new_data;
        capacity = new_capacity;
    }

    void destroy() {
        if (!_data) return;
        if constexpr (!std::is_trivially_destructible<T>::value) {
            for (size_t i = 0; i < length; ++i) {
                _data[i].~T();
            }
        }
        kernel::memory::free(_data);
        _data = nullptr;
        capacity = 0;
        length = 0;
    }
};

#endif // VECTOR_H