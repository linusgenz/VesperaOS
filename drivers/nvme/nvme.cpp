//
// Created by linus on 07.07.25.
//

#include "nvme.h"
#include "../../include/log.h"
#include "../../kernel/cpu/cpu_manager.h"
#include <kernel/memory.h>
#include <kernel/time.h>

#include "errno.h"
#include <kernel/devices/device_manager.h>

#include "../../filesystem/devfs/devfs.h"

namespace NVMe
{
    NvmeDriver::NvmeDriver(PCI::PCIDeviceHeader* pci_base_address)
    {
        const auto* pci = reinterpret_cast<const PCI::PCIHeader0*>(pci_base_address);
        auto mmio = ((static_cast<uint64_t>(pci->BAR1) << 32) | (pci->BAR0 & 0xFFFFFFF0));
        c_regs = static_cast<Registers*>(kernel::memory::request_pages(4));
        for (int i = 0; i < 4; i++)
        {
            auto virt = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(c_regs) + i * 0x1000);
            auto phys = reinterpret_cast<void*>(mmio + i * 0x1000);
            kernel::memory::map_memory(virt, phys, (1ULL << WriteThrough) | (1ULL << CacheDisabled));
        }

        Log::Info("[NVMe] Initializing Controller...");

        Disable();

        unsigned spin = 50;
        while (spin-- && (c_regs->status & NVME_CSTS_READY))
        {
            kernel::time::sleep_ms(10);
        }

        if (spin <= 0)
        {
            Log::Error("[NVMe] Controller not deactivated, abort...");
            d_status = ControllerError;
            return;
        }

        c_regs->config |= NVME_CFG_DEFAULT_IOCQES | NVME_CFG_DEFAULT_IOSQES;

        void* admCQPhysPage = kernel::memory::request_page();
        void* admSQPhysPage = kernel::memory::request_page();
        if (!admCQPhysPage || !admSQPhysPage)
        {
            Log::Error("[NVMe] No physical pages for admin queues");
            return;
        }

        void* admCQVirtPage = kernel::memory::request_page();
        void* admSQVirtPage = kernel::memory::request_page();
        if (!admCQVirtPage || !admSQVirtPage)
        {
            Log::Error("[NVMe] No virtual pages for admin queues");
            return;
        }

        kernel::memory::map_memory(admCQVirtPage, admCQPhysPage,
                                   (1ULL << WriteThrough) | (1ULL << CacheDisabled));
        kernel::memory::map_memory(admSQVirtPage, admSQPhysPage,
                                   (1ULL << WriteThrough) | (1ULL << CacheDisabled));

        memset(admCQVirtPage, 0, PAGE_SIZE_4K);
        memset(admSQVirtPage, 0, PAGE_SIZE_4K);

        c_regs->admin_completion_q = reinterpret_cast<uintptr_t>(admCQPhysPage);
        c_regs->admin_submission_q = reinterpret_cast<uintptr_t>(admSQPhysPage);

        new(&admin_queue) NVMe::NvmeQueue(0, reinterpret_cast<uintptr_t>(admCQPhysPage),
                                          reinterpret_cast<uintptr_t>(admSQPhysPage),
                                          admCQVirtPage, admSQVirtPage, GetCompletionDoorbell(0),
                                          GetSubmissionDoorbell(0),
                                          PAGE_SIZE_4K, PAGE_SIZE_4K);

        c_regs->admin_q_attr = 0;
        SetAdminCompletionQueueSize(admin_queue.CQSize());
        SetAdminSubmissionQueueSize(admin_queue.SQSize());

        Enable();

        // Warten auf Ready-Status
        spin = 500;
        while (spin-- && !(c_regs->status & NVME_CSTS_READY))
        {
            kernel::time::sleep_ms(10);
        }
        if (spin <= 0)
        {
            Log::Error("[NVMe] Controller not ready");
            d_status = DriverStatus::ControllerError;
            return;
        }

        if (c_regs->status & NVME_CSTS_FATAL)
        {
            Log::Error("[NVMe] Controller error (Fatal)");
            d_status = DriverStatus::ControllerError;
            return;
        }

        if (IdentifyController())
        {
            Log::Error("[NVMe] Controller identifiy failed");
            d_status = DriverStatus::ControllerError;
            return;
        }

        Vector<uint32_t> namespaceIDs;

        if (GetNamespaceList(&namespaceIDs))
        {
            Log::Error("[NVMe] Failed to get namespace list");
            d_status = DriverStatus::ControllerError;
        }

        if (CreateIOQueue(&io_queue))
        {
            Log::Error("Failed to create IO Queue");
        }

        char name[16];
        DeviceManager::AllocUniqueDeviceName("nvme", name, sizeof(name));
        kd = DeviceManager::RegisterController(
            name,
            DeviceClass::Storage,
            BusType::BUS_PCI,
            ControllerType::NVMe,
            nullptr
        );
        DevFS::register_device(kd);

