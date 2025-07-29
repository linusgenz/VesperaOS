//
// Created by linus on 07.07.25.
//

#include "nvme.h"
#include "../../include/log.h"
#include "../../kernel/include/page_frame_allocator.h"
#include "../../kernel/time/time.h"

namespace NVMe {
    NvmeDriver::NvmeDriver(PCI::PCIDeviceHeader *pciBaseAddress) {
        const PCI::PCIHeader0 *pci = reinterpret_cast<const PCI::PCIHeader0 *>(pciBaseAddress);
        auto mmio = (((uint64_t) pci->BAR1 << 32) | (pci->BAR0 & 0xFFFFFFF0));
        c_regs = reinterpret_cast<Registers *>(global_allocator.request_pages(4));
        for (int i = 0; i < 4; i++) {
            auto virt = reinterpret_cast<void *>((uintptr_t) c_regs + i * 0x1000);
            auto phys = reinterpret_cast<void *>(mmio + i * 0x1000);
            global_page_table_manager.map_memory(virt, phys, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);
        }

        Log::Info("[NVMe] Initializing Controller...");

        Disable();

        unsigned spin = 500;
        while (spin-- && (c_regs->status & NVME_CSTS_READY)) {
            kernel::time::sleep_ms(10);
        }

        if (spin <= 0) {
            Log::Error("[NVMe] Controller not deactivated, abort...");
            d_status = DriverStatus::ControllerError;
            return;
        }

        c_regs->config |= NVME_CFG_DEFAULT_IOCQES | NVME_CFG_DEFAULT_IOSQES;

        void *admCQPhysPage = global_allocator.request_page();
        void *admSQPhysPage = global_allocator.request_page();
        if (!admCQPhysPage || !admSQPhysPage) {
            Log::Error("[NVMe] No physical pages for admin queues");
            return;
        }

        void *admCQVirtPage = global_allocator.request_page();
        void *admSQVirtPage = global_allocator.request_page();
        if (!admCQVirtPage || !admSQVirtPage) {
            Log::Error("[NVMe] No virtual pages for admin queues");
            return;
        }

        global_page_table_manager.map_memory(admCQVirtPage, admCQPhysPage, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);
        global_page_table_manager.map_memory(admSQVirtPage, admSQPhysPage, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);

        memset(admCQVirtPage, 0, PAGE_SIZE_4K);
        memset(admSQVirtPage, 0, PAGE_SIZE_4K);

        c_regs->admin_completion_q = (uintptr_t) admCQPhysPage;
        c_regs->admin_submission_q = (uintptr_t) admSQPhysPage;

        admin_queue = NVMe::NvmeQueue(0, (uintptr_t) admCQPhysPage, (uintptr_t) admSQPhysPage,
                                      admCQVirtPage, admSQVirtPage, GetCompletionDoorbell(0), GetSubmissionDoorbell(0),
                                      PAGE_SIZE_4K, PAGE_SIZE_4K);

        c_regs->admin_q_attr = 0;
        SetAdminCompletionQueueSize(admin_queue.CQSize());
        SetAdminSubmissionQueueSize(admin_queue.SQSize());

        Enable();

        // Warten auf Ready-Status
        spin = 500;
        while (spin-- && !(c_regs->status & NVME_CSTS_READY)) {
            kernel::time::sleep_ms(10);
        }
        if (spin <= 0) {
            Log::Error("[NVMe] Controller not ready");
            d_status = DriverStatus::ControllerError;
            return;
        }

        if (c_regs->status & NVME_CSTS_FATAL) {
            Log::Error("[NVMe] Controller error (Fatal)");
            d_status = DriverStatus::ControllerError;
            return;
        }

        if (IdentifyController()) {
            Log::Error("[NVMe] Controller identifiy failed");
            d_status = DriverStatus::ControllerError;
            return;
        }

        Vector<uint32_t> namespaceIDs;

        if (GetNamespaceList(&namespaceIDs)) {
            Log::Error("[NVMe] Failed to get namespace list");
            d_status = DriverStatus::ControllerError;
        }

        if (CreateIOQueue(&io_queue)) {
            Log::Error("Failed to create IO Queue");
        }

        for (int i = 0; i < namespaceIDs.size(); i++) {
            Log::LogMsg("[NVMe] Namespace ID %u", namespaceIDs[i]);

            void* physBuffer = global_allocator.request_page();
            NvmeCompletion completion{};
            NvmeCommand identifyNsCmd = {};
            identifyNsCmd.opcode = AdminCmdIdentify;
            identifyNsCmd.prp1 = (uintptr_t) physBuffer;
            identifyNsCmd.ns_id = namespaceIDs[i];
            identifyNsCmd.identify.cns = NvmeIdentifyCommand::CnsNamespace;

            admin_queue.SubmitWait(identifyNsCmd, completion);

            auto nsIdentify = reinterpret_cast<NamespaceIdentity *>(physBuffer);

            uint8_t lbaFormatIndex = nsIdentify->fmt_lba_size & 0x0F;
            uint8_t lbads = nsIdentify->lba_formats[lbaFormatIndex].lba_data_size;
            uint32_t lbaSize = 1 << lbads;

            Log::debug("namespaceSize: %u", nsIdentify->namespace_size);

            auto* ns = new NvmeNamespace(namespaceIDs[i], &io_queue, lbaSize);
            namespaces.push_back(ns);
        }

        Log::Ok("[NVMe] Controller initialized");
    }

