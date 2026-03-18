/**
 * lsusb.c — VesperaOS USB device lister
 *
 * Iterates /dev, probes each entry with IOCTL_USB_GET_DEVICE_INFO,
 * and prints controllers + devices like real lsusb.
 *
 * Build:
 *   gcc -o lsusb lsusb.c -I/path/to/vespera/include
 *
 * Usage:
 *   lsusb          — compact list (one line per device)
 *   lsusb -v       — verbose (full descriptor info per device)
 *   lsusb -t       — tree view: controllers at root, devices indented
 */

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <vespera/dev/ioctl_devinfo.h>
#include <vespera/dev/ioctl_usb_device.h>

#include "vespera/dev/ioctl_devinfo.h"
#include "vespera/dev/ioctl_usb_device.h"
#include "vespera/fflags.h"

#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_DIM     "\033[2m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_CYAN    "\033[36m"
#define C_BLUE    "\033[34m"

#define MAX_DEVICES 128
#define DEV_DIR     "/dev"

/* slot_id == 0 is our sentinel for "this is a controller, not a USB device".
   Real xHCI slot IDs start at 1. */
#define SLOT_ID_CONTROLLER 0

static const char* speed_str(uint8_t s) {
    switch (s) {
        case USB_SPEED_LOW_SPEED:        return "1.5 Mb/s  (LS)";
        case USB_SPEED_FULL_SPEED:       return "12 Mb/s   (FS)";
        case USB_SPEED_HIGH_SPEED:       return "480 Mb/s  (HS)";
        case USB_SPEED_SUPER_SPEED:      return "5 Gb/s    (SS)";
        case USB_SPEED_SUPER_SPEED_PLUS: return "10 Gb/s   (SS+)";
        default:                         return "-";
    }
}

static const char* class_str(uint8_t c) {
    switch (c) {
        case 0x00: return "Device-defined";
        case 0x01: return "Audio";
        case 0x02: return "CDC";
        case 0x03: return "HID";
        case 0x06: return "Image";
        case 0x07: return "Printer";
        case 0x08: return "Mass Storage";
        case 0x09: return "Hub";
        case 0x0A: return "CDC-Data";
        case 0x0E: return "Video";
        case 0xE0: return "Wireless";
        case 0xFF: return "Vendor Specific";
        default:   return "Unknown";
    }
}

typedef struct {
    char              dev_path[128];
    char              dev_name[64];
    char              model[128];
    char              vendor[128];
    char              serial[128];
    char              firmware[16];
    usb_device_info_t usb;
    int               is_controller;
} UsbEntry;

