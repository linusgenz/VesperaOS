#include <acpi/acpi_subsystem.h>

#include <arch/x86_64/interrupts/ioapic.h>
#include "../units/unit_manager.h"
#include "vespera/interrupts.h"
#include "vespera/scheduling.h"
#include "vespera/sync/semaphore.h"
#include "vespera/unit_config.h"

extern "C" {
#include "acpica/include/acpi.h"
}

#include <vespera/cpu/io.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/time.h>

struct SciHandlerEntry {
    ACPI_OSD_HANDLER handler = nullptr;
    void* context = nullptr;
    u8 vector = 0xFF;
    u32 irq = 0;
    bool active = false;
};

static SciHandlerEntry g_sci;

static Irqreturn sci_irq_shim(void* cookie) {
    auto* entry = static_cast<SciHandlerEntry*>(cookie);
    if (entry && entry->handler) {
        entry->handler(entry->context);
    }
    return IRQ_HANDLED;
}

ACPI_STATUS AcpiOsInitialize() {
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate() {
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
    return kernel::acpi::get_rsdp_phys();
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

// multithreading

ACPI_THREAD_ID AcpiOsGetThreadId() {
    if (!kernel::scheduling::is_curent_cpu_enabled()) {
        return 1;
    }
    const Unit* u = kernel::scheduling::get_current_unit();
    if (!u) return 1;
    return u->id ? u->id : 1;
}

struct AcpiTaskNode {
    ACPI_OSD_EXEC_CALLBACK fn;
    void* ctx;
    AcpiTaskNode* next;
};

static AcpiTaskNode* s_task_head = nullptr;
static AcpiTaskNode* s_task_tail = nullptr;
static Spinlock* s_task_lock = nullptr;
static Semaphore* s_task_sem = nullptr;  // signaled when task enqueued
static volatile bool s_worker_ready = false;
static volatile u32 g_pending_async_tasks = 0;

static void acpi_worker_thread(void* /*arg*/) {
    s_worker_ready = true;

    while (true) {
        // Wait for a task to arrive
        s_task_sem->wait(ACPI_WAIT_FOREVER);

        // Dequeue
        u64 flags = 0;
        s_task_lock->lock_irqsave(flags);
        AcpiTaskNode* node = s_task_head;
        if (node) {
            s_task_head = node->next;
            if (!s_task_head) s_task_tail = nullptr;
        }
        s_task_lock->unlock_irqrestore(flags);

        if (!node) continue;

        // Execute
        node->fn(node->ctx);
        kernel::memory::free(node);
        __atomic_fetch_sub(&g_pending_async_tasks, 1u, __ATOMIC_RELEASE);
    }
}

void acpi_osl_init_worker() {
    s_task_lock = static_cast<Spinlock*>(kernel::memory::malloc(sizeof(Spinlock)));
    s_task_lock->init("acpi_task_lock");

    s_task_sem = static_cast<Semaphore*>(kernel::memory::malloc(sizeof(Semaphore)));
    s_task_sem->init(1024, 0);

    static constexpr UnitConfig kWorkerCfg = {
        .name = "acpi_worker",
        .cpu_id = 7,
        .priority = 5,
        .stack_size = DEFAULT_UNIT_STACK_SIZE,
        .initial_handles = nullptr,
        .initial_handle_count = 0,
        .is_idle = false,
        .is_user = false,
        .user_stack_size = 0,
        .auto_schedule = true,
        .argv = nullptr,
        .envp = nullptr,
    };

    UnitManager::create(KERNEL_REALM_SYSTEM, acpi_worker_thread, nullptr, &kWorkerCfg);
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK fn, void* context) {
    if (!fn) return AE_BAD_PARAMETER;

    auto* node = static_cast<AcpiTaskNode*>(kernel::memory::malloc(sizeof(AcpiTaskNode)));
    if (!node) return AE_NO_MEMORY;

    node->fn = fn;
    node->ctx = context;
    node->next = nullptr;

    __atomic_fetch_add(&g_pending_async_tasks, 1u, __ATOMIC_RELAXED);

    // Enqueue
    u64 flags = 0;
    s_task_lock->lock_irqsave(flags);
    if (s_task_tail) {
        s_task_tail->next = node;
    } else {
        s_task_head = node;
    }
    s_task_tail = node;
    s_task_lock->unlock_irqrestore(flags);

    // Wake worker
    s_task_sem->signal(1);
    return AE_OK;
}

void AcpiOsWaitEventsComplete() {
    while (__atomic_load_n(&g_pending_async_tasks, __ATOMIC_ACQUIRE) != 0) {
        if (kernel::scheduling::is_curent_cpu_enabled()) {
            kernel::scheduling::yield();
        } else {
            asm volatile("pause");
        }
    }
}

void AcpiOsSleep(UINT64 ms) {
    kernel::time::sleep_ms(ms);
}

void AcpiOsStall(UINT32 us) {
    for (u32 i = 0; i < us * 10; i++) asm volatile("pause");
}

// interrupts

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler, void* context) {
    if (!handler) {
        return AE_BAD_PARAMETER;
    }

    if (g_sci.active) {
        Log::warning("ACPI OSL: SCI handler already installed (IRQ %u), ignoring duplicate", g_sci.irq);
        return AE_ALREADY_EXISTS;
    }

    const u8 vector = kernel::interrupts::get_free_vector();
    if (vector == 0xFF) {
        Log::error("ACPI OSL: No free IDT vector for SCI IRQ %u", irq);
        return AE_NO_MEMORY;
    }

    g_sci.handler = handler;
    g_sci.context = context;
    g_sci.vector = vector;
    g_sci.irq = irq;
    g_sci.active = true;

    kernel::interrupts::allocate_vector(vector, sci_irq_shim, &g_sci);

    const u8 bsp_apic_id = static_cast<u8>(kernel::acpi::madt::bsp_apic_id());
    arch::x86_64::interrupts::ioapic::configure_irq(static_cast<u8>(irq), vector, bsp_apic_id);

    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 irq, ACPI_OSD_HANDLER handler) {
    if (!g_sci.active || g_sci.irq != irq || g_sci.handler != handler) {
        return AE_NOT_EXIST;
    }

    const u32 gsi = irq;
    arch::x86_64::interrupts::ioapic::mask_gsi(gsi);  // TODO cache gsi in SciHandlerEntry

    kernel::interrupts::free_vector(g_sci.vector);

    g_sci = {};
    Log::info("ACPI OSL: SCI IRQ %u handler removed", irq);
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
    // Log::print(fmt, args);
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

// semaphore

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max_units, UINT32 initial_units, ACPI_SEMAPHORE* out) {
    if (!out) return AE_BAD_PARAMETER;

    auto* sem = static_cast<Semaphore*>(kernel::memory::malloc(sizeof(Semaphore)));
    if (!sem) return AE_NO_MEMORY;

    sem->init(max_units, initial_units);
    *out = reinterpret_cast<ACPI_SEMAPHORE>(sem);
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE handle) {
    if (!handle) return AE_BAD_PARAMETER;
    kernel::memory::free(reinterpret_cast<Semaphore*>(handle));
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE handle, UINT32 units, UINT16 timeout_ms) {
    if (!handle) return AE_BAD_PARAMETER;

    auto* sem = reinterpret_cast<Semaphore*>(handle);

    // ACPICA sometimes asks for more than one unit at a time.
    for (UINT32 i = 0; i < units; ++i) {
        if (!sem->wait(timeout_ms)) {
            // Partial acquire: signal back the units we already took.
            if (i > 0) sem->signal(i);
            return AE_TIME;
        }
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE handle, UINT32 units) {
    if (!handle) return AE_BAD_PARAMETER;
    reinterpret_cast<Semaphore*>(handle)->signal(units);
    return AE_OK;
}

// spinlock

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* out) {
    if (!out) return AE_BAD_PARAMETER;

    auto* sl = static_cast<Spinlock*>(kernel::memory::malloc(sizeof(Spinlock)));
    if (!sl) return AE_NO_MEMORY;

    sl->init("acpi_spinlock");
    *out = reinterpret_cast<ACPI_SPINLOCK>(sl);
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK handle) {
    if (handle) kernel::memory::free(reinterpret_cast<Spinlock*>(handle));
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK handle) {
    if (!handle) return 0;

    // Save and clear interrupt flag so the lock is truly IRQ-safe.
    u64 flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags)::"memory");

    reinterpret_cast<Spinlock*>(handle)->lock();
    return static_cast<ACPI_CPU_FLAGS>(flags);
}

