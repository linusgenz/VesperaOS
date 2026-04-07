//
// Created by linus on 07.07.25.
//

#include "nvme.h"

#include <vespera/devices/device_manager.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/time.h>

#include <vespera/filesystem/devfs.h>
#include "vespera/scheduling.h"
#include "vespera_errno.h"

namespace nvme {
    NvmeDriver::NvmeDriver(pci::PCI_DEVICE_HEADER* pci_base_address) {
        const auto* pci = reinterpret_cast<const pci::PCI_HEADER0*>(pci_base_address);
        phys_addr_t mmio = make_phys(((static_cast<u64>(pci->bar1) << 32) | (pci->bar0 & 0xFFFFFFF0)));
        virt_addr_t addr = kernel::memory::request_pages(4);
        c_regs_ = virt_as<NVME_CONTROLLER_REGISTERS>(addr);
        kernel::memory::map_range(addr, mmio, PAGE_SIZE * 4, (1ULL << WriteThrough) | (1ULL << CacheDisabled));

        Log::info("[NVMe] Initializing Controller...");

        disable();

        unsigned spin = 50;
        while (spin-- && (c_regs_->csts.rdy)) {
            kernel::time::sleep_ms(10);
        }

        if (spin <= 0) {
            Log::error("[NVMe] Controller not deactivated, abort...");
            d_status = CONTROLLER_ERROR;
            return;
        }

        c_regs_->cc.iosqes = 6;  // 2^6 = 64 bytes
        c_regs_->cc.iocqes = 4;  // 2^4 = 16 bytes

        phys_addr_t adm_cq_phys_page = kernel::memory::request_page_phys();
        phys_addr_t adm_sq_phys_page = kernel::memory::request_page_phys();
        if (phys_null(adm_cq_phys_page) || phys_null(adm_sq_phys_page)) {
            Log::error("[NVMe] No physical pages for admin queues");
            return;
        }

        virt_addr_t adm_cq_virt_page = phys_to_virt(adm_cq_phys_page);
        virt_addr_t adm_sq_virt_page = phys_to_virt(adm_sq_phys_page);

        kernel::memory::map_memory(
            adm_cq_virt_page, adm_cq_phys_page, (1ULL << WriteThrough) | (1ULL << CacheDisabled)
        );
        kernel::memory::map_memory(
            adm_sq_virt_page, adm_sq_phys_page, (1ULL << WriteThrough) | (1ULL << CacheDisabled)
        );

        memset(virt_ptr(adm_cq_virt_page), 0, PAGE_SIZE);
        memset(virt_ptr(adm_sq_virt_page), 0, PAGE_SIZE);

        c_regs_->acq.acqb = phys_raw(adm_cq_phys_page) >> 12;
        c_regs_->asq.asqb = phys_raw(adm_sq_phys_page) >> 12;

        new (&admin_queue_) NvmeQueue(
            0,
            adm_cq_phys_page,
            adm_sq_phys_page,
            adm_cq_virt_page,
            adm_sq_virt_page,
            get_completion_doorbell(0),
            get_submission_doorbell(0),
            PAGE_SIZE,
            PAGE_SIZE
        );

        c_regs_->aqa.acqs = 0;
        c_regs_->aqa.asqs = 0;
        set_admin_completion_queue_size(admin_queue_.cq_size());
        set_admin_submission_queue_size(admin_queue_.sq_size());

        enable();

        spin = 50;
        while (spin-- && !c_regs_->csts.rdy) {
            kernel::time::sleep_ms(10);
        }
        if (spin <= 0) {
            Log::error("[NVMe] Controller not ready");
            d_status = DRIVER_STATUS::CONTROLLER_ERROR;
            return;
        }

        if (c_regs_->csts.cfs) {
            Log::error("[NVMe] Controller error (Fatal)");
            d_status = DRIVER_STATUS::CONTROLLER_ERROR;
            return;
        }

        if (identify_controller()) {
            Log::error("[NVMe] Controller identifiy failed");
            d_status = DRIVER_STATUS::CONTROLLER_ERROR;
            return;
        }

        Vector<u32> namespace_ids;

        if (get_namespace_list(&namespace_ids)) {
            Log::error("[NVMe] Failed to get namespace list");
            d_status = DRIVER_STATUS::CONTROLLER_ERROR;
        }

        if (create_io_queue(&io_queue_)) {
            Log::error("Failed to create IO Queue");
        }

        char name[16];
        DeviceManager::alloc_unique_device_name("nvme", name, sizeof(name));
        kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
                .set_name(name)
                .set_type(DeviceType::Controller)
                .set_class(DeviceClass::Storage)
                .set_bus(BusType::Pci)
                .set_controller(ControllerType::Nvme)
                .with_lifecycle(this)
                .with_smart(this)
                .with_info(this)
        );
        DevFs::register_device(kd_);

