// powermon.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.04.26.
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
#ifndef VESPERAOS_POWERMON_H
#define VESPERAOS_POWERMON_H

/**
 * @brief Entry point for the power-monitor background unit.
 *
 * Subscribes to VBUS_IFACE_POWER (BatteryChanged, AcChanged, LidChanged)
 * and redraws the status bar on every relevant event. Additionally, polls
 * the battery devices every POWER_MONITOR_POLL_MS milliseconds so that
 * slow charge-percentage drift is reflected even when ACPI stays silent.
 */
void power_monitor_unit(void* arg);

#endif  // VESPERAOS_POWERMON_H