void AcpiOsReleaseLock(ACPI_SPINLOCK handle, ACPI_CPU_FLAGS flags) {
    if (!handle) return;

    reinterpret_cast<Spinlock*>(handle)->unlock();

    // Restore interrupt flag from saved state.
    asm volatile("push %0; popfq" ::"r"(static_cast<u64>(flags)) : "memory", "cc");
}

// mutex
/*
ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX* out) {
    if (!out) return AE_BAD_PARAMETER;

    auto* m = static_cast<kernel::Mutex*>(kernel::memory::malloc(sizeof(kernel::Mutex)));
    if (!m) return AE_NO_MEMORY;

    m->init();
    *out = reinterpret_cast<ACPI_MUTEX>(m);
    return AE_OK;
}

void AcpiOsDeleteMutex(ACPI_MUTEX handle) {
    if (handle) kernel::memory::free(reinterpret_cast<kernel::Mutex*>(handle));
}

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX handle, UINT16 timeout_ms) {
    if (!handle) return AE_BAD_PARAMETER;

    auto* m = reinterpret_cast<kernel::Mutex*>(handle);

    if (timeout_ms == 0) {
        return m->try_lock() ? AE_OK : AE_TIME;
    }

    m->lock();
    return AE_OK;
}

void AcpiOsReleaseMutex(ACPI_MUTEX handle) {
    if (handle) reinterpret_cast<kernel::Mutex*>(handle)->unlock();
}
*/
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