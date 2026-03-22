// signal.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.03.26.
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

#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/scheduling.h>
#include <vespera/signals.h>
#include <vespera/types.h>

#include "../scheduling/unit_termination.h"

enum class DefaultAction : u8 {
    Terminate,
    TerminateCore,
    Ignore,
    Stop,  // TODO
};

static DefaultAction default_action_for(Signal sig) {
    switch (sig) {
        case Signal::SIGFPE:
        case Signal::SIGILL:
        case Signal::SIGSEGV:
        case Signal::SIGBUS:
            return DefaultAction::TerminateCore;
        case Signal::SIGINT:
        case Signal::SIGKILL:
        case Signal::SIGTERM:
            return DefaultAction::Terminate;
        case Signal::SIGPIPE:
        case Signal::SIGCHLD:
            return DefaultAction::Ignore;
        default:
            return DefaultAction::Terminate;
    }
}

static const char* signal_name(Signal sig) {
    switch (sig) {
        case Signal::SIGFPE:
            return "floating point exception";
        case Signal::SIGILL:
            return "illegal instruction";
        case Signal::SIGSEGV:
            return "segmentation fault";
        case Signal::SIGBUS:
            return "bus error";
        case Signal::SIGKILL:
            return "killed";
        case Signal::SIGTERM:
            return "terminated";
        case Signal::SIGPIPE:
            return "broken pipe";
        case Signal::SIGCHLD:
            return "child exited";
        case Signal::SIGALRM:
            return "alarm";
        default:
            return "signal";
    }
}

bool is_valid_signal(i32 signum) {
    switch (static_cast<Signal>(signum)) {
        case Signal::SIGINT:
        case Signal::SIGFPE:
        case Signal::SIGKILL:
        case Signal::SIGSEGV:
        case Signal::SIGBUS:
        case Signal::SIGILL:
        case Signal::SIGTERM:
        case Signal::SIGCHLD:
        case Signal::SIGALRM:
        case Signal::SIGPIPE:
            return true;
        default:
            return false;
    }
}

void signal_send(Unit* u, Signal sig) {
    const u32 n = static_cast<u32>(sig);

    if (sig == Signal::SIGKILL) {
        kernel::scheduling::kill_current_realm(sig, "killed");
    }

    __sync_or_and_fetch(&u->signals_pending, 1ULL << n);
    if (u->state == UnitState::Blocked) {
        u->state = UnitState::Ready;
    }
}

void signal_setup_userframe(Unit* u, Signal sig, void (*handler)(int), TrapFrame* trap) {
    const uptr stack_virt_base = virt_raw(u->context.user_stack_virt_base);
    const uptr stack_hhdm_base = virt_raw(u->context.user_stack);

    uptr usp = trap->rsp;

    usp -= sizeof(SignalFrame);
    usp &= ~0xFULL;

    if (usp < stack_virt_base) return;

    const uptr frame_offset = usp - stack_virt_base;
    auto* frame_dst = reinterpret_cast<SignalFrame*>(stack_hhdm_base + frame_offset);

    SignalFrame frame{};
    frame.rax = trap->rax;
    frame.rbx = trap->rbx;
    frame.rcx = trap->rcx;
    frame.rdx = trap->rdx;
    frame.rbp = trap->rbp;
    frame.rsi = trap->rsi;
    frame.rdi = trap->rdi;
    frame.r8 = trap->r8;
    frame.r9 = trap->r9;
    frame.r10 = trap->r10;
    frame.r11 = trap->r11;
    frame.r12 = trap->r12;
    frame.r13 = trap->r13;
    frame.r14 = trap->r14;
    frame.r15 = trap->r15;
    frame.rip = trap->rip;
    frame.rsp = trap->rsp;  // original RSP
    frame.rflags = trap->rflags;
    frame.signum = static_cast<i32>(sig);
    memcpy(frame_dst, &frame, sizeof(SignalFrame));

    usp -= sizeof(uptr);

    if (usp < stack_virt_base) return;

    const uptr ret_offset = usp - stack_virt_base;
    auto* ret_dst = reinterpret_cast<uptr*>(stack_hhdm_base + ret_offset);
    *ret_dst = SIGNAL_TRAMPOLINE_VADDR;

    trap->rsp = usp;
    trap->rip = reinterpret_cast<uptr>(handler);
    trap->rdi = static_cast<u64>(static_cast<i32>(sig));
    trap->rflags &= ~(1ULL << 8);
}

void signal_dispatch(Unit* u, TrapFrame* trap) {
    const u64 deliverable = u->signals_pending & ~u->signals_masked;
    if (!deliverable) return;

    const u32 signum = __builtin_ctzll(deliverable);
    const auto sig = static_cast<Signal>(signum);
    const SignalAction& action = u->signal_actions[signum];

    if (action.disposition == SignalAction::Disposition::Default ||
        action.disposition == SignalAction::Disposition::Ignore) {
        __sync_and_and_fetch(&u->signals_pending, ~(1ULL << signum));
        if (action.disposition == SignalAction::Disposition::Default) signal_default(u, sig);
        return;
    }

    if (!trap) {
        return;
    }

    __sync_and_and_fetch(&u->signals_pending, ~(1ULL << signum));
    signal_setup_userframe(u, sig, action.handler, trap);
}

void signal_default(Unit* unit, Signal sig) {
    switch (default_action_for(sig)) {
        case DefaultAction::Ignore:
            return;

        case DefaultAction::Terminate:
        case DefaultAction::TerminateCore:
            kernel::scheduling::kill_current_realm(sig, signal_name(sig));
            break;

        case DefaultAction::Stop:
            // TODO: Unit pause
            break;
    }
}