        for (uint32_t namespaceID : namespaceIDs)
        {
            Log::LogMsg("[NVMe] Namespace ID %u", namespaceID);

            void* physBuffer = kernel::memory::request_page();
            NvmeCompletion completion{};
            NvmeCommand identifyNsCmd = {};
            identifyNsCmd.opcode = AdminCmdIdentify;
            identifyNsCmd.prp1 = reinterpret_cast<uintptr_t>(physBuffer);
            identifyNsCmd.ns_id = namespaceID;
            identifyNsCmd.identify.cns = NvmeIdentifyCommand::CnsNamespace;

            admin_queue.SubmitWait(identifyNsCmd, completion);

            auto nsIdentify = static_cast<NVME_IDENTIFY_NAMESPACE_DATA*>(physBuffer);

            auto* ns = new NvmeNamespace(namespaceID, &io_queue, nsIdentify);

            char name_namespace[32];
            DeviceManager::GenerateNVMeDeviceName(kd, name_namespace, sizeof(name_namespace), namespaceID);
            ns->kd = DeviceManager::RegisterBlockDevice(
                ns,
                name_namespace,
                DeviceClass::Storage,
                BusType::BUS_PCI,
                ControllerType::NVMe,
                kd
            );
            DevFS::register_device(ns->kd);
            DeviceManager::FindAndRegisterPartitions(ns->kd);

            namespaces.push_back(ns);
        }

