#include "cpu_manager.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "../include/page_table_manager.h"
#include "../include/page_frame_allocator.h"
#include "../include/memory.h"
#include "../../include/log.h"
#include "../acpi/madt.h"
#include "../time/time.h"

namespace CPUManager {
    
    // Globale Variablen
    CPUInfo cpu_infos[MAX_CPU_CORES];
    uint32_t total_cpus;
    static uint32_t online_cpus;
    static bool is_initialized = false;
    static uint32_t bsp_apic_id;
    
    // AP Startup Variablen
    static uint64_t ap_startup_stack = 0;
    static uint64_t ap_startup_page_table;
    static uint64_t ap_startup_entry_point = 0;
    
    void initialize() {
        if (is_initialized) return;
        
        // Initialisiere Stack Manager
        StackManager::initialize();
        
        // Hole CPU-Informationen von MADT
        MADT::CPUCore* madt_cores = MADT::get_cpu_cores();
        uint32_t madt_cpu_count = MADT::get_cpu_count();
        bsp_apic_id = MADT::get_bsp_apic_id();
        
        if (madt_cpu_count == 0) {
            Log::Error("No CPUs found in MADT");
            return;
        }
        
        // Initialisiere CPU-Infos
        for (uint32_t i = 0; i < madt_cpu_count && i < MAX_CPU_CORES; i++) {
            cpu_infos[i].apic_id = madt_cores[i].apic_id;
            cpu_infos[i].cpu_id = i;
            cpu_infos[i].state = CPU_STATE_OFFLINE;
            cpu_infos[i].kernel_stack = nullptr;
            cpu_infos[i].total_cycles = 0;
            cpu_infos[i].idle_cycles = 0;
            cpu_infos[i].current_task_id = 0;
            cpu_infos[i].is_bsp = madt_cores[i].is_bsp;
            
            // Allokiere Kernel-Stack für jeden CPU
            cpu_infos[i].kernel_stack = StackManager::allocate_kernel_stack(i);
            if (!cpu_infos[i].kernel_stack) {
                Log::Error("Failed to allocate kernel stack for CPU %u", i);
                continue;
            }
            
            // BSP ist bereits online
            if (cpu_infos[i].is_bsp) {
                cpu_infos[i].state = CPU_STATE_ONLINE;
                online_cpus++;
            }
            
            total_cpus++;
        }
        
        is_initialized = true;
        Log::Ok("CPU Manager initialized - %u CPUs detected, %u online", total_cpus, online_cpus);
    }
    
    void setup_ap_startup_code() {
        // Für UEFI Long Mode: Setze nur die Variablen
        ap_startup_page_table = (uint64_t)global_page_table_manager.PML4;
        
        Log::Info("AP startup variables configured for Long Mode");
    }
    
    bool send_init_ipi(uint32_t apic_id) {
        // Prüfe ob APIC bereit ist
        uint32_t icr_low = lapic_read(LAPIC_ICRLO);
        if (icr_low & (1 << 12)) { // Delivery Status Bit
            Log::Warning("APIC busy, cannot send INIT IPI to %u", apic_id);
            return false;
        }
        
        // INIT IPI senden
        lapic_write(LAPIC_ICRHI, apic_id << 24);
        lapic_write(LAPIC_ICRLO, 0x4500); // INIT, Physical, Assert
        
        // Warte 10ms mit proper sleep
        pmt_delay(10000); // 10ms = 10000 microseconds
        
        // INIT De-assert
        lapic_write(LAPIC_ICRHI, apic_id << 24);
        lapic_write(LAPIC_ICRLO, 0x8500); // INIT, Physical, De-assert
        
        // Warte 10ms
        pmt_delay(10000); // 10ms = 10000 microseconds
        
        // Prüfe ob IPI gesendet wurde
        icr_low = lapic_read(LAPIC_ICRLO);
        if (icr_low & (1 << 12)) {
            Log::Warning("INIT IPI to %u still pending", apic_id);
            return false;
        }
        
        return true;
    }
    
