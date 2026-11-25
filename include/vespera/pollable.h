// pollable.h
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

#ifndef VESPERAOS_POLLABLE_H
#define VESPERAOS_POLLABLE_H

class PollTable;

class Pollable {
public:
    // Returns the current readiness mask (POLLIN, POLLOUT, POLLHUP, POLLERR).
    // If pt != nullptr, the resource registers itself in its WaitQueue under its own lock.
    virtual int poll(PollTable* pt) = 0;

protected:
    ~Pollable() = default;
};

#endif