        Log::Ok("[NVMe] Controller initialized");
    }

    long NvmeDriver::IdentifyController()
    {
        controller_identity_phys = reinterpret_cast<uintptr_t>(kernel::memory::request_page());
        if (!controller_identity_phys)
        {
            Log::Error("[NVMe] No physical memory for controller identifiy");
            return -1;
        }

        controller_identity = static_cast<NVME_IDENTIFY_CONTROLLER_DATA*>(kernel::memory::request_page());
        if (!controller_identity)
        {
            Log::Error("[NVMe] No virtual memory for controller identify");
            return -1;
        }

        kernel::memory::map_memory(controller_identity, reinterpret_cast<void*>(controller_identity_phys));

        NvmeCommand identifyCommand{};
        memset(&identifyCommand, 0, sizeof(NvmeCommand));

        identifyCommand.opcode = AdminCmdIdentify;
        identifyCommand.prp1 = controller_identity_phys;
        identifyCommand.identify.cns = NvmeIdentifyCommand::CnsController;

        NvmeCompletion completion{};
        admin_queue.SubmitWait(identifyCommand, completion);

        if (completion.status > 0)
        {
            Log::Error("[NVMe] Controller identification status error: %u", completion.status);
            return completion.status;
        }

        char serial[21] = {};
        memcpy(serial, controller_identity->SN, 20);

        for (int i = 19; i >= 0 && serial[i] == ' '; --i)
        {
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

    long NvmeDriver::GetNamespaceList(Vector<uint32_t>* namespace_ids)
    {
        auto* namespaceList = static_cast<uint32_t*>(kernel::memory::request_page());
        kernel::memory::map_memory(namespaceList, namespaceList,
                                   (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));

        NvmeCommand identifyNsList{};
        memset(&identifyNsList, 0, sizeof(NvmeCommand));

        identifyNsList.opcode = AdminCmdIdentify;
        identifyNsList.prp1 = reinterpret_cast<uint64_t>(namespaceList);

        identifyNsList.identify.cns = NvmeIdentifyCommand::CnsNamespaceList;
        identifyNsList.ns_id = 0;

        NvmeCompletion completion{};
        admin_queue.SubmitWait(identifyNsList, completion);

        if (completion.status > 0)
        {
            return completion.status;
        }

        uint32_t* namespaceListEnd = namespaceList + (PAGE_SIZE_4K / sizeof(uint32_t));
        while (*namespaceList && namespaceList < namespaceListEnd)
        {
            namespace_ids->push_back(*namespaceList++);
        }

        kernel::memory::free_page(namespaceList);

        return 0;
    }

    long NvmeDriver::CreateIOQueue(NvmeQueue* queue_ptr)
    {
        void* sqPhys = kernel::memory::request_page();
        void* cqPhys = kernel::memory::request_page();
        if (!sqPhys || !cqPhys)
        {
            Log::Error("[NVMe] Failed to allocate physical pages for IO queue");
            return -1;
        }

        void* sqVirt = kernel::memory::request_page();
        void* cqVirt = kernel::memory::request_page();
        if (!sqVirt || !cqVirt)
        {
            Log::Error("[NVMe] Failed to allocate virtual pages for IO queue");
            return -1;
        }

        kernel::memory::map_memory(sqVirt, sqPhys, (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
        kernel::memory::map_memory(cqVirt, cqPhys, (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));

        uint16_t queueID = AllocateQueueID();

        NvmeCompletion completion{};

        queue_ptr->~NvmeQueue();

        new(queue_ptr) NvmeQueue(queueID,
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
        if (completion.status > 0)
        {
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
        if (completion.status > 0)
        {
            Log::Warning("[NVMe] Status %u creating I/O submission queue", completion.status);
            return completion.status;
        }

        return 0;
    }

    NvmeQueue::NvmeQueue(uint16_t qid, uintptr_t cq_base, uintptr_t sq_base, void* cq, void* sq, uint32_t* cq_db,
                         uint32_t* sq_db, uint16_t csz, uint16_t ssz)
    {
        queue_id = qid;

        completion_base = cq_base;
        submission_base = sq_base;

        completion_queue = static_cast<NvmeCompletion*>(cq);
        submission_queue = static_cast<NvmeCommand*>(sq);

        memset(completion_queue, 0, csz);
        memset(submission_queue, 0, ssz);

        completion_db = cq_db;
        submission_db = sq_db;

        c_queue_size = csz;
        cq_count = csz / sizeof(NvmeCompletion);
        s_queue_size = ssz;
        sq_count = ssz / sizeof(NvmeCommand);

        queue_mutex.init();
    }

    long NvmeQueue::Consume(NvmeCommand& cmd) { return 0; }

    void NvmeQueue::Submit(NvmeCommand& cmd)
    {
        cmd.command_id = next_command_id++;
        if (next_command_id == 0xffff)
        {
            next_command_id = 0;
        }

        submission_queue[sq_tail] = cmd;

        sq_tail++;
        if (sq_tail >= sq_count)
        {
            sq_tail = 0;
        }

        *submission_db = sq_tail;
    }

    void NvmeQueue::SubmitWait(NvmeCommand& cmd, NvmeCompletion& complet)
    {
        kernel::mutex_guard guard(queue_mutex);

        cmd.command_id = next_command_id++;
        if (next_command_id == 0xffff)
        {
            next_command_id = 0;
        }

        submission_queue[sq_tail] = cmd;

        sq_tail++;
        if (sq_tail >= sq_count)
        {
            sq_tail = 0;
        }

        *submission_db = sq_tail;

        auto start = 0;
        while (completion_cycle_state == !completion_queue[cq_head].phase_tag)
        {
            if (start > 50)
            {
                complet.status = 32767;
                return;
            }
            start++;
            kernel::time::sleep_ms(10);
        }
        complet = completion_queue[cq_head];

        if (++cq_head >= cq_count)
        {
            cq_head = 0;
            completion_cycle_state = !completion_cycle_state;
        }

        *completion_db = cq_head;
    }

    ssize_t NvmeNamespace::read(uint64_t lba, uint32_t sectorCount, void* buffer, size_t bufferSize)
    {
        size_t bytes = sectorCount * sectorSize;
        if (!buffer || sectorCount == 0 || bufferSize < bytes)
            return -EINVAL;

        kernel::mutex_guard guard(namespace_mutex);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        void* dma_phys = kernel::memory::request_pages(pages);
        if (!dma_phys) return -ENOMEM;

        NvmeCommand read_cmd = {};
        read_cmd.opcode = NVME_OPCODE_READ;
        read_cmd.ns_id = nsID;
        read_cmd.prp1 = reinterpret_cast<uintptr_t>(dma_phys);
        read_cmd.read.start_lba = lba;
        read_cmd.read.block_num = sectorCount - 1;

        NvmeCompletion completion{};
        queue->SubmitWait(read_cmd, completion);

        if (completion.status != 0)
        {
            Log::Warning("[NVMe] Read failed with status %u", completion.status);
            kernel::memory::free_pages(dma_phys, pages);
            return -EIO;
        }

        memcpy(buffer, dma_phys, bytes);

        kernel::memory::free_pages(dma_phys, pages);

        return static_cast<ssize_t>(bytes);
    }

    ssize_t NvmeNamespace::write(uint64_t lba, uint32_t sectorCount, void* buffer, size_t bufferSize)
    {
        size_t bytes = sectorCount * sectorSize;
        if (!buffer || sectorCount == 0 || bufferSize < bytes)
            return -EINVAL;

        kernel::mutex_guard guard(namespace_mutex);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        void* dma_phys = kernel::memory::request_pages(pages);
        if (!dma_phys) return -ENOMEM;

        memcpy(dma_phys, buffer, bytes);

        NvmeCommand write_cmd = {};
        write_cmd.opcode = NVME_OPCODE_WRITE; // NVM Write
        write_cmd.ns_id = nsID;
        write_cmd.prp1 = reinterpret_cast<uintptr_t>(dma_phys);
        write_cmd.write.start_lba = lba;
        write_cmd.write.block_num = sectorCount - 1;

        NvmeCompletion completion{};
        queue->SubmitWait(write_cmd, completion);

        kernel::memory::free_pages(dma_phys, pages);

        if (completion.status != 0)
        {
            Log::Warning("[NVMe] Write failed with status %u", completion.status);
            return -EIO;
        }

        return static_cast<ssize_t>(bytes);
    }
} // namespace NVMe
