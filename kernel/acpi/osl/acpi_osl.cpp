#include "../acpi_manager.h"
extern "C" {
#include "../acpica/include/acpi.h"
}

#include <vespera/cpu/io.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/time.h>

ACPI_STATUS AcpiOsInitialize() {
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate() {
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
    return acpi::rsdp_phys;
}

// memory

void* AcpiOsAllocate(ACPI_SIZE size) {
    return kernel::memory::malloc(static_cast<usize>(size));
}

void AcpiOsFree(void* memory) {
    kernel::memory::free(memory);
}

// mem mapping

void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS phys, ACPI_SIZE length) {
    return virt_ptr(phys_to_virt(make_phys(phys)));
}

void AcpiOsUnmapMemory(void* logical, ACPI_SIZE length) {
    (void)logical;
    (void)length;
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void* logical, ACPI_PHYSICAL_ADDRESS* physical) {
    *physical = static_cast<ACPI_PHYSICAL_ADDRESS>(phys_raw(virt_to_phys(make_virt(logical))));
    return AE_OK;
}

BOOLEAN AcpiOsReadable(void* pointer, ACPI_SIZE length) {
    return TRUE;
}

BOOLEAN AcpiOsWritable(void* pointer, ACPI_SIZE length) {
    return TRUE;
}

ACPI_THREAD_ID AcpiOsGetThreadId() {
    return 1;  // no mt for now, always thread id 1
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK fn, void* context) {
    fn(context);  // always sync
    return AE_OK;
}

void AcpiOsWaitEventsComplete() {
}

void AcpiOsSleep(UINT64 ms) {
    kernel::time::sleep_ms(ms);
}

void AcpiOsStall(UINT32 us) {
    for (volatile u32 i = 0; i < us * 10; i++) asm volatile("pause");
}

// interrupts

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler, void* context) {
    // TODO: via kernel::interrupts::allocate_vector
    (void)irq;
    (void)handler;
    (void)context;
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler) {
    (void)irq;
    (void)handler;
    return AE_OK;
}