static int probe(const char* path, const char* name, UsbEntry* out) {
    HANDLE fd = open(path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    usb_device_info_t usb;
    memset(&usb, 0, sizeof(usb));

    if (ioctl(fd, IOCTL_USB_GET_DEVICE_INFO, &usb) != 0) {
        close(fd);
        return 0;
    }

    devinfo_t info;
    memset(&info, 0, sizeof(info));
    ioctl(fd, IOCTL_DEVINFO_GET_ALL, &info);

    close(fd);

    memset(out, 0, sizeof(*out));
    strncpy(out->dev_path, path,         sizeof(out->dev_path)  - 1);
    strncpy(out->dev_name, name,         sizeof(out->dev_name)  - 1);
    strncpy(out->model,    info.model,   sizeof(out->model)     - 1);
    strncpy(out->vendor,   info.vendor,  sizeof(out->vendor)    - 1);
    strncpy(out->serial,   info.serial,  sizeof(out->serial)    - 1);
    strncpy(out->firmware, info.firmware,sizeof(out->firmware)  - 1);
    out->usb           = usb;
    out->is_controller = (usb.slot_id == SLOT_ID_CONTROLLER);
    return 1;
}

/* ---- sorting: by bus, controllers first, then by slot ------------------- */
static int cmp_entries(const void* a, const void* b) {
    const UsbEntry* ea = (const UsbEntry*)a;
    const UsbEntry* eb = (const UsbEntry*)b;
    if (ea->usb.bus_number != eb->usb.bus_number)
        return (int)ea->usb.bus_number - (int)eb->usb.bus_number;
    if (ea->is_controller != eb->is_controller)
        return eb->is_controller - ea->is_controller;
    return (int)ea->usb.slot_id - (int)eb->usb.slot_id;
}

/* ---- compact line -------------------------------------------------------- */
static void print_compact(const UsbEntry* e) {
    if (e->is_controller) {
        printf("Bus %03u Device 001: ID %04x:%04x  "
               C_BOLD C_BLUE "%-20s" C_RESET "  %s"
               C_DIM "  [controller]" C_RESET "\n",
               e->usb.bus_number,
               e->usb.vendor_id,
               e->usb.product_id,
               e->vendor[0] ? e->vendor : "Unknown Vendor",
               e->model[0]  ? e->model  : "xHCI Host Controller");
    } else {
        printf("Bus %03u Device %03u: ID %04x:%04x  "
               C_BOLD "%-20s" C_RESET "  %s\n",
               e->usb.bus_number,
               e->usb.slot_id,
               e->usb.vendor_id,
               e->usb.product_id,
               e->vendor[0] ? e->vendor : "Unknown Vendor",
               e->model[0]  ? e->model  : "Unknown Device");
    }
}

/* ---- verbose block ------------------------------------------------------- */
static void print_verbose(const UsbEntry* e) {
    printf("\n");
    if (e->is_controller)
        printf(C_BOLD C_BLUE "[Controller]" C_RESET "  %s\n", e->dev_path);
    else
        printf(C_BOLD C_CYAN "[Device]" C_RESET "      %s\n", e->dev_path);

    printf("  %-22s %s%s%s  %s\n",
           "Vendor / Model:",
           C_GREEN,
           e->vendor[0] ? e->vendor : "(unknown)",
           C_RESET,
           e->model[0]  ? e->model  : "(unknown)");

    if (e->serial[0])
        printf("  %-22s %s\n", "Serial:",   e->serial);
    if (e->firmware[0])
        printf("  %-22s %s\n", "Firmware:", e->firmware);

    printf("  %-22s " C_YELLOW "%04x" C_RESET ":" C_YELLOW "%04x" C_RESET "\n",
           "ID (VID:PID):", e->usb.vendor_id, e->usb.product_id);

    if (!e->is_controller) {
        printf("  %-22s bcdUSB=%u.%02u  bcdDevice=%u.%02u\n",
               "USB version:",
               e->usb.bcd_usb    >> 8, e->usb.bcd_usb    & 0xFF,
               e->usb.bcd_device >> 8, e->usb.bcd_device & 0xFF);
        printf("  %-22s %s\n", "Speed:", speed_str(e->usb.speed));
    }

    printf("  %-22s %s%s\n",
       "Power:",
       (e->usb.bm_attributes & 0x40) ? "Self-powered" : "Bus-powered",
       (e->usb.bm_attributes & 0x20) ? ", Remote-wakeup" : "");

    printf("  %-22s %umA\n",
           "Max power:",
           e->usb.b_max_power * 2);

    printf("  %-22s %u\n",
           "Max packet size:",
           e->usb.b_max_packet_size0);

    printf("  %-22s %u\n",
           "Active config:",
           e->usb.b_configuration_value);

    printf("  %-22s Bus %u", "Topology:", e->usb.bus_number);
    if (!e->is_controller)
        printf(", Port %u, Slot %u", e->usb.port_num, e->usb.slot_id);
    printf("\n");

    printf("  %-22s %s (0x%02x",
           "Class:", class_str(e->usb.b_device_class), e->usb.b_device_class);
    if (!e->is_controller)
        printf(" / 0x%02x / 0x%02x",
               e->usb.b_device_subclass, e->usb.b_device_protocol);
    printf(")\n");

    if (!e->is_controller)
        printf("  %-22s %u configuration(s), %u interface(s)\n",
               "Configurations:",
               e->usb.num_configurations,
               e->usb.num_interfaces);
}

/* ---- tree view ----------------------------------------------------------- */
static void print_tree(UsbEntry* entries, int count) {
    uint8_t buses[32];
    int     nbus = 0;

    for (int i = 0; i < count; i++) {
        uint8_t b = entries[i].usb.bus_number;
        int found = 0;
        for (int j = 0; j < nbus; j++)
            if (buses[j] == b) { found = 1; break; }
        if (!found && nbus < 32) buses[nbus++] = b;
    }

    for (int bi = 0; bi < nbus; bi++) {
        uint8_t bus = buses[bi];

        /* find controller for this bus */
        const UsbEntry* ctrl = NULL;
        for (int i = 0; i < count; i++)
            if (entries[i].usb.bus_number == bus && entries[i].is_controller)
                { ctrl = &entries[i]; break; }

        /* Bus / controller header */
        printf(C_BOLD C_BLUE "/:  Bus %02u" C_RESET, bus);
        if (ctrl) {
            printf("  " C_BOLD "%s" C_RESET "  " C_DIM "%s  (%s)" C_RESET,
                   ctrl->vendor[0] ? ctrl->vendor : "Unknown",
                   ctrl->model[0]  ? ctrl->model  : "xHCI Host Controller",
                   ctrl->dev_name);
        }
        printf("\n");

        /* devices on this bus, sorted by slot */
        int dev_idx[MAX_DEVICES];
        int ndev = 0;
        for (int i = 0; i < count; i++)
            if (entries[i].usb.bus_number == bus && !entries[i].is_controller)
                dev_idx[ndev++] = i;

        if (ndev == 0) {
            printf("    " C_DIM "(no devices connected)" C_RESET "\n");
        } else {
            for (int di = 0; di < ndev; di++) {
                const UsbEntry* e  = &entries[dev_idx[di]];
                int             last = (di == ndev - 1);

                printf("    %s%-3s%s "
                       C_DIM "Port %-2u  Slot %-2u" C_RESET
                       "  ID %04x:%04x"
                       "  " C_BOLD "%-18s" C_RESET "  %-16s"
                       "  " C_DIM "[%s]" C_RESET "\n",
                       C_DIM, last ? "\\__" : "|__", C_RESET,
                       e->usb.port_num,
                       e->usb.slot_id,
                       e->usb.vendor_id,
                       e->usb.product_id,
                       e->vendor[0] ? e->vendor : "?",
                       e->model[0]  ? e->model  : "?",
                       speed_str(e->usb.speed));
            }
        }
        printf("\n");
    }
}

/* ---- main ---------------------------------------------------------------- */
int main(int argc, char* argv[]) {
    int verbose = 0;
    int tree    = 0;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-v") == 0) verbose = 1;
        else if (strcmp(argv[i], "-t") == 0) tree    = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: lsusb [-v] [-t]\n"
                   "  -v   verbose output (full descriptor info per device)\n"
                   "  -t   tree view (controllers as roots, devices indented)\n");
            return 0;
        }
    }

    DIR_HANDLE dir = opendir(DEV_DIR);
    if (!dir) { printf("lsusb: cannot open " DEV_DIR); return 1; }

    UsbEntry* entries = (UsbEntry*)calloc(MAX_DEVICES, sizeof(UsbEntry));
    if (!entries) { printf("lsusb: malloc"); return 1; }

    int count = 0;
    dirent_t de;
    while ((0 != readdir(dir, &de)) && count < MAX_DEVICES) {
        if (de.name[0] == '.') continue;
        char path[320];
        snprintf(path, sizeof(path), "%s/%s", DEV_DIR, de.name);
        if (probe(path, de.name, &entries[count])) {
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("lsusb: no USB devices found\n");
        free(entries);
        return 0;
    }

    for (int i = 1; i < count; i++) {
        UsbEntry tmp = entries[i];
        int j = i - 1;
        while (j >= 0 && cmp_entries(&entries[j], &tmp) > 0) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = tmp;
    }

    if (tree) {
        print_tree(entries, count);
    } else if (verbose) {
        for (int i = 0; i < count; i++)
            print_verbose(&entries[i]);
        printf("\n");
    } else {
        for (int i = 0; i < count; i++)
            print_compact(&entries[i]);

        int nctrl = 0, ndev = 0;
        for (int i = 0; i < count; i++)
            entries[i].is_controller ? nctrl++ : ndev++;
        printf("\n%d controller(s), %d device(s)\n", nctrl, ndev);
    }

    free(entries);
    return 0;
}