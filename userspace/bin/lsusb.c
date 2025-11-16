// lsusb.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 21.09.25.
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

#include <fflags.h>
#include <realm.h>
#include <stdio.h>
#include <sysstd.h>
#include <dev/usb_xhci_ioctl.h>
#include <sys/ioctl.h>

#include "stddef.h"
#include "stdint.h"

#define MAX_INPUT 256

void main(int argc, char **argv) {
    FILE_HANDLE bus_hdl = fopen("/dev/xhci/", O_RDONLY);
    if (bus_hdl < 0) {
        printf("lsusb: cannot open /dev/xhci/ (hdl=%lld)\n", (long long) bus_hdl);
        exit(-1);
        return;
    }

    char entry[128];
    char devname[128];

    printf("lsusb: lsusb started\n");

    while (sys_readdir(bus_hdl, (uint64_t) devname, sizeof(devname), 0, 0, 0) > 0) {
        // build controller path e.g. /dev/xhci/xhci0
        char ctrl_path[256];
        snprintf(ctrl_path, sizeof(ctrl_path), "%s%s", "/dev/xhci/", devname);

        FILE_HANDLE ctrl_hdl = fopen(ctrl_path, O_RDONLY);
        if (ctrl_hdl < 0) {
            printf("lsusb: cannot open controller %s (hdl=%lld)\n", ctrl_path, (long long) ctrl_hdl);
            continue;
        }

        // ask controller how many devices it reports
        size_t dev_count = 0;
        int rc = ioctl(ctrl_hdl, XHCI_IOCTL_GET_COUNT, &dev_count);
        if (rc != 0) {
            printf("%s: ioctl(GET_COUNT) failed (%d)\n", ctrl_path, rc);
            fclose(ctrl_hdl);
            continue;
        }

        if (dev_count == 0) {
            fclose(ctrl_hdl);
            continue;
        }

        // iterate potential slot ids and collect dev_count devices
        int found = 0;
        const int MAX_SLOTS = 64; // gleiche Grenze wie Treiber (sicher)
        for (int slot = 0; slot < MAX_SLOTS && found < (int) dev_count; ++slot) {
            xhci_device_stat stat;
            // set slot_id field as query parameter
            stat.slot_id = (uint8_t) slot;

            int64_t r2 = ioctl(ctrl_hdl, XHCI_IOCTL_GET_DEVICE, &stat);
            if (r2 == 0) {
                printf("Bus %u Port %u: ID %04x:%04x Speed %u %s %s %s\n",
                       (unsigned) stat.bus_number, (unsigned) stat.port_num,
                       stat.vendor_id, stat.product_id,
                       (unsigned) stat.speed,
                       stat.manufacturer[0] ? stat.manufacturer : "",
                       stat.product[0] ? stat.product : "",
                       stat.serial_number[0] ? stat.serial_number : ""
                );
                found++;
            } else {
            }
        }

        if (found == 0) {
            printf("%s: device count %lu but could not fetch device entries\n", ctrl_path, dev_count);
        }

        fclose(ctrl_hdl);
    }

    fclose(bus_hdl);
}