// port io

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS address, UINT32* value, UINT32 width) {
    switch (width) {
        case 8:
            *value = inb(static_cast<u16>(address));
            break;
        case 16:
            *value = inw(static_cast<u16>(address));
            break;
        case 32:
            *value = inl(static_cast<u16>(address));
            break;
        default:
            return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS address, UINT32 value, UINT32 width) {
    switch (width) {
        case 8:
            outb(static_cast<u16>(address), static_cast<u8>(value));
            break;
        case 16:
            outw(static_cast<u16>(address), static_cast<u16>(value));
            break;
        case 32:
            outl(static_cast<u16>(address), value);
            break;
        default:
            return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS address, UINT64* value, UINT32 width) {
    void* virt = AcpiOsMapMemory(address, width / 8);
    switch (width) {
        case 8:
            *value = *static_cast<volatile u8*>(virt);
            break;
        case 16:
            *value = *static_cast<volatile u16*>(virt);
            break;
        case 32:
            *value = *static_cast<volatile u32*>(virt);
            break;
        case 64:
            *value = *static_cast<volatile u64*>(virt);
            break;
        default:
            return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS address, UINT64 value, UINT32 width) {
    void* virt = AcpiOsMapMemory(address, width / 8);
    switch (width) {
        case 8:
            *static_cast<volatile u8*>(virt) = static_cast<u8>(value);
            break;
        case 16:
            *static_cast<volatile u16*>(virt) = static_cast<u16>(value);
            break;
        case 32:
            *static_cast<volatile u32*>(virt) = static_cast<u32>(value);
            break;
        case 64:
            *static_cast<volatile u64*>(virt) = value;
            break;
        default:
            return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char* fmt, ...) {
    /*__builtin_va_list args;
    __builtin_va_start(args, fmt);
    Log::print(fmt, args);
    __builtin_va_end(args);*/
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsVprintf(const char* fmt, va_list args) {
    //Log::print(fmt, args);
}

void AcpiOsRedirectOutput(void* destination) {
    (void)destination;
}

// tables

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* existing, ACPI_TABLE_HEADER** newTable) {
    *newTable = nullptr;
    return AE_NO_ACPI_TABLES;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(
    ACPI_TABLE_HEADER* existing, ACPI_PHYSICAL_ADDRESS* newAddress, UINT32* newLength
) {
    *newAddress = 0;
    *newLength = 0;
    return AE_NO_ACPI_TABLES;
}

ACPI_STATUS AcpiOsSignal(UINT32 function, void* info) {
    (void)function;
    (void)info;
    return AE_OK;
}

UINT64 AcpiOsGetTimer() {
    return kernel::time::get_uptime_ms() * 10000ULL;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 sleep_state, UINT32 rega, UINT32 regb) {
    (void)sleep_state;
    (void)rega;
    (void)regb;
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max, UINT32 initial, ACPI_SEMAPHORE* out) {
    *out = reinterpret_cast<ACPI_SEMAPHORE>(1);
    return AE_OK;
}
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE handle) {
    (void)handle;
    return AE_OK;
}
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE handle, UINT32 units, UINT16 timeout) {
    (void)handle;
    (void)units;
    (void)timeout;
    return AE_OK;
}
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE handle, UINT32 units) {
    (void)handle;
    (void)units;
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* out) {
    *out = reinterpret_cast<ACPI_SPINLOCK>(1);
    return AE_OK;
}
void AcpiOsDeleteLock(ACPI_SPINLOCK handle) {
    (void)handle;
}
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK handle) {
    (void)handle;
    return 0;
}
void AcpiOsReleaseLock(ACPI_SPINLOCK handle, ACPI_CPU_FLAGS flags) {
    (void)handle;
    (void)flags;
}

static u32 pci_read_config(u8 bus, u8 dev, u8 func, u32 reg, u32 width) {
    // PCI Config Space via IO ports 0xCF8/0xCFC
    const u32 addr = (1u << 31) | (static_cast<u32>(bus) << 16) | (static_cast<u32>(dev) << 11) |
                     (static_cast<u32>(func) << 8) | (reg & 0xFC);
    outl(0xCF8, addr);

    const u32 val = inl(0xCFC);
    const u32 shift = (reg & 3) * 8;

    switch (width) {
        case 8:
            return (val >> shift) & 0xFF;
        case 16:
            return (val >> shift) & 0xFFFF;
        case 32:
            return val;
        default:
            return 0xFFFFFFFF;
    }
}

static void pci_write_config(u8 bus, u8 dev, u8 func, u32 reg, u64 value, u32 width) {
    const u32 addr = (1u << 31) | (static_cast<u32>(bus) << 16) | (static_cast<u32>(dev) << 11) |
                     (static_cast<u32>(func) << 8) | (reg & 0xFC);
    outl(0xCF8, addr);

    switch (width) {
        case 8:
            outb(0xCFC + (reg & 3), static_cast<u8>(value));
            break;
        case 16:
            outw(0xCFC + (reg & 2), static_cast<u16>(value));
            break;
        case 32:
            outl(0xCFC, static_cast<u32>(value));
            break;
    }
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* id, UINT32 reg, UINT64* value, UINT32 width) {
    *value = pci_read_config(
        static_cast<u8>(id->Bus), static_cast<u8>(id->Device), static_cast<u8>(id->Function), reg, width
    );
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* id, UINT32 reg, UINT64 value, UINT32 width) {
    pci_write_config(
        static_cast<u8>(id->Bus), static_cast<u8>(id->Device), static_cast<u8>(id->Function), reg, value, width
    );
    return AE_OK;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* init_val, ACPI_STRING* new_val) {
    *new_val = nullptr;
    return AE_OK;
}