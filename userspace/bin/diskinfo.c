// diskinfo.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 10.03.26.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fflags.h>
#include <vespera/dev/ioctl_smart.h>
#include <vespera/dev/ioctl_devinfo.h>
#include <dirent.h>
#include <errno.h>

static void print_separator() {
    puts("───────────────────────────────────────────────────");
}

static void print_health(uint8_t ok) {
    if (ok)
        puts("  Health:               \033[32mOK\033[0m");
    else
        puts("  Health:               \033[31mFAILED\033[0m");
}

static void print_temp(uint8_t celsius) {
    char buf[64];
    const char* label =
        celsius >= 70 ? "\033[31m" :
        celsius >= 55 ? "\033[33m" :
                        "\033[32m";

    snprintf(buf, sizeof(buf),
        "  %-22s%s%u °C\033[0m\n",
        "Temperature:",
        label,
        celsius);

    puts(buf);
}

static void print_u64(const char* label, uint64_t val) {
    char buf[128];
    snprintf(buf, sizeof(buf), "  %-22s%lu", label, (uint64_t)val);
    puts(buf);
}

static void print_u32(const char* label, uint32_t val) {
    char buf[128];
    snprintf(buf, sizeof(buf), "  %-22s%u", label, val);
    puts(buf);
}

static void print_u8_pct(const char* label, uint8_t val) {
    char buf[128];
    snprintf(buf, sizeof(buf), "  %-22s%u %%", label, val);
    puts(buf);
}

// NVMe display

static void show_nvme(int64_t handle) {
    smart_nvme_t nvme;
    if (ioctl((uint64_t)handle, IOCTL_SMART_GET_NVME, &nvme) < 0) {
        puts("  [Could not retrieve NVMe SMART data]\n");
        return;
    }

    puts("  [NVMe SMART Data]");
    print_temp(nvme.temperature_celsius);
    print_u8_pct("Available Spare:", nvme.available_spare);
    print_u8_pct("Spare Threshold:", nvme.available_spare_threshold);
    print_u8_pct("Percentage Used:", nvme.percentage_used);

    char warn_buf[64];
    snprintf(warn_buf, sizeof(warn_buf), "  %-22s0x%02X\n", "Critical Warning:", nvme.critical_warning_raw);
    puts(warn_buf);

    puts("\n  [NVMe Statistics]");
    print_u64("Data Units Read:", nvme.data_units_read);
    print_u64("Data Units Written:", nvme.data_units_written);
    print_u64("Host Read Cmds:", nvme.host_read_commands);
    print_u64("Host Write Cmds:", nvme.host_write_commands);
    print_u64("Power Cycles:", nvme.power_cycles);
    print_u64("Power-On Hours:", nvme.power_on_hours);
    print_u64("Unsafe Shutdowns:", nvme.unsafe_shutdowns);
    print_u64("Media Errors:", nvme.media_errors);
    print_u64("Error Log Entries:", nvme.error_log_entries);
    print_u32("Warn Temp Time (min):", nvme.warning_temp_time_min);
    print_u32("Crit Temp Time (min):", nvme.critical_temp_time_min);

    // Extra temp sensors
    int has_sensors = 0;
    for (int i = 0; i < 8; i++) {
        if (nvme.temperature_sensor[i] != 0) {
            if (!has_sensors) {
                puts("\n  [Temperature Sensors]\n");
                has_sensors = 1;
            }
            char sbuf[64];
            snprintf(sbuf, sizeof(sbuf), "  Sensor %d:             %d °C", i + 1, nvme.temperature_sensor[i]);
            puts(sbuf);
        }
    }
}

// ATA display

static void show_ata(int64_t handle) {
    smart_ata_t ata;
    if (ioctl((uint64_t)handle, IOCTL_SMART_GET_ATA, &ata) < 0) {
        puts("  [Could not retrieve ATA SMART data]\n");
        return;
    }

    puts("  [ATA SMART Data]\n");
    print_temp(ata.temperature_celsius);
    print_health(ata.health_ok);
    print_u64("Power-On Hours:", ata.power_on_hours);
    print_u32("Power Cycles:", ata.power_cycles);
    print_u32("Reallocated Sectors:", ata.reallocated_sectors);
    print_u32("Pending Sectors:", ata.pending_sectors);
    print_u32("Uncorrectable Sectors:", ata.uncorrectable_sectors);

    if (ata.attr_count > 0) {
        puts("\n  [ATA SMART Attributes]\n");
        puts("  ID   Name (raw id)         Cur  Wst  Thr  Flags   Raw");
        puts("  ====================================================");

        for (uint8_t i = 0; i < ata.attr_count && i < 30; i++) {
            smart_attribute_t* a = &ata.attrs[i];
            if (a->id == 0) continue;

            // Raw value as 48-bit little-endian
            uint64_t raw = 0;
            for (int b = 5; b >= 0; b--)
                raw = (raw << 8) | a->raw[b];

            char row[128];
            snprintf(row, sizeof(row),
                "  %-4u %-20s %-4u %-4u %-4u 0x%04X  %llu",
                a->id, "---", a->current, a->worst, a->threshold, a->flags,
                (unsigned long long)raw);
            puts(row);
        }
    }
}