    long NvmeDriver::IdentifyController() {
        controller_identity_phys = reinterpret_cast<uintptr_t>(global_allocator.request_page());
        if (!controller_identity_phys) {
            Log::Error("[NVMe] No physical memory for controller identifiy");
            return -1;
        }

        controller_identity = (ControllerIdentity *) global_allocator.request_page();
        if (!controller_identity) {
            Log::Error("[NVMe] No virtual memory for controller identify");
            return -1;
        }

        global_page_table_manager.map_memory(controller_identity, reinterpret_cast<void *>(controller_identity_phys),
                                             true);

        NvmeCommand identifyCommand{};
        memset(&identifyCommand, 0, sizeof(NvmeCommand));

        identifyCommand.opcode = AdminCmdIdentify;
        identifyCommand.prp1 = (uintptr_t) controller_identity_phys;
        identifyCommand.identify.cns = NvmeIdentifyCommand::CnsController;

        NvmeCompletion completion{};
        admin_queue.SubmitWait(identifyCommand, completion);

        if (completion.status > 0) {
            Log::Error("[NVMe] Controller identification status error: %u", completion.status);
            return completion.status;
        }

        char serial[21] = {0};
        memcpy(serial, controller_identity->serial_number, 20);

        for (int i = 19; i >= 0 && serial[i] == ' '; --i) {
            serial[i] = 0;
        }

        char model[41] = {0};
        memcpy(model, controller_identity->model_number, 40);

        for (int i = 39; i >= 0 && model[i] == ' '; --i) model[i] = 0;

        char fw[9] = {0};
        memcpy(fw, controller_identity->firmware_revision, 8);
        for (int i = 7; i >= 0 && fw[i] == ' '; --i) fw[i] = 0;

        Log::Info("[NVMe] Model: %s", model);
        Log::Info("[NVMe] Firmware: %s", fw);
        Log::Info("[NVMe] Serial: %s", serial);

        return 0;
    }

    long NvmeDriver::GetNamespaceList(Vector<uint32_t> *namespaceIDs) {
        uint32_t *namespaceList = reinterpret_cast<uint32_t *>(global_allocator.request_page());
        global_page_table_manager.map_memory(namespaceList, namespaceList, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);

        NvmeCommand identifyNsList;
        memset(&identifyNsList, 0, sizeof(NvmeCommand));

        identifyNsList.opcode = AdminCmdIdentify;
        identifyNsList.prp1 = reinterpret_cast<uint64_t>(namespaceList);

        identifyNsList.identify.cns = NvmeIdentifyCommand::CnsNamespaceList;
        identifyNsList.ns_id = 0;

        NvmeCompletion completion;
        admin_queue.SubmitWait(identifyNsList, completion);

        if (completion.status > 0) {
            return completion.status;
        }

        uint32_t *namespaceListEnd = namespaceList + (PAGE_SIZE_4K / sizeof(uint32_t));
        while (*namespaceList && namespaceList < namespaceListEnd) {
            namespaceIDs->push_back(*namespaceList++);
        }

        global_allocator.free_page(namespaceList);

        return 0;
    }

