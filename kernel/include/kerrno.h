// kerrno.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 23.11.25.
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

#ifndef VESPERAOS_KERRNO_H
#define VESPERAOS_KERRNO_H

// Process/Task Management Errors
#define KENOUNIT         1   // Unit no longer exists
#define KEUNITGONE       2   // Attempt to operate on non-existent unit

// Memory Management Errors
#define KENOMEM          3   // Out of memory
#define KEVECALLOC       4   // Vector allocation failed
#define KEVECRESIZE      5   // Vector resize allocation failed
#define KERANGE          6   // Index out of range
#define KEEMPTY          7   // Operation on empty container

// Hardware/ACPI Errors
#define KENOACPI         8   // ACPI feature unavailable
#define KENOTIMER        9   // Hardware timer unavailable
#define KENOCPUID       10   // CPU identification failed

// CPU Exception Errors (Faults)
#define KEDIVZERO       11   // Division by zero fault
#define KEINVOP         12   // Invalid opcode fault
#define KEGPF           13   // General protection fault
#define KEPAGEFAULT     14   // Page fault
#define KESTACKFAULT    15   // Stack segment fault
#define KESEGNOTPRES    16   // Segment not present fault
#define KEDOUBLEFAULT   17   // Double fault
#define KEMACHCHECK     18   // Machine check exception
#define KEUNHANDLED     19   // Unhandled interrupt or exception

// Synchronization Errors
#define KEDEADLK        19   // Deadlock detected
#define KESELFDEADLK    20   // Self-deadlock on same lock

// Scheduling/Context Errors
#define KEAPRETURN      21   // Application processor returned unexpectedly
#define KECTXSWITCH     22   // Context switch error

#endif //VESPERAOS_KERRNO_H