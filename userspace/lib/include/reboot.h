/**
 * @file reboot.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 06.12.25.
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
#ifndef VESPERAOS_REBOOT_H
#define VESPERAOS_REBOOT_H

/**
 * @brief Possible reboot operation modes.
 *
 * This enumeration defines the action the system should take when `reboot()`
 * is invoked. Higher-level convenience macros (such as `reboot_restart()`)
 * map directly to these modes.
 */
typedef enum {
    REBOOT_MODE_RESTART = 0,   ///< Reboot the system normally.
    REBOOT_MODE_POWER_OFF = 1, ///< Power off the system completely.
    REBOOT_MODE_HALT = 2       ///< Halt the CPU without powering off.
} reboot_mode_t;

/**
 * @brief Perform a system reboot, shutdown or halt.
 *
 * Executes the low-level system call responsible for restarting, shutting
 * down, or halting the machine depending on the requested @p mode.
 *
 * @param mode Reboot mode specifying the type of shutdown action to perform.
 *             See ::reboot_mode_t for available modes.
 *
 * @return @c does not return on success, or a negative error code on failure.
 *
 * @see reboot_restart()
 * @see reboot_poweroff()
 * @see reboot_halt()
 */
int reboot(reboot_mode_t mode);


/**
 * @brief Convenience wrapper to restart the system.
 *
 * Equivalent to calling `reboot(REBOOT_MODE_RESTART)`.
 *
 * @return Same return value as `reboot()`.
 */
#define reboot_restart()   reboot(REBOOT_MODE_RESTART)

/**
 * @brief Convenience wrapper to power off the system.
 *
 * Equivalent to calling `reboot(REBOOT_MODE_POWER_OFF)`.
 *
 * @return Same return value as `reboot()`.
 */
#define reboot_poweroff()  reboot(REBOOT_MODE_POWER_OFF)

/**
 * @brief Convenience wrapper to halt the system.
 *
 * Equivalent to calling `reboot(REBOOT_MODE_HALT)`.
 *
 * @return Same return value as `reboot()`.
 */
#define reboot_halt()      reboot(REBOOT_MODE_HALT)


#endif //VESPERAOS_REBOOT_H