    long NvmeDriver::CreateIOQueue(NvmeQueue *queue_ptr) {
        void *sqPhys = global_allocator.request_page();
        void *cqPhys = global_allocator.request_page();
        if (!sqPhys || !cqPhys) {
            Log::Error("[NVMe] Failed to allocate physical pages for IO queue");
            return -1;
        }

        void *sqVirt = global_allocator.request_page();
        void *cqVirt = global_allocator.request_page();
        if (!sqVirt || !cqVirt) {
            Log::Error("[NVMe] Failed to allocate virtual pages for IO queue");
            return -1;
        }

        global_page_table_manager.map_memory(sqVirt, sqPhys, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);
        global_page_table_manager.map_memory(cqVirt, cqPhys, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);

        uint16_t queueID = AllocateQueueID();

        NvmeCompletion completion{};

        *queue_ptr = NvmeQueue(queueID,
                               reinterpret_cast<uintptr_t>(cqPhys),
                               reinterpret_cast<uintptr_t>(sqPhys),
                               cqVirt,
                               sqVirt,
                               GetCompletionDoorbell(queueID),
                               GetSubmissionDoorbell(queueID),
                               PAGE_SIZE_4K,
                               PAGE_SIZE_4K);

        NvmeCommand createCq{};
        createCq.opcode = AdminCmdCreateIOCompletionQueue;
        createCq.create_io_cq.contiguous = 1;
        createCq.create_io_cq.queue_id = queueID;
        createCq.create_io_cq.queue_size = queue_ptr->CQSize() - 1;
        createCq.prp1 = reinterpret_cast<uintptr_t>(cqPhys);

        admin_queue.SubmitWait(createCq, completion);
        if (completion.status > 0) {
            Log::Warning("[NVMe] Status %u creating I/O completion queue", completion.status);
            return completion.status;
        }

        NvmeCommand createSq{};
        createSq.opcode = AdminCmdCreateIOSubmissionQueue;
        createSq.create_io_sq.contiguous = 1;
        createSq.create_io_sq.queue_id = queueID;
        createSq.create_io_sq.queue_size = queue_ptr->SQSize() - 1;
        createSq.create_io_sq.cq_id = queueID;
        createSq.prp1 = reinterpret_cast<uintptr_t>(sqPhys);

        admin_queue.SubmitWait(createSq, completion);
        if (completion.status > 0) {
            Log::Warning("[NVMe] Status %u creating I/O submission queue", completion.status);
            return completion.status;
        }

        return 0;
    }

    NvmeQueue::NvmeQueue(uint16_t qid, uintptr_t cqBase, uintptr_t sqBase, void *cq, void *sq, uint32_t *cqDB,
                         uint32_t *sqDB, uint16_t csz, uint16_t ssz) {
        queue_id = qid;

        completion_base = cqBase;
        submission_base = sqBase;

        completion_queue = reinterpret_cast<NvmeCompletion *>(cq);
        submission_queue = reinterpret_cast<NvmeCommand *>(sq);

        memset(completion_queue, 0, csz);
        memset(submission_queue, 0, ssz);

        completion_db = cqDB;
        submission_db = sqDB;

        c_queue_size = csz;
        cq_count = csz / sizeof(NvmeCompletion);
        s_queue_size = ssz;
        sq_count = ssz / sizeof(NvmeCommand);
    }

    long NvmeQueue::Consume(NvmeCommand &cmd) { return 0; }

    void NvmeQueue::Submit(NvmeCommand &cmd) {
        cmd.command_id = next_command_id++;
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

    void NvmeQueue::SubmitWait(NvmeCommand &cmd, NvmeCompletion &complet) {
        cmd.command_id = next_command_id++;
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
        while (completion_cycle_state == !completion_queue[cq_head].phase_tag) {
            if (start > 500) {
                complet.status = 32767;
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


    bool NvmeNamespace::read(uint64_t lba, uint32_t sectorCount, void* buffer) {
        NvmeCommand read_cmd = {};
        read_cmd.opcode = NVME_OPCODE_READ; // NVM Read
        read_cmd.ns_id = nsID;
        read_cmd.prp1 = reinterpret_cast<uintptr_t>(buffer);
        read_cmd.read.start_lba = lba;
        read_cmd.read.block_num = sectorCount - 1;

        NvmeCompletion completion{};
        queue->SubmitWait(read_cmd, completion);

        if (completion.status > 0) {
            Log::Warning("[NVMe] Read failed with status %u", completion.status);
            return false;
        }

        return true;
    }

    bool NvmeNamespace::write(uint64_t lba, uint32_t sectorCount, void* buffer) const {
        NvmeCommand write_cmd = {};
        write_cmd.opcode = NVME_OPCODE_WRITE; // NVM Write
        write_cmd.ns_id = nsID;
        write_cmd.prp1 = reinterpret_cast<uintptr_t>(buffer);
        write_cmd.write.start_lba = lba;
        write_cmd.write.block_num = sectorCount - 1;

        NvmeCompletion completion{};
        queue->SubmitWait(write_cmd, completion);

        if (completion.status > 0) {
            Log::Warning("[NVMe] Write failed with status %u", completion.status);
            return false;
        }

        return true;
    }

} // namespace NVMe
