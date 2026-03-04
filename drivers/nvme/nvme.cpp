//
// Created by linus on 07.07.25.
//

#include "nvme.h"

#include <kernel/devices/device_manager.h>
#include <kernel/memory.h>
#include <kernel/time.h>

#include "../../filesystem/devfs/devfs.h"
#include "../../include/log.h"
#include "../../kernel/cpu/cpu_manager.h"
#include "vespera_errno.h"

namespace NVMe {
    NvmeDriver::NvmeDriver(PCI::PCIDeviceHeader* pci_base_address) {
        const auto* pci = reinterpret_cast<const PCI::PCIHeader0*>(pci_base_address);
        phys_addr_t mmio = make_phys(((static_cast<uint64_t>(pci->BAR1) << 32) | (pci->BAR0 & 0xFFFFFFF0)));
        virt_addr_t addr = kernel::memory::request_pages(4);
        c_regs = virt_as<NVME_CONTROLLER_REGISTERS>(addr);
        kernel::memory::map_range(addr, mmio, PAGE_SIZE * 4, (1ULL << WriteThrough) | (1ULL << CacheDisabled));

        Log::Info("[NVMe] Initializing Controller...");

        disable();

        unsigned spin = 50;
        while (spin-- && (c_regs->CSTS.RDY)) {
            kernel::time::sleep_ms(10);
        }

        if (spin <= 0) {
            Log::Error("[NVMe] Controller not deactivated, abort...");
            d_status = controller_error;
            return;
        }

        c_regs->CC.IOSQES = 6;  // 2^6 = 64 bytes
        c_regs->CC.IOCQES = 4;  // 2^4 = 16 bytes

        phys_addr_t adm_cq_phys_page = kernel::memory::request_page_phys();
        phys_addr_t adm_sq_phys_page = kernel::memory::request_page_phys();
        if (phys_null(adm_cq_phys_page) || phys_null(adm_sq_phys_page)) {
            Log::Error("[NVMe] No physical pages for admin queues");
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

        c_regs->ACQ.ACQB = phys_raw(adm_cq_phys_page) >> 12;
        c_regs->ASQ.ASQB = phys_raw(adm_sq_phys_page) >> 12;

        new (&admin_queue) NVMe::NvmeQueue(
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

        c_regs->AQA.ACQS = 0;
        c_regs->AQA.ASQS = 0;
        set_admin_completion_queue_size(admin_queue.cq_size());
        set_admin_submission_queue_size(admin_queue.sq_size());

        enable();

        spin = 50;
        while (spin-- && !c_regs->CSTS.RDY) {
            kernel::time::sleep_ms(10);
        }
        if (spin <= 0) {
            Log::Error("[NVMe] Controller not ready");
            d_status = driver_status::controller_error;
            return;
        }

        if (c_regs->CSTS.CFS) {
            Log::Error("[NVMe] Controller error (Fatal)");
            d_status = driver_status::controller_error;
            return;
        }

        if (identify_controller()) {
            Log::Error("[NVMe] Controller identifiy failed");
            d_status = driver_status::controller_error;
            return;
        }

        Vector<uint32_t> namespace_ids;

        if (get_namespace_list(&namespace_ids)) {
            Log::Error("[NVMe] Failed to get namespace list");
            d_status = driver_status::controller_error;
        }

        if (create_io_queue(&io_queue)) {
            Log::Error("Failed to create IO Queue");
        }

        char name[16];
        DeviceManager::AllocUniqueDeviceName("nvme", name, sizeof(name));
        kd = DeviceManager::RegisterController(
            name, DeviceClass::Storage, BusType::BUS_PCI, ControllerType::NVMe, nullptr
        );
        DevFS::register_device(kd);

        for (uint32_t namespace_id : namespace_ids) {
            Log::LogMsg("[NVMe] Namespace ID %u", namespace_id);

            phys_addr_t phys_buffer = kernel::memory::request_page_phys();
            virt_addr_t virt_buffer = phys_to_virt(phys_buffer);
            NVME_COMPLETION_ENTRY completion{};
            NVME_COMMAND identify_ns_cmd = {};
            identify_ns_cmd.CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
            identify_ns_cmd.PRP1 = phys_raw(phys_buffer);
            identify_ns_cmd.NSID = namespace_id;
            identify_ns_cmd.u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE;

            admin_queue.submit_wait(identify_ns_cmd, completion);

            auto ns_identify = virt_as<NVME_IDENTIFY_NAMESPACE_DATA>(virt_buffer);

            auto* ns = new NvmeNamespace(namespace_id, &io_queue, ns_identify);

            char name_namespace[32];
            DeviceManager::GenerateNVMeDeviceName(kd, name_namespace, sizeof(name_namespace), namespace_id);
            ns->kd = DeviceManager::RegisterBlockDevice(
                ns, name_namespace, DeviceClass::Storage, BusType::BUS_PCI, ControllerType::NVMe, kd
            );
            DevFS::register_device(ns->kd);
            DeviceManager::FindAndRegisterPartitions(ns->kd);

            namespaces.push_back(ns);
        }

        Log::Ok("[NVMe] Controller initialized");
    }

    long NvmeDriver::identify_controller() {
        controller_identity_phys = kernel::memory::request_page_phys();
        if (phys_null(controller_identity_phys)) {
            Log::Error("[NVMe] No physical memory for controller identifiy");
            return -1;
        }

        virt_addr_t controller_identity_virt = kernel::memory::request_page();
        if (virt_null(controller_identity_virt)) {
            Log::Error("[NVMe] No virtual memory for controller identify");
            return -1;
        }

        controller_identity = virt_as<NVME_IDENTIFY_CONTROLLER_DATA>(controller_identity_virt);

        kernel::memory::map_memory(controller_identity_virt, controller_identity_phys);

        NVME_COMMAND identify_command{};
        memset(&identify_command, 0, sizeof(NVME_COMMAND));

        identify_command.CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
        identify_command.PRP1 = phys_raw(controller_identity_phys);
        identify_command.u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_CONTROLLER;

        NVME_COMPLETION_ENTRY completion{};
        admin_queue.submit_wait(identify_command, completion);

        if (completion.DW3.Status > 0) {
            Log::Error("[NVMe] Controller identification status error: %u", completion.DW3.Status);
            return completion.DW3.Status;
        }

        char serial[21] = {};
        memcpy(serial, controller_identity->SN, 20);

        for (int i = 19; i >= 0 && serial[i] == ' '; --i) {
            serial[i] = 0;
        }

        char model[41] = {};
        memcpy(model, controller_identity->MN, 40);

        for (int i = 39; i >= 0 && model[i] == ' '; --i) model[i] = 0;

        char fw[9] = {};
        memcpy(fw, controller_identity->FR, 8);
        for (int i = 7; i >= 0 && fw[i] == ' '; --i) fw[i] = 0;

        Log::Info("[NVMe] Model: %s", model);
        Log::Info("[NVMe] Firmware: %s", fw);
        Log::Info("[NVMe] Serial: %s", serial);

        return 0;
    }

    long NvmeDriver::get_namespace_list(Vector<uint32_t>* namespace_ids) {
        virt_addr_t namespace_list_virt = kernel::memory::request_page();
        phys_addr_t namespace_list_phys = virt_to_phys(namespace_list_virt);
        kernel::memory::map_memory(
            namespace_list_virt, namespace_list_phys, (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled)
        );

        auto* namespace_list = virt_as<uint32_t>(namespace_list_virt);

        NVME_COMMAND identify_ns_list{};
        memset(&identify_ns_list, 0, sizeof(NVME_COMMAND));

        identify_ns_list.CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
        identify_ns_list.PRP1 = phys_raw(namespace_list_phys);

        identify_ns_list.u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_ACTIVE_NAMESPACES;
        identify_ns_list.NSID = 0;

        NVME_COMPLETION_ENTRY completion{};
        admin_queue.submit_wait(identify_ns_list, completion);

        if (completion.DW3.Status > 0) {
            return completion.DW3.Status;
        }

        uint32_t* namespace_list_end = namespace_list + (PAGE_SIZE / sizeof(uint32_t));
        while (*namespace_list && namespace_list < namespace_list_end) {
            namespace_ids->push_back(*namespace_list++);
        }

        kernel::memory::free_page(namespace_list_virt);

        return 0;
    }

    long NvmeDriver::create_io_queue(NvmeQueue* queue_ptr) {
        phys_addr_t sq_phys = kernel::memory::request_page_phys();
        phys_addr_t cq_phys = kernel::memory::request_page_phys();
        if (phys_null(sq_phys) || phys_null(cq_phys)) {
            Log::Error("[NVMe] Failed to allocate physical pages for IO queue");
            return -1;
        }

        virt_addr_t sq_virt = kernel::memory::request_page();
        virt_addr_t cq_virt = kernel::memory::request_page();
        if (virt_null(sq_virt) || virt_null(cq_virt)) {
            Log::Error("[NVMe] Failed to allocate virtual pages for IO queue");
            return -1;
        }

        kernel::memory::map_memory(
            sq_virt, sq_phys, (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled)
        );
        kernel::memory::map_memory(
            cq_virt, cq_phys, (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled)
        );

        uint16_t queue_id = allocate_queue_id();

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
        create_cq.CDW0.OPC = NVME_ADMIN_COMMAND_CREATE_IO_CQ;
        create_cq.u.CREATEIOCQ.CDW10.QID = queue_id;
        create_cq.u.CREATEIOCQ.CDW10.QSIZE = queue_ptr->cq_size() - 1;
        create_cq.u.CREATEIOCQ.CDW11.PC = 1;
        create_cq.PRP1 = phys_raw(cq_phys);

        admin_queue.submit_wait(create_cq, completion);
        if (completion.DW3.Status > 0) {
            Log::Warning("[NVMe] Status %u creating I/O completion queue", completion.DW3.Status);
            return completion.DW3.Status;
        }

        NVME_COMMAND create_sq{};
        create_sq.CDW0.OPC = NVME_ADMIN_COMMAND_CREATE_IO_SQ;
        create_sq.u.CREATEIOSQ.CDW10.QID = queue_id;
        create_sq.u.CREATEIOSQ.CDW10.QSIZE = queue_ptr->sq_size() - 1;
        create_sq.u.CREATEIOSQ.CDW11.PC = 1;
        create_sq.u.CREATEIOSQ.CDW11.CQID = queue_id;
        create_sq.PRP1 = phys_raw(sq_phys);

        admin_queue.submit_wait(create_sq, completion);
        if (completion.DW3.Status > 0) {
            Log::Warning("[NVMe] Status %u creating I/O submission queue", completion.DW3.Status);
            return completion.DW3.Status;
        }

        return 0;
    }

    NvmeQueue::NvmeQueue(
        uint16_t qid, phys_addr_t cq_base, phys_addr_t sq_base, virt_addr_t cq, virt_addr_t sq,
        volatile uint32_t* cq_db, volatile uint32_t* sq_db, uint16_t csz, uint16_t ssz
    )
        : queue_id(qid)
        , completion_base(cq_base)
        , submission_base(sq_base)
        , completion_queue(virt_as<NVME_COMPLETION_ENTRY>(cq))
        , submission_queue(virt_as<NVME_COMMAND>(sq))
        , completion_db(cq_db)
        , submission_db(sq_db)
        , c_queue_size(csz)
        , s_queue_size(ssz)
        , cq_count(csz / sizeof(NVME_COMPLETION_ENTRY))
        , sq_count(ssz / sizeof(NVME_COMMAND)) {
        memset(completion_queue, 0, csz);
        memset(submission_queue, 0, ssz);
        queue_mutex.init();
    }

    long NvmeQueue::consume(NVME_COMMAND& cmd) {
        return 0;
    }

    void NvmeQueue::submit(NVME_COMMAND& cmd) {
        cmd.CDW0.CID = next_command_id++;
        if (next_command_id == 0xffff) {
            next_command_id = 0;
        }

        submission_queue[sq_tail] = cmd;

        sq_tail++;
        if (sq_tail >= sq_count) {
            sq_tail = 0;
        }

        *submission_db = sq_tail;
    }

    void NvmeQueue::submit_wait(NVME_COMMAND& cmd, NVME_COMPLETION_ENTRY& complet) {
        kernel::mutex_guard guard(queue_mutex);

        cmd.CDW0.CID = next_command_id++;
        if (next_command_id == 0xffff) {
            next_command_id = 0;
        }

        submission_queue[sq_tail] = cmd;

        sq_tail++;
        if (sq_tail >= sq_count) {
            sq_tail = 0;
        }

        *submission_db = sq_tail;

        auto start = 0;
        constexpr uint16_t NVME_CS_P_MASK = 0x0001;
        while (completion_cycle_state == !completion_queue[cq_head].DW3.P) {
            if (start > 50) {
                //   NVME_COMMAND_STATUS timeout_status{};
                //  timeout_status.P = 1;
                //  timeout_status.SCT = 1;
                //  timeout_status.SC = 0x01;
                //  timeout_status.DNR = 1;
                //  timeout_status.M = 0;
                complet.DW3.Status = 10000;
                return;
            }
            start++;
            kernel::time::sleep_ms(10);
        }
        complet = completion_queue[cq_head];

        if (++cq_head >= cq_count) {
            cq_head = 0;
            completion_cycle_state = !completion_cycle_state;
        }

        *completion_db = cq_head;
    }

    static uint64_t setup_prp2(phys_addr_t dma_phys, size_t pages) {
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

        virt_addr_t prp_list_virt = phys_to_virt(prp_list_phys);
        auto* prp_list = virt_as<uint64_t>(prp_list_virt);

        for (size_t i = 1; i < pages; i++) {
            prp_list[i - 1] = phys_raw(phys_add(dma_phys, i * PAGE_SIZE));
        }

        return phys_raw(prp_list_phys);
    }

    ssize_t NvmeNamespace::read(uint64_t lba, size_t sector_count, void* buffer, size_t buffer_size) {
        size_t bytes = sector_count * sector_size;
        if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

        kernel::mutex_guard guard(namespace_mutex);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;

        virt_addr_t dma_virt = phys_to_virt(dma_phys);

        NVME_COMMAND read_cmd = {};
        read_cmd.CDW0.OPC = NVME_NVM_COMMAND_READ;
        read_cmd.NSID = ns_id;
        read_cmd.PRP1 = phys_raw(dma_phys);
        read_cmd.PRP2 = setup_prp2(dma_phys, pages);
        read_cmd.u.READWRITE.LBALOW = static_cast<uint32_t>(lba & 0xFFFFFFFFULL);
        read_cmd.u.READWRITE.LBAHIGH = static_cast<uint32_t>(lba >> 32);
        read_cmd.u.READWRITE.CDW12.NLB = sector_count - 1;

        if (read_cmd.PRP2 == 0 && pages > 1) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            return -ENOMEM;
        }

        NVME_COMPLETION_ENTRY completion{};
        queue->submit_wait(read_cmd, completion);

        if (pages > 2) {
            phys_addr_t prp_list = make_phys(read_cmd.PRP2);
            kernel::memory::free_page_phys(prp_list);
        }

        if (completion.DW3.Status > 0) {
            Log::Warning("[NVMe] Read failed with status %u", completion.DW3.Status);
            kernel::memory::free_pages_phys(dma_phys, pages);
            return -EIO;
        }

        memcpy(buffer, virt_ptr(dma_virt), bytes);

        kernel::memory::free_pages_phys(dma_phys, pages);

        return static_cast<ssize_t>(bytes);
    }

    ssize_t NvmeNamespace::write(uint64_t lba, size_t sector_count, void* buffer, size_t buffer_size) {
        size_t bytes = sector_count * sector_size;
        if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

        kernel::mutex_guard guard(namespace_mutex);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;

        virt_addr_t dma_virt = phys_to_virt(dma_phys);

        memcpy(virt_ptr(dma_virt), buffer, bytes);

        NVME_COMMAND write_cmd = {};
        write_cmd.CDW0.OPC = NVME_NVM_COMMAND_WRITE;  // NVM Write
        write_cmd.NSID = ns_id;
        write_cmd.PRP1 = phys_raw(dma_phys);
        write_cmd.PRP2 = setup_prp2(dma_phys, pages);
        write_cmd.u.READWRITE.LBALOW = static_cast<uint32_t>(lba & 0xFFFFFFFFULL);
        write_cmd.u.READWRITE.LBAHIGH = static_cast<uint32_t>(lba >> 32);
        write_cmd.u.READWRITE.CDW12.NLB = sector_count - 1;

        if (write_cmd.PRP2 == 0 && pages > 1) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            return -ENOMEM;
        }

        NVME_COMPLETION_ENTRY completion{};
        queue->submit_wait(write_cmd, completion);

        if (pages > 2) {
            phys_addr_t prp_list = make_phys(write_cmd.PRP2);
            kernel::memory::free_page_phys(prp_list);
        }

        kernel::memory::free_pages_phys(dma_phys, pages);

        if (completion.DW3.Status > 0) {
            Log::Warning("[NVMe] Write failed with status %u", completion.DW3.Status);
            return -EIO;
        }

        return static_cast<ssize_t>(bytes);
    }
}  // namespace NVMe