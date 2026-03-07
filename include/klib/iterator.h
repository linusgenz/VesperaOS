/**
 * @file iterator.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 07.01.26.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_ITERATOR_H
#define VESPERAOS_ITERATOR_H

template<typename Iterator>
struct IteratorTraits {
    using difference_type = decltype(Iterator{} - Iterator{});
    using value_type = Iterator::value_type;
    using pointer_t = Iterator::pointer;
    using reference_t = Iterator::reference;
};

template<typename T>
struct IteratorTraits<T*> {
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer_t = T*;
    using reference_t = T&;
};

template<typename T>
struct IteratorTraits<const T*> {
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer_t = const T*;
    using reference_t = const T&;
};

#endif //VESPERAOS_ITERATOR_H