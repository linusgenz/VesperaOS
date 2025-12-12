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
        c_regs = static_cast<NVME_CONTROLLER_REGISTERS*>(kernel::memory::request_pages(4));
        for (int i = 0; i < 4; i++)
        {
            auto virt = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(c_regs) + i * 0x1000);
            auto phys = reinterpret_cast<void*>(mmio + i * 0x1000);
            kernel::memory::map_memory(virt, phys, (1ULL << WriteThrough) | (1ULL << CacheDisabled));
        }

        Log::Info("[NVMe] Initializing Controller...");

        Disable();

        unsigned spin = 50;
        while (spin-- && (c_regs->CSTS.RDY))
        {
            kernel::time::sleep_ms(10);
        }

        if (spin <= 0)
        {
            Log::Error("[NVMe] Controller not deactivated, abort...");
            d_status = ControllerError;
            return;
        }

        c_regs->CC.IOSQES = 6; // 2^6 = 64 bytes
        c_regs->CC.IOCQES = 4; // 2^4 = 16 bytes

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

        c_regs->ACQ.ACQB = reinterpret_cast<uintptr_t>(admCQPhysPage) >> 12;
        c_regs->ASQ.ASQB = reinterpret_cast<uintptr_t>(admSQPhysPage) >> 12;

        new(&admin_queue) NVMe::NvmeQueue(0, reinterpret_cast<uintptr_t>(admCQPhysPage),
                                          reinterpret_cast<uintptr_t>(admSQPhysPage),
                                          admCQVirtPage, admSQVirtPage, GetCompletionDoorbell(0),
                                          GetSubmissionDoorbell(0),
                                          PAGE_SIZE_4K, PAGE_SIZE_4K);

        c_regs->AQA.ACQS = 0;
        c_regs->AQA.ASQS = 0;
        SetAdminCompletionQueueSize(admin_queue.CQSize());
        SetAdminSubmissionQueueSize(admin_queue.SQSize());

        Enable();

        spin = 50;
        while (spin-- && !c_regs->CSTS.RDY)
        {
            kernel::time::sleep_ms(10);
        }
        if (spin <= 0)
        {
            Log::Error("[NVMe] Controller not ready");
            d_status = DriverStatus::ControllerError;
            return;
        }

        if (c_regs->CSTS.CFS)
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
            NVME_COMPLETION_ENTRY completion{};
            NVME_COMMAND identifyNsCmd = {};
            identifyNsCmd.CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
            identifyNsCmd.PRP1 = reinterpret_cast<uintptr_t>(physBuffer);
            identifyNsCmd.NSID = namespaceID;
            identifyNsCmd.u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE;

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

        NVME_COMMAND identifyCommand{};
        memset(&identifyCommand, 0, sizeof(NVME_COMMAND));

        identifyCommand.CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
        identifyCommand.PRP1 = controller_identity_phys;
        identifyCommand.u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_CONTROLLER;

        NVME_COMPLETION_ENTRY completion{};
        admin_queue.SubmitWait(identifyCommand, completion);

        if (completion.DW3.Status > 0)
        {
            Log::Error("[NVMe] Controller identification status error: %u", completion.DW3.Status);
            return completion.DW3.Status;
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

        NVME_COMMAND identifyNsList{};
        memset(&identifyNsList, 0, sizeof(NVME_COMMAND));

        identifyNsList.CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
        identifyNsList.PRP1 = reinterpret_cast<uint64_t>(namespaceList);

        identifyNsList.u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_ACTIVE_NAMESPACES;
        identifyNsList.NSID = 0;

        NVME_COMPLETION_ENTRY completion{};
        admin_queue.SubmitWait(identifyNsList, completion);

        if (completion.DW3.Status > 0)
        {
            return completion.DW3.Status;
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

        NVME_COMPLETION_ENTRY completion{};

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

        NVME_COMMAND createCq{};
        createCq.CDW0.OPC = NVME_ADMIN_COMMAND_CREATE_IO_CQ;
        createCq.u.CREATEIOCQ.CDW10.QID = queueID;
        createCq.u.CREATEIOCQ.CDW10.QSIZE = queue_ptr->CQSize() - 1;
        createCq.u.CREATEIOCQ.CDW11.PC = 1;
        createCq.PRP1 = reinterpret_cast<uintptr_t>(cqPhys);

        admin_queue.SubmitWait(createCq, completion);
        if (completion.DW3.Status > 0)
        {
            Log::Warning("[NVMe] Status %u creating I/O completion queue", completion.DW3.Status);
            return completion.DW3.Status;
        }

        NVME_COMMAND createSq{};
        createSq.CDW0.OPC = NVME_ADMIN_COMMAND_CREATE_IO_SQ;
        createSq.u.CREATEIOSQ.CDW10.QID = queueID;
        createSq.u.CREATEIOSQ.CDW10.QSIZE = queue_ptr->SQSize() - 1;
        createSq.u.CREATEIOSQ.CDW11.PC = 1;
        createSq.u.CREATEIOSQ.CDW11.CQID = queueID;
        createSq.PRP1 = reinterpret_cast<uintptr_t>(sqPhys);

        admin_queue.SubmitWait(createSq, completion);
        if (completion.DW3.Status > 0)
        {
            Log::Warning("[NVMe] Status %u creating I/O submission queue", completion.DW3.Status);
            return completion.DW3.Status;
        }

        return 0;
    }

    NvmeQueue::NvmeQueue(uint16_t qid, uintptr_t cq_base, uintptr_t sq_base, void* cq, void* sq, volatile uint32_t* cq_db,
                         volatile uint32_t* sq_db, uint16_t csz, uint16_t ssz)
    {
        queue_id = qid;

        completion_base = cq_base;
        submission_base = sq_base;

        completion_queue = static_cast<NVME_COMPLETION_ENTRY*>(cq);
        submission_queue = static_cast<NVME_COMMAND*>(sq);

        memset(completion_queue, 0, csz);
        memset(submission_queue, 0, ssz);

        completion_db = cq_db;
        submission_db = sq_db;

        c_queue_size = csz;
        cq_count = csz / sizeof(NVME_COMPLETION_ENTRY);
        s_queue_size = ssz;
        sq_count = ssz / sizeof(NVME_COMMAND);

        queue_mutex.init();
    }

    long NvmeQueue::Consume(NVME_COMMAND& cmd) { return 0; }

    void NvmeQueue::Submit(NVME_COMMAND& cmd)
    {
        cmd.CDW0.CID = next_command_id++;
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

    void NvmeQueue::SubmitWait(NVME_COMMAND& cmd, NVME_COMPLETION_ENTRY& complet)
    {
        kernel::mutex_guard guard(queue_mutex);

        cmd.CDW0.CID = next_command_id++;
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
        constexpr uint16_t NVME_CS_P_MASK   = 0x0001;
        while (completion_cycle_state == !completion_queue[cq_head].DW3.P)
        {
            if (start > 50)
            {
              /*   NVME_COMMAND_STATUS timeout_status{};
               timeout_status.P = 1;
                timeout_status.SCT = 1;
                timeout_status.SC = 0x01;
                timeout_status.DNR = 1;
                timeout_status.M = 0;*/
                complet.DW3.Status = 10000;
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

        NVME_COMMAND read_cmd = {};
        read_cmd.CDW0.OPC = NVME_NVM_COMMAND_READ;
        read_cmd.NSID = nsID;
        read_cmd.PRP1 = reinterpret_cast<uintptr_t>(dma_phys);
        read_cmd.u.READWRITE.LBALOW = static_cast<uint32_t>(lba & 0xFFFFFFFFULL);
        read_cmd.u.READWRITE.LBAHIGH = static_cast<uint32_t>(lba >> 32);
        read_cmd.u.READWRITE.CDW12.NLB = sectorCount - 1;

        NVME_COMPLETION_ENTRY completion{};
        queue->SubmitWait(read_cmd, completion);

        if (completion.DW3.Status > 0)
        {
            Log::Warning("[NVMe] Read failed with status %u", completion.DW3.Status);
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

        NVME_COMMAND write_cmd = {};
        write_cmd.CDW0.OPC = NVME_NVM_COMMAND_WRITE; // NVM Write
        write_cmd.NSID = nsID;
        write_cmd.PRP1 = reinterpret_cast<uintptr_t>(dma_phys);
        write_cmd.u.READWRITE.LBALOW = static_cast<uint32_t>(lba & 0xFFFFFFFFULL);
        write_cmd.u.READWRITE.LBAHIGH = static_cast<uint32_t>(lba >> 32);
        write_cmd.u.READWRITE.CDW12.NLB = sectorCount - 1;

        NVME_COMPLETION_ENTRY completion{};
        queue->SubmitWait(write_cmd, completion);

        kernel::memory::free_pages(dma_phys, pages);

        if (completion.DW3.Status > 0)
        {
            Log::Warning("[NVMe] Write failed with status %u", completion.DW3.Status);
            return -EIO;
        }

        return static_cast<ssize_t>(bytes);
    }
} // namespace NVMe
