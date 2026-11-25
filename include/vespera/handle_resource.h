// handle_resource.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef VESPERAOS_HANDLE_RESOURCE_H
#define VESPERAOS_HANDLE_RESOURCE_H

class Pollable;

/**
 * @brief Common base class for all resources in a HandleTable.
 *
 * Replaces RTTI with explicit capability queries (as_pollable(), etc.).
 * Every class registered via HandleTable::add() must inherit from
 * HandleResource - in cases of multiple inheritance, it must ALWAYS be the first base class.
 */
class HandleResource {
public:
    virtual Pollable* as_pollable() {
        return nullptr;
    }

    // virtual Readable* as_readable() { return nullptr; }
    // virtual Writable* as_writable() { return nullptr; }

protected:
    virtual ~HandleResource() = default;
};

#endif // VESPERAOS_HANDLE_RESOURCE_H