    bool send_sipi(uint32_t apic_id) {
        lapic_write(LAPIC_ESR, 0);

        // 1. Send INIT IPI
        Log::LogMsg("INIT IPI to %u", apic_id);
        lapic_write(LAPIC_ICRHI, apic_id << 24);
        lapic_write(LAPIC_ICRLO, ICR_INIT | ICR_ASSERT | ICR_LEVEL);
        wait_for_delivery();

        kernel::time::sleep_ms(10);

        // INIT deassert
        Log::LogMsg("INIT to %u", apic_id);
        lapic_write(LAPIC_ICRHI, apic_id << 24);
        lapic_write(LAPIC_ICRLO, ICR_INIT | ICR_DEASSERT | ICR_LEVEL);
        wait_for_delivery();

         kernel::time::sleep_ms(10);

        // first SIPI
        Log::LogMsg("first Sipi");
        lapic_write(LAPIC_ICRHI, apic_id << 24);
        lapic_write(LAPIC_ICRLO, ICR_SIPI | (0x8000 >> 12));
        wait_for_delivery();
        while (true);
         kernel::time::sleep_ms(10);

        // second SIPI
        Log::LogMsg("second Sipi");
        lapic_write(LAPIC_ICRHI, apic_id << 24);
        lapic_write(LAPIC_ICRLO, ICR_SIPI | (0x8000 >> 12));
        wait_for_delivery();

        // Check for errors
        uint32_t esr = lapic_read(LAPIC_ESR);
        if (esr != 0) {
            Log::Error("lapic_read error!");
            return false;
        }
        

        Log::Info("SIPI to %u completed successfully", apic_id);
        while (true);
        return true;
    }
    
    bool start_ap(uint32_t apic_id) {
        CPUInfo* cpu_info = nullptr;
        for (uint32_t i = 0; i < total_cpus; i++) {
            if (cpu_infos[i].apic_id == apic_id) {
                cpu_info = &cpu_infos[i];
                break;
            }
        }
        
        if (!cpu_info || cpu_info->is_bsp) {
            Log::Warning("Cannot start AP with APIC ID %u", apic_id);
            return false;
        }
        
        if (cpu_info->state != CPU_STATE_OFFLINE) {
            Log::Warning("CPU %u (APIC %u) is not offline", cpu_info->cpu_id, apic_id);
            return false;
        }
        
        cpu_info->state = CPU_STATE_STARTING;
        
        Log::Info("Starting AP %u (APIC ID %u)...", cpu_info->cpu_id, apic_id);
        
        // Sende INIT IPI
        if (!send_init_ipi(apic_id)) {
            Log::Error("Failed to send INIT IPI to APIC %u", apic_id);
            cpu_info->state = CPU_STATE_OFFLINE;
            return false;
        }

        Log::Info("sending sipi");

        // Sende SIPI
        if (!send_sipi(apic_id)) {
            Log::Error("Failed to send SIPI to APIC %u", apic_id);
            cpu_info->state = CPU_STATE_OFFLINE;
            return false;
        }
        
        Log::Info("SIPI sent to AP %u - waiting for response...", apic_id);

        // Warte auf AP-Start mit kürzerem Timeout
        /*uint32_t cycles = 0;
        uint32_t timeout_cycles = 2000000; // 1 Million Zyklen (kürzer)
        
        while (cpu_info->state == CPU_STATE_STARTING && cycles < timeout_cycles) {
            cycles++;

            if (cpu_info->state == CPU_STATE_ONLINE) {
                cpu_info->state = CPU_STATE_ONLINE;
                asm volatile("cli; hlt");
            }

            
            if (cycles % 100000 == 0) {
                Log::Info("Waiting for AP %u... (cycles: %u)", apic_id, cycles);
            }
        }
        
        if (cpu_info->state == CPU_STATE_ONLINE) {
            // WICHTIG: Erhöhe online_cpus erst NACH der while-Schleife
            online_cpus++;
            Log::Ok("Successfully started AP %u (APIC ID %u)", cpu_info->cpu_id, apic_id);
            
            // Kurze Verzögerung um sicherzustellen, dass der AP stabil ist
            for (volatile int i = 0; i < 10000; i++);
            
            return true;
        } else {
            Log::Error("Failed to start AP %u (APIC ID %u) - final state: %u", 
                      cpu_info->cpu_id, apic_id, (uint32_t)cpu_info->state);
            cpu_info->state = CPU_STATE_OFFLINE;
            return false;
        }*/
    }
    