        for (u32 namespace_id : namespace_ids) {
            Log::log_msg("[NVMe] Namespace ID %u", namespace_id);

            phys_addr_t phys_buffer = kernel::memory::request_page_phys();
            virt_addr_t virt_buffer = phys_to_virt(phys_buffer);
            NVME_COMPLETION_ENTRY completion{};
            NVME_COMMAND identify_ns_cmd = {};
            identify_ns_cmd.cdw0.opc = NVME_ADMIN_COMMAND_IDENTIFY;
            identify_ns_cmd.prp1 = phys_raw(phys_buffer);
            identify_ns_cmd.nsid = namespace_id;
            identify_ns_cmd.u.identify.cdw10.cns = NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE;

            admin_queue_.submit_wait(identify_ns_cmd, completion);

            auto ns_identify = virt_as<NVME_IDENTIFY_NAMESPACE_DATA>(virt_buffer);

            bool supports_dsm = controller_identity_->oncs.dataset_management;
            auto* ns = new NvmeNamespace(namespace_id, &io_queue_, ns_identify, supports_dsm);

            char name_namespace[32];
            DeviceManager::generate_nvme_device_name(kd_, name_namespace, sizeof(name_namespace), namespace_id);
            ns->kd = DeviceManager::register_device(
                DeviceDescriptor{}
                    .set_name(name_namespace)
                    .set_type(DeviceType::Block)
                    .set_class(DeviceClass::Storage)
                    .set_bus(BusType::Pci)
                    .set_controller(ControllerType::Nvme)
                    .with_block(ns)
                    .with_parent(kd_)
            );
            DevFs::register_device(ns->kd);

            namespaces_.push_back(ns);
        }

        d_status = CONTROLLER_READY;
        Log::ok("[NVMe] Controller initialized");
    }

    NvmeDriver::~NvmeDriver() {
        shutdown();
    }

    long NvmeDriver::identify_controller() {
        virt_addr_t controller_identity_virt = kernel::memory::request_page();
        if (virt_null(controller_identity_virt)) {
            Log::error("[NVMe] No virtual memory for controller identify");
            return -1;
        }

        controller_identity_phys_ = virt_to_phys(controller_identity_virt);
        controller_identity_ = virt_as<NVME_IDENTIFY_CONTROLLER_DATA>(controller_identity_virt);

        NVME_COMMAND identify_command{};
        memset(&identify_command, 0, sizeof(NVME_COMMAND));

        identify_command.cdw0.opc = NVME_ADMIN_COMMAND_IDENTIFY;
        identify_command.prp1 = phys_raw(controller_identity_phys_);
        identify_command.u.identify.cdw10.cns = NVME_IDENTIFY_CNS_CONTROLLER;

        NVME_COMPLETION_ENTRY completion{};
        admin_queue_.submit_wait(identify_command, completion);

        if (completion.dw3.status > 0) {
            Log::error("[NVMe] Controller identification status error: %u", completion.dw3.status);
            return completion.dw3.status;
        }

        char serial[21] = {};
        memcpy(serial, controller_identity_->sn, 20);

        for (int i = 19; i >= 0 && serial[i] == ' '; --i) {
            serial[i] = 0;
        }

        char model[41] = {};
        memcpy(model, controller_identity_->mn, 40);

        for (int i = 39; i >= 0 && model[i] == ' '; --i) model[i] = 0;

        char fw[9] = {};
        memcpy(fw, controller_identity_->fr, 8);
        for (int i = 7; i >= 0 && fw[i] == ' '; --i) fw[i] = 0;

        Log::info("[NVMe] Model: %s", model);
        Log::info("[NVMe] Firmware: %s", fw);
        Log::info("[NVMe] Serial: %s", serial);

        return 0;
    }

    long NvmeDriver::get_namespace_list(Vector<u32>* namespace_ids) {
        virt_addr_t namespace_list_virt = kernel::memory::request_page();
        phys_addr_t namespace_list_phys = virt_to_phys(namespace_list_virt);
        kernel::memory::map_memory(
            namespace_list_virt, namespace_list_phys, (1ULL << PtFlag::WriteThrough) | (1ULL << PtFlag::CacheDisabled)
        );

        auto* namespace_list = virt_as<u32>(namespace_list_virt);

        NVME_COMMAND identify_ns_list{};
        memset(&identify_ns_list, 0, sizeof(NVME_COMMAND));

        identify_ns_list.cdw0.opc = NVME_ADMIN_COMMAND_IDENTIFY;
        identify_ns_list.prp1 = phys_raw(namespace_list_phys);

        identify_ns_list.u.identify.cdw10.cns = NVME_IDENTIFY_CNS_ACTIVE_NAMESPACES;
        identify_ns_list.nsid = 0;

        NVME_COMPLETION_ENTRY completion{};
        admin_queue_.submit_wait(identify_ns_list, completion);

        if (completion.dw3.status > 0) {
            return completion.dw3.status;
        }

        u32* namespace_list_end = namespace_list + (PAGE_SIZE / sizeof(u32));
        while (*namespace_list && namespace_list < namespace_list_end) {
            namespace_ids->push_back(*namespace_list++);
        }

        kernel::memory::free_page(namespace_list_virt);

        return 0;
    }

    void NvmeDriver::shutdown() {
        if (d_status != CONTROLLER_READY || !c_regs_) {
            return;
        }

        Log::info("[NVMe] Starting controller shutdown...");

        if (c_regs_->cc.en) {
            if (io_queue_.get_queue_id() != 0) {
                delete_io_queue(&io_queue_);
            }

            c_regs_->cc.shn = 0x1;

            // Wait for shutdown to complete
            // Spec recommends waiting RTD3 Entry Latency, or 1 second minimum
            u32 timeout_ms = 1000;
            if (controller_identity_ && controller_identity_->rtd3_e > 0) {
                timeout_ms = controller_identity_->rtd3_e;
            }

            u32 elapsed = 0;

            // Poll CSTS.SHST for shutdown complete (10b)
            while (elapsed < timeout_ms) {
                constexpr u32 poll_interval_ms = 10;
                if (c_regs_->csts.shst == 0x2)  // 10b = shutdown complete
                {
                    Log::ok("[NVMe] Controller shutdown complete");
                    d_status = DRIVER_STATUS::CONTROLLER_SHUTDOWN;
                    return;
                }

                kernel::time::sleep_ms(poll_interval_ms);
                elapsed += poll_interval_ms;
            }

            Log::warning("[NVMe] Controller shutdown timeout (SHST = %u)", c_regs_->csts.shst);
        }
    }

    long NvmeDriver::delete_io_queue(NvmeQueue* queue_ptr) {
        if (!queue_ptr || queue_ptr->get_queue_id() == 0) {
            return 0;
        }

        u16 queue_id = queue_ptr->get_queue_id();

        NVME_COMPLETION_ENTRY completion{};

        // Delete I/O Submission Queue
        NVME_COMMAND delete_sq{};
        delete_sq.cdw0.opc = NVME_ADMIN_COMMAND_DELETE_IO_SQ;
        delete_sq.u.deleteioq.cdw10.qid = queue_id;

        admin_queue_.submit_wait(delete_sq, completion);
        if (completion.dw3.status > 0) {
            Log::warning("[NVMe] Failed to delete I/O submission queue %u, status %u", queue_id, completion.dw3.status);
            return completion.dw3.status;
        }

        // Delete I/O Completion Queue
        NVME_COMMAND delete_cq{};
        delete_cq.cdw0.opc = NVME_ADMIN_COMMAND_DELETE_IO_CQ;
        delete_cq.u.deleteioq.cdw10.qid = queue_id;

        admin_queue_.submit_wait(delete_cq, completion);
        if (completion.dw3.status > 0) {
            Log::warning("[NVMe] Failed to delete I/O completion queue %u, status %u", queue_id, completion.dw3.status);
            return completion.dw3.status;
        }

        return 0;
    }

    long NvmeDriver::create_io_queue(NvmeQueue* queue_ptr) {
        phys_addr_t sq_phys = kernel::memory::request_page_phys();
        phys_addr_t cq_phys = kernel::memory::request_page_phys();
        if (phys_null(sq_phys) || phys_null(cq_phys)) {
            Log::error("[NVMe] Failed to allocate physical pages for IO queue");
            return -1;
        }

        virt_addr_t sq_virt = kernel::memory::request_page();
        virt_addr_t cq_virt = kernel::memory::request_page();
        if (virt_null(sq_virt) || virt_null(cq_virt)) {
            Log::error("[NVMe] Failed to allocate virtual pages for IO queue");
            return -1;
        }

        kernel::memory::map_memory(sq_virt, sq_phys, (1ULL << PtFlag::WriteThrough) | (1ULL << PtFlag::CacheDisabled));
        kernel::memory::map_memory(cq_virt, cq_phys, (1ULL << PtFlag::WriteThrough) | (1ULL << PtFlag::CacheDisabled));

        u16 queue_id = allocate_queue_id();

        NVME_COMPLETION_ENTRY completion{};

        queue_ptr->~NvmeQueue();

        new (queue_ptr) NvmeQueue(
            queue_id,
            cq_phys,
            sq_phys,
            cq_virt,
            sq_virt,
            get_completion_doorbell(queue_id),
            get_submission_doorbell(queue_id),
            PAGE_SIZE,
            PAGE_SIZE
        );

        NVME_COMMAND create_cq{};
        create_cq.cdw0.opc = NVME_ADMIN_COMMAND_CREATE_IO_CQ;
        create_cq.u.createiocq.cdw10.qid = queue_id;
        create_cq.u.createiocq.cdw10.qsize = queue_ptr->cq_size() - 1;
        create_cq.u.createiocq.cdw11.pc = 1;
        create_cq.prp1 = phys_raw(cq_phys);

        admin_queue_.submit_wait(create_cq, completion);
        if (completion.dw3.status > 0) {
            Log::warning("[NVMe] Status %u creating I/O completion queue", completion.dw3.status);
            return completion.dw3.status;
        }

        NVME_COMMAND create_sq{};
        create_sq.cdw0.opc = NVME_ADMIN_COMMAND_CREATE_IO_SQ;
        create_sq.u.createiosq.cdw10.qid = queue_id;
        create_sq.u.createiosq.cdw10.qsize = queue_ptr->sq_size() - 1;
        create_sq.u.createiosq.cdw11.pc = 1;
        create_sq.u.createiosq.cdw11.cqid = queue_id;
        create_sq.prp1 = phys_raw(sq_phys);

        admin_queue_.submit_wait(create_sq, completion);
        if (completion.dw3.status > 0) {
            Log::warning("[NVMe] Status %u creating I/O submission queue", completion.dw3.status);
            return completion.dw3.status;
        }

        return 0;
    }

    NvmeQueue::NvmeQueue(
        const u16 qid, const phys_addr_t cq_base, const phys_addr_t sq_base, const virt_addr_t cq, const virt_addr_t sq, volatile u32* cq_db,
        volatile u32* sq_db, const u16 csz, const u16 ssz
    )
        : queue_id_(qid)
        , completion_base_(cq_base)
        , submission_base_(sq_base)
        , completion_queue_(virt_as<NVME_COMPLETION_ENTRY>(cq))
        , submission_queue_(virt_as<NVME_COMMAND>(sq))
        , completion_db_(cq_db)
        , submission_db_(sq_db)
        , cq_count_(csz / sizeof(NVME_COMPLETION_ENTRY))
        , sq_count_(ssz / sizeof(NVME_COMMAND)) {
        memset(completion_queue_, 0, csz);
        memset(submission_queue_, 0, ssz);
        queue_mutex_.init();
    }

    long NvmeQueue::consume(NVME_COMMAND&) {
        return 0;
    }

    void NvmeQueue::submit(NVME_COMMAND& cmd) {
        cmd.cdw0.cid = next_command_id_++;
        if (next_command_id_ == 0xffff) {
            next_command_id_ = 0;
        }

        submission_queue_[sq_tail] = cmd;

        sq_tail++;
        if (sq_tail >= sq_count_) {
            sq_tail = 0;
        }

        *submission_db_ = sq_tail;
    }

    void NvmeQueue::submit_wait(NVME_COMMAND& cmd, NVME_COMPLETION_ENTRY& complet) {
        kernel::MutexGuard guard(queue_mutex_);

        cmd.cdw0.cid = next_command_id_++;
        if (next_command_id_ == 0xffff) {
            next_command_id_ = 0;
        }

        submission_queue_[sq_tail] = cmd;

        sq_tail++;
        if (sq_tail >= sq_count_) {
            sq_tail = 0;
        }

        *submission_db_ = sq_tail;

        auto start = 0;
        while (completion_cycle_state == !completion_queue_[cq_head].dw3.p) {
            if (start > 500000) {
                //   NVME_COMMAND_STATUS timeout_status{};
                //  timeout_status.P = 1;
                //  timeout_status.SCT = 1;
                //  timeout_status.SC = 0x01;
                //  timeout_status.DNR = 1;
                //  timeout_status.M = 0;
                complet.dw3.status = 10000;
                return;
            }
            start++;
            kernel::time::sleep_ms(10);
            //kernel::scheduling::yield();
        }
        complet = completion_queue_[cq_head];

        if (++cq_head >= cq_count_) {
            cq_head = 0;
            completion_cycle_state = !completion_cycle_state;
        }

        *completion_db_ = cq_head;
    }

    static u64 setup_prp2(const phys_addr_t dma_phys, const usize pages) {
        if (pages <= 1) {
            return 0;
        }

        if (pages == 2) {
            return phys_raw(phys_add(dma_phys, PAGE_SIZE));
        }

        const phys_addr_t prp_list_phys = kernel::memory::request_page_phys();
        if (phys_null(prp_list_phys)) {
            return 0;  // Allocation failed
        }

        const virt_addr_t prp_list_virt = phys_to_virt(prp_list_phys);
        auto* prp_list = virt_as<u64>(prp_list_virt);

        for (usize i = 1; i < pages; i++) {
            prp_list[i - 1] = phys_raw(phys_add(dma_phys, i * PAGE_SIZE));
        }

        return phys_raw(prp_list_phys);
    }

    bool NvmeDriver::smart_read_data(u8* out_buf) {
        phys_addr_t phys = kernel::memory::request_page_phys();
        if (phys_null(phys)) return false;

        memset(virt_ptr(phys_to_virt(phys)), 0, 512);

        NVME_COMMAND cmd{};
        cmd.cdw0.opc = NVME_ADMIN_COMMAND_GET_LOG_PAGE;
        cmd.nsid = 0xFFFFFFFF;  // controller-wide
        cmd.prp1 = phys_raw(phys);

        // CDW10: LID=0x02, NUMDL=511/4=127 dwords (512 bytes)
        cmd.u.getlogpage.cdw10_v13.lid = 0x02;   // Health Information
        cmd.u.getlogpage.cdw10_v13.numdl = 127;  // (512/4) - 1

        NVME_COMPLETION_ENTRY completion{};
        admin_queue_.submit_wait(cmd, completion);

        if (completion.dw3.status > 0) {
            kernel::memory::free_page_phys(phys);
            return false;
        }

        memcpy(out_buf, virt_ptr(phys_to_virt(phys)), 512);
        kernel::memory::free_page_phys(phys);
        return true;
    }

    constexpr u16 KELVIN_TO_CELSIUS_OFFSET = 273;
    static u8 kelvin_to_celsius(const u8 lo, const u8 hi) {
        const u16 kelvin = static_cast<u16>(lo) | static_cast<u16>(hi) << 8;
        return kelvin > KELVIN_TO_CELSIUS_OFFSET ? static_cast<u8>(kelvin - KELVIN_TO_CELSIUS_OFFSET) : 0;
    }

    bool NvmeDriver::smart_get_common(smart_common* out) {
        u8 raw[512]{};
        if (!smart_read_data(raw)) return false;
        const auto* h = reinterpret_cast<const NVME_HEALTH_INFO_LOG*>(raw);

        out->driver_type = SMART_DRIVER_NVME;
        out->temperature_celsius = kelvin_to_celsius(h->temperature[0], h->temperature[1]);
        out->power_on_hours = static_cast<u64>(h->power_on_hours);
        out->health_ok = h->critical_warning.raw == 0;
        out->critical_warning_raw = h->critical_warning.raw;
        return true;
    }

    bool NvmeDriver::smart_get_nvme(smart_nvme* out) {
        u8 raw[512]{};
        if (!smart_read_data(raw)) return false;
        const auto* h = reinterpret_cast<const NVME_HEALTH_INFO_LOG*>(raw);

        out->critical_warning_raw = h->critical_warning.raw;
        out->temperature_celsius = kelvin_to_celsius(h->temperature[0], h->temperature[1]);
        out->available_spare = h->available_spare;
        out->available_spare_threshold = h->available_spare_threshold;
        out->percentage_used = h->percentage_used;

        out->temperature_sensor[0] = kelvin_to_celsius(h->temperature_sensor1[0], h->temperature_sensor1[1]);
        out->temperature_sensor[1] = kelvin_to_celsius(h->temperature_sensor2[0], h->temperature_sensor2[1]);
        out->temperature_sensor[2] = kelvin_to_celsius(h->temperature_sensor3[0], h->temperature_sensor3[1]);
        out->temperature_sensor[3] = kelvin_to_celsius(h->temperature_sensor4[0], h->temperature_sensor4[1]);
        out->temperature_sensor[4] = kelvin_to_celsius(h->temperature_sensor5[0], h->temperature_sensor5[1]);
        out->temperature_sensor[5] = kelvin_to_celsius(h->temperature_sensor6[0], h->temperature_sensor6[1]);
        out->temperature_sensor[6] = kelvin_to_celsius(h->temperature_sensor7[0], h->temperature_sensor7[1]);
        out->temperature_sensor[7] = kelvin_to_celsius(h->temperature_sensor8[0], h->temperature_sensor8[1]);

        out->data_units_read = static_cast<u64>(h->data_unit_read);
        out->data_units_written = static_cast<u64>(h->data_unit_written);
        out->host_read_commands = static_cast<u64>(h->host_read_commands);
        out->host_write_commands = static_cast<u64>(h->host_written_commands);
        out->controller_busy_time_min = static_cast<u64>(h->controller_busy_time);
        out->power_cycles = static_cast<u64>(h->power_cycle);
        out->power_on_hours = static_cast<u64>(h->power_on_hours);
        out->unsafe_shutdowns = static_cast<u64>(h->unsafe_shutdowns);
        out->media_errors = static_cast<u64>(h->media_errors);
        out->error_log_entries = static_cast<u64>(h->error_info_log_entry_count);
        out->warning_temp_time_min = h->warning_composite_temperature_time;
        out->critical_temp_time_min = h->critical_composite_temperature_time;

        return true;
    }

    void NvmeDriver::copy_nvme_string(char* dst, const usize dst_len, const u8* src, const usize src_len) {
        const usize n = src_len < dst_len - 1 ? src_len : dst_len - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
        for (isize i = static_cast<isize>(n) - 1; i >= 0 && dst[i] == ' '; i--) dst[i] = '\0';
    }

    bool NvmeDriver::get_vendor(char* out, const usize len) {
        strncpy(out, pci::get_vendor_name(controller_identity_->vid), len);
        out[len - 1] = '\0';
        return true;
    }

    bool NvmeDriver::get_model(char* out, const usize len) {
        copy_nvme_string(out, len, controller_identity_->mn, sizeof(controller_identity_->mn));
        return true;
    }

    bool NvmeDriver::get_serial(char* out, const usize len) {
        copy_nvme_string(out, len, controller_identity_->sn, sizeof(controller_identity_->sn));
        return true;
    }

    bool NvmeDriver::get_firmware(char* out, const usize len) {
        copy_nvme_string(out, len, controller_identity_->fr, sizeof(controller_identity_->fr));
        return true;
    }

    isize NvmeNamespace::read(u64 lba, usize sector_count, void* buffer, usize buffer_size) {
        usize bytes = sector_count * sector_size_;
        if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

        kernel::MutexGuard guard(namespace_mutex_);

        usize pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;

        virt_addr_t dma_virt = phys_to_virt(dma_phys);

        NVME_COMMAND read_cmd = {};
        read_cmd.cdw0.opc = NVME_NVM_COMMAND_READ;
        read_cmd.nsid = ns_id_;
        read_cmd.prp1 = phys_raw(dma_phys);
        read_cmd.prp2 = setup_prp2(dma_phys, pages);
        read_cmd.u.readwrite.lbalow = static_cast<u32>(lba & 0xFFFFFFFFULL);
        read_cmd.u.readwrite.lbahigh = static_cast<u32>(lba >> 32);
        read_cmd.u.readwrite.cdw12.nlb = sector_count - 1;

        if (read_cmd.prp2 == 0 && pages > 1) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            return -ENOMEM;
        }

        NVME_COMPLETION_ENTRY completion{};
        queue_->submit_wait(read_cmd, completion);

        if (pages > 2) {
            phys_addr_t prp_list = make_phys(read_cmd.prp2);
            kernel::memory::free_page_phys(prp_list);
        }

        if (completion.dw3.status > 0) {
            Log::warning("[NVMe] Read failed with status %u", completion.dw3.status);
            kernel::memory::free_pages_phys(dma_phys, pages);
            return -EIO;
        }

        memcpy(buffer, virt_ptr(dma_virt), bytes);

        kernel::memory::free_pages_phys(dma_phys, pages);

        return static_cast<isize>(bytes);
    }

    isize NvmeNamespace::write(u64 lba, usize sector_count, const void* buffer, usize buffer_size) {
        usize bytes = sector_count * sector_size_;
        if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

        kernel::MutexGuard guard(namespace_mutex_);

        usize pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;

        virt_addr_t dma_virt = phys_to_virt(dma_phys);

        memcpy(virt_ptr(dma_virt), buffer, bytes);

        NVME_COMMAND write_cmd = {};
        write_cmd.cdw0.opc = NVME_NVM_COMMAND_WRITE;  // NVM Write
        write_cmd.nsid = ns_id_;
        write_cmd.prp1 = phys_raw(dma_phys);
        write_cmd.prp2 = setup_prp2(dma_phys, pages);
        write_cmd.u.readwrite.lbalow = static_cast<u32>(lba & 0xFFFFFFFFULL);
        write_cmd.u.readwrite.lbahigh = static_cast<u32>(lba >> 32);
        write_cmd.u.readwrite.cdw12.nlb = sector_count - 1;

        if (write_cmd.prp2 == 0 && pages > 1) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            return -ENOMEM;
        }

        NVME_COMPLETION_ENTRY completion{};
        queue_->submit_wait(write_cmd, completion);

        if (pages > 2) {
            phys_addr_t prp_list = make_phys(write_cmd.prp2);
            kernel::memory::free_page_phys(prp_list);
        }

        kernel::memory::free_pages_phys(dma_phys, pages);

        if (completion.dw3.status > 0) {
            Log::warning("[NVMe] Write failed with status %u", completion.dw3.status);
            return -EIO;
        }

        return static_cast<isize>(bytes);
    }

    bool NvmeNamespace::trim(const TrimRange* ranges, usize count) {
        if (!has_trim_ || !ranges || count == 0) return false;

        kernel::MutexGuard guard(namespace_mutex_);

        usize offset = 0;
        while (offset < count) {
            constexpr usize max_ranges_per_cmd = 256;
            const usize batch = (count - offset < max_ranges_per_cmd) ? (count - offset) : max_ranges_per_cmd;

            const phys_addr_t dma_phys = kernel::memory::request_page_phys();
            if (phys_null(dma_phys)) return false;

            auto* payload = static_cast<NVME_LBA_RANGE*>(virt_ptr(phys_to_virt(dma_phys)));
            memset(payload, 0, PAGE_SIZE);

            for (usize i = 0; i < batch; i++) {
                payload[i].starting_lba = ranges[offset + i].lba;
                payload[i].logical_block_count = ranges[offset + i].sector_count;
                // no need fro attributes, just simple deallocate
            }

            NVME_COMMAND cmd{};
            cmd.cdw0.opc = NVME_NVM_COMMAND_DATASET_MANAGEMENT;
            cmd.nsid = ns_id_;
            cmd.prp1 = phys_raw(dma_phys);
            cmd.u.datasetmanagement.cdw10.nr = static_cast<u8>(batch - 1);  // 0-based
            cmd.u.datasetmanagement.cdw11.ad = 1;

            NVME_COMPLETION_ENTRY completion{};
            queue_->submit_wait(cmd, completion);

            kernel::memory::free_page_phys(dma_phys);

            if (completion.dw3.status > 0) {
                Log::warning("[NVMe] TRIM failed, status %u", completion.dw3.status);
                return false;
            }

            offset += batch;
        }

        return true;
    }
}  // namespace nvme