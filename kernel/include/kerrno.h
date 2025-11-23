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
#define ENOUNIT         1   // Unit no longer exists
#define EUNITGONE       2   // Attempt to operate on non-existent unit

// Memory Management Errors
#define ENOMEM          3   // Out of memory
#define EVECALLOC       4   // Vector allocation failed
#define EVECRESIZE      5   // Vector resize allocation failed
#define ERANGE          6   // Index out of range
#define EEMPTY          7   // Operation on empty container

// Hardware/ACPI Errors
#define ENOACPI         8   // ACPI feature unavailable
#define ENOTIMER        9   // Hardware timer unavailable
#define ENOCPUID       10   // CPU identification failed

// CPU Exception Errors (Faults)
#define EDIVZERO       11   // Division by zero fault
#define EINVOP         12   // Invalid opcode fault
#define EGPF           13   // General protection fault
#define EPAGEFAULT     14   // Page fault
#define ESTACKFAULT    15   // Stack segment fault
#define ESEGNOTPRES    16   // Segment not present fault
#define EDOUBLEFAULT   17   // Double fault
#define EMACHCHECK     18   // Machine check exception
#define EUNHANDLED     19   // Unhandled interrupt or exception

// Synchronization Errors
#define EDEADLK        19   // Deadlock detected
#define ESELFDEADLK    20   // Self-deadlock on same lock

// Scheduling/Context Errors
#define EAPRETURN      21   // Application processor returned unexpectedly
#define ECTXSWITCH     22   // Context switch error

#endif //VESPERAOS_KERRNO_H