    void start_all_aps() {
        if (!is_initialized) {
            Log::Error("CPU Manager not initialized");
            return;
        }
        
        // Setup AP startup code
        setup_ap_startup_code();
        
        Log::Info("Starting all Application Processors...");
        
        uint32_t started_aps = 0;
        for (uint32_t i = 0; i < total_cpus; i++) {
            if (!cpu_infos[i].is_bsp && cpu_infos[i].state == CPU_STATE_OFFLINE) {
                Log::Info("Attempting to start AP %u (APIC ID %u)", cpu_infos[i].cpu_id, cpu_infos[i].apic_id);
                
                if (start_ap(cpu_infos[i].apic_id)) {
                    started_aps++;
                    
                    // WICHTIG: Verzögerung zwischen AP-Starts um Race Conditions zu vermeiden
                    Log::Info("AP %u started successfully, waiting before next AP...", cpu_infos[i].cpu_id);
                    for (volatile int j = 0; j < 50000; j++); // Kurze Verzögerung
                } else {
                    Log::Error("Failed to start AP %u", cpu_infos[i].cpu_id);
                }
            }
        }
        
        Log::Info("Started %u out of %u Application Processors", started_aps, total_cpus - 1);
        
        // Finale Verzögerung um sicherzustellen, dass alle APs stabil sind
        Log::Info("All AP startup attempts completed, system should be stable now");
    }
    
    CPUInfo* get_cpu_info(uint32_t apic_id) {
        if (!is_initialized) return nullptr;
        
        for (uint32_t i = 0; i < total_cpus; i++) {
            if (cpu_infos[i].apic_id == apic_id) {
                return &cpu_infos[i];
            }
        }
        return nullptr;
    }
    
    uint32_t get_current_cpu_id() {
        if (!is_initialized) return 0;
        
        uint32_t current_apic_id = LocalApicGetId();
        
        for (uint32_t i = 0; i < total_cpus; i++) {
            if (cpu_infos[i].apic_id == current_apic_id) {
                return cpu_infos[i].cpu_id;
            }
        }
        
        return 0; // Default zu BSP
    }
    
    uint32_t get_online_cpu_count() {
        return online_cpus;
    }
    
    uint32_t get_available_cpu_count() {
        return total_cpus;
    }
    
    void halt_cpu(uint32_t apic_id) {
        CPUInfo* cpu_info = get_cpu_info(apic_id);
        if (cpu_info) {
            cpu_info->state = CPU_STATE_HALTED;
        }
    }
    
    void send_ipi(uint32_t target_apic_id, uint32_t vector) {
        lapic_write(LAPIC_ICRHI, target_apic_id << 24);
        lapic_write(LAPIC_ICRLO, 0x4000 | vector); // Fixed, Physical, vector
    }
    
    void send_ipi_to_all_aps(uint32_t vector) {
        // Broadcast IPI an alle APs (außer BSP)
        lapic_write(LAPIC_ICRHI, 0);
        lapic_write(LAPIC_ICRLO, 0xC0000 | vector); // All except self, vector
    }
    
    void print_cpu_info() {
        if (!is_initialized) {
            Log::Info("CPU Manager not initialized");
            return;
        }
        
        Log::Info("=== CPU Manager Info ===");
        Log::Info("Total CPUs: %u, Online: %u", total_cpus, online_cpus);
        Log::Info("BSP APIC ID: %u", bsp_apic_id);
        
        for (uint32_t i = 0; i < total_cpus; i++) {
            const char* state_str;
            switch (cpu_infos[i].state) {
                case CPU_STATE_OFFLINE: state_str = "OFFLINE"; break;
                case CPU_STATE_STARTING: state_str = "STARTING"; break;
                case CPU_STATE_ONLINE: state_str = "ONLINE"; break;
                case CPU_STATE_HALTED: state_str = "HALTED"; break;
                default: state_str = "UNKNOWN"; break;
            }
            
            Log::Info("CPU %u: APIC ID %u, %s, %s, Stack: 0x%p", 
                      cpu_infos[i].cpu_id,
                      cpu_infos[i].apic_id,
                      cpu_infos[i].is_bsp ? "BSP" : "AP",
                      state_str,
                      cpu_infos[i].kernel_stack ? cpu_infos[i].kernel_stack->stack_top : nullptr);
        }
    }
    
    void update_cpu_stats(uint32_t apic_id, uint64_t cycles, uint64_t idle_cycles) {
        CPUInfo* cpu_info = get_cpu_info(apic_id);
        if (cpu_info) {
            cpu_info->total_cycles = cycles;
            cpu_info->idle_cycles = idle_cycles;
        }
    }
    
    double get_cpu_usage(uint32_t apic_id) {
        CPUInfo* cpu_info = get_cpu_info(apic_id);
        if (!cpu_info || cpu_info->total_cycles == 0) {
            return 0.0;
        }
        
        uint64_t active_cycles = cpu_info->total_cycles - cpu_info->idle_cycles;
        return ((double)active_cycles / (double)cpu_info->total_cycles) * 100.0;
    }
}
