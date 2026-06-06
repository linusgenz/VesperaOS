// astra.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 29.05.26.
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

/**
 * @file astra.h
 * @brief Astra desktop icon loader.
 *
 * Reads all *.astrum files from /etc/astra, parses their key-value pairs,
 * and registers a desktop icon for each valid entry via desktop_add_icon().
 *
 * .astrum format (all keys required unless noted):
 *
 *   name=My App          # display label shown below the icon
 *   bin=/bin/myapp       # absolute path to the executable
 *   color=0xFF4A3B5C     # fallback icon background colour (0xRRGGBB or 0xAARRGGBB)
 *   icon=myapp           # optional, name of the image file in /usr/share/icons. for myapp e.g. myapp.png
 *                        # the recommended size for the icons is 40x40, larger icons get downscaled
 *
 * Lines starting with '#' are treated as comments and ignored.
 * Unknown keys are silently ignored for forward-compatibility.
 * If 'name' or 'bin' are missing the file is skipped with a warning.
 * 'color' defaults to VESPERA_BLUE when absent.
 */


#ifndef VESPERAOS_ASTRA_H
#define VESPERAOS_ASTRA_H

/**
 * Scan /etc/astra for *.astrum files and register an icon for each one.
 *
 * @return Number of icons successfully loaded (0 if directory is empty or
 *         missing, negative on a fatal I/O error opening the directory).
 */
int desktop_astra_load(void);

#endif  // VESPERAOS_ASTRA_H