// common display

static void show_common(int64_t handle) {
    smart_common_t common;
    if (ioctl((uint64_t)handle, IOCTL_SMART_GET_COMMON, &common) < 0) {
        puts("  [Could not retrieve common SMART data]\n");
        return;
    }

    const char* dtype = "Unknown";
    if (common.driver_type == SMART_DRIVER_NVME) dtype = "NVMe";
    else if (common.driver_type == SMART_DRIVER_ATA) dtype = "ATA/SATA";

    char buf[64];
    snprintf(buf, sizeof(buf), "  Type:                 %s\n", dtype);
    puts(buf);

    print_health(common.health_ok);
    print_temp(common.temperature_celsius);
    print_u64("Power-On Hours:", common.power_on_hours);

    if (common.critical_warning_raw) {
        char wbuf[64];
        snprintf(wbuf, sizeof(wbuf), "  Critical Warning:     0x%02X\n", common.critical_warning_raw);
        puts(wbuf);
    }
}

static void show_devinfo(int64_t handle) {
    devinfo_t info;
    if (ioctl((uint64_t)handle, IOCTL_DEVINFO_GET_ALL, &info) < 0)
        return;

    if (info.model[0] == '\0' && info.serial[0] == '\0' &&
        info.vendor[0] == '\0' && info.firmware[0] == '\0')
        return;

    puts("  [Device Information]\n");

    char buf[192];
    if (info.vendor[0])   { snprintf(buf, sizeof(buf), "  %-22s%s", "Vendor:",   info.vendor);   puts(buf); }
    if (info.model[0])    { snprintf(buf, sizeof(buf), "  %-22s%s", "Model:",    info.model);    puts(buf); }
    if (info.serial[0])   { snprintf(buf, sizeof(buf), "  %-22s%s", "Serial:",   info.serial);   puts(buf); }
    if (info.firmware[0]) { snprintf(buf, sizeof(buf), "  %-22s%s", "Firmware:", info.firmware); puts(buf); }

    puts("\n");
}

static void inspect_device(const char* dev_path) {
    print_separator();

    char header[128];
    snprintf(header, sizeof(header), "Device: %s", dev_path);
    puts(header);
    print_separator();
    printf("\n");

    int64_t handle = open(dev_path, O_RDONLY);
    if (handle < 0) {
        printf("  Cannot open device (error %ld)\n", handle);
        return;
    }

    show_devinfo(handle);

    smart_common_t common;
    int64_t rc = ioctl((uint64_t)handle, IOCTL_SMART_GET_COMMON, &common);
    if (rc < 0) {
        printf("  No SMART support detected for this device. (error=%ld)\n", rc);
        close(handle);
        return;
    }

    show_common(handle);
    puts("\n");

    if (common.driver_type == SMART_DRIVER_NVME) {
        show_nvme(handle);
    } else if (common.driver_type == SMART_DRIVER_ATA) {
        show_ata(handle);
    } else {
        // Try both, show whichever works
        smart_nvme_t nvme;
        if (ioctl((uint64_t)handle, IOCTL_SMART_GET_NVME, &nvme) == 0) {
            show_nvme(handle);
        } else {
            smart_ata_t ata;
            if (ioctl((uint64_t)handle, IOCTL_SMART_GET_ATA, &ata) == 0)
                show_ata(handle);
        }
    }

    close(handle);
    puts("\n");
}


static void usage() {
    puts("Usage: diskinfo [device]\n");
    puts("       diskinfo /dev/nvme0n1   - inspect a specific device\n");
}

int main(int argc, const char** argv) {
    puts("\033[1mdiskinfo - VesperaOS Disk Health Monitor\033[0m\n");

    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage();
            return 0;
        }
        inspect_device(argv[1]);
    }

    return 0;
}