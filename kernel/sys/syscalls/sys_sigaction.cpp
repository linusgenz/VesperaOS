// sys_sigaction.cpp
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

#include <uapi/vespera/signal.h>
#include <vespera/log.h>
#include <vespera/scheduling.h>
#include <kernel/units/unit.h>
#include <vespera/signals.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_sigaction(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const i32 signum = static_cast<i32>(arg0);
        const auto user_act = reinterpret_cast<const sigaction_t*>(arg1);

        if (!is_valid_signal(signum)) return -EINVAL;
        if (!user_act) return -EINVAL;

        if (static_cast<Signal>(signum) == Signal::SIGKILL) return -EINVAL;

        Unit* u = kernel::scheduling::get_current_unit();
        if (!u) return -EINVAL;

        SignalAction& action = u->signal_actions[signum];

        const uptr handler_addr = reinterpret_cast<uptr>(user_act->handler);

        if (handler_addr == 0) {
            action.disposition = SignalAction::Disposition::Default;
            action.handler = nullptr;
        } else if (handler_addr == 1) {
            action.disposition = SignalAction::Disposition::Ignore;
            action.handler = nullptr;
        } else {
            action.disposition = SignalAction::Disposition::Handler;
            action.handler = reinterpret_cast<void (*)(int)>(handler_addr);
        }

        u->signals_masked |= user_act->sa_mask;

        return SUCCESS_CODE;
    }
}  // namespace syscalls::internal