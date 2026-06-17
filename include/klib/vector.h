//
// Created by Linus on 11.07.25.
//

#ifndef VECTOR_H
#define VECTOR_H


#include <vespera/mm/memory.h>
#include <klib/type_traits.h>
#include <vespera/kerrno.h>
#include <vespera/system/system_manager.h>

template <typename T>
class Vector {
public:
    explicit Vector(usize initial_capacity = 0)
        : data_(nullptr)
          , capacity_(0)
          , length_(0) {
        if (initial_capacity > 0) {
            data_ = static_cast<T*>(kernel::memory::malloc(sizeof(T) * initial_capacity));
            if (!data_) kernel::SystemManager::system_panic("Vector malloc failed", -KEVECALLOC);
            capacity_ = initial_capacity;
        }
    }

    Vector(const Vector&) = delete;
    Vector& operator=(const Vector&) = delete;

    Vector(Vector&& other) noexcept
        : data_(other.data_), capacity_(other.capacity_), length_(other.length_) {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.length_ = 0;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            destroy();
            data_ = other.data_;
            capacity_ = other.capacity_;
            length_ = other.length_;
            other.data_ = nullptr;
            other.capacity_ = 0;
            other.length_ = 0;
        }
        return *this;
    }

    Vector copy() const {
        Vector result(length_);
        for (usize i = 0; i < length_; ++i) {
            if constexpr (klib::is_trivially_copyable_v<T>) {
                result.data_[i] = data_[i];
            } else {
                new(&result.data_[i]) T(data_[i]);
            }
        }
        result.length_ = length_;
        return result;
    }

    ~Vector() {
        destroy();
    }

    void clear() {
        if constexpr (!klib::is_trivially_destructible_v<T>) {
            for (usize i = 0; i < length_; ++i) {
                data_[i].~T();
            }
        }
        length_ = 0;
    }

    void push_back(const T& value) {
        if (length_ >= capacity_) {
            resize(capacity_ ? capacity_ * 2 : 4);
        }

        if constexpr (klib::is_trivially_copyable_v<T>) {
            data_[length_] = value;
        } else {
            new(&data_[length_]) T(value);
        }
        ++length_;
    }

    T* data() { return data_; }
    const T* data() const { return data_; }

    T& operator[](usize index) {
        return data_[index];
    }

    const T& operator[](usize index) const {
        return data_[index];
    }

    void erase(usize index) {
        if (index >= length_) {
            kernel::SystemManager::system_panic("Vector::erase() index out of range", -KERANGE);
        }

        if constexpr (!klib::is_trivially_destructible_v<T>) {
            data_[index].~T();
        }

        for (usize i = index; i < length_ - 1; ++i) {
            if constexpr (klib::is_trivially_copyable_v<T>) {
                data_[i] = data_[i + 1];
            } else {
                new(&data_[i]) T(data_[i + 1]);
                data_[i + 1].~T();
            }
        }

        --length_;
    }

    bool erase_value(const T& value) {
        for (usize i = 0; i < length_; ++i) {
            if (data_[i] == value) {
                erase(i);
                return true;
            }
        }
        return false;
    }

    void pop() {
        if (length_ == 0) {
            kernel::SystemManager::system_panic("Vector::pop() called on empty vector", -KEEMPTY);
        }
        --length_;
        if constexpr (!klib::is_trivially_destructible_v<T>) {
            data_[length_].~T();
        }
    }

    T pop_back() {
        if (length_ == 0) {
            kernel::SystemManager::system_panic("Vector::pop_back() called on empty vector", -KEEMPTY);
        }
        --length_;
        if constexpr (!klib::is_trivially_destructible_v<T>) {
            T value = data_[length_];
            data_[length_].~T();
            return value;
        } else {
            return data_[length_];
        }
    }

    T& back() {
        if (length_ == 0) kernel::SystemManager::system_panic("Vector::back() called on empty vector", -KEEMPTY);
        return data_[length_ - 1];
    }

    const T& back() const {
        if (length_ == 0) kernel::SystemManager::system_panic("Vector::back() called on empty vector", -KEEMPTY);
        return data_[length_ - 1];
    }

    [[nodiscard]] usize size() const { return length_; }
    [[nodiscard]] bool empty() const { return length_ == 0; }

    // Iterator support (raw pointers)
    T* begin() { return data_; }
    T* end() { return data_ + length_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + length_; }
    const T* cbegin() const { return data_; }
    const T* cend() const { return data_ + length_; }

private:
    T* data_;
    usize capacity_{0};
    usize length_{0};

    void resize(usize new_capacity) {
        if (new_capacity <= capacity_) return;
        T* newdata = static_cast<T*>(kernel::memory::malloc(sizeof(T) * new_capacity));
        if (!newdata) kernel::SystemManager::system_panic("Vector resize malloc failed", -KEVECRESIZE);

        if constexpr (klib::is_trivially_copyable_v<T>) {
            // triviale Typen
            for (usize i = 0; i < length_; ++i) {
                newdata[i] = data_[i];
            }
        } else {
            for (usize i = 0; i < length_; ++i) {
                new(&newdata[i]) T(data_[i]); // copy-construct
            }
            for (usize i = 0; i < length_; ++i) {
                data_[i].~T();
            }
        }

        kernel::memory::free(data_);
        data_ = newdata;
        capacity_ = new_capacity;
    }

    void destroy() {
        if (!data_) return;
        if constexpr (!klib::is_trivially_destructible_v<T>) {
            for (usize i = 0; i < length_; ++i) {
                data_[i].~T();
            }
        }
        kernel::memory::free(data_);
        data_ = nullptr;
        capacity_ = 0;
        length_ = 0;
    }
};

#endif // VECTOR_H
