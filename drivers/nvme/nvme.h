//
// Created by linus on 07.07.25.
//

#ifndef NVME_H
#define NVME_H
#include <vector.h>

#include "../../kernel/devices/blockdevice.h"
#include "../pci/pci.h"
#include "kernel/devices/device_manager.h"
#include "log.h"
#include "nvme_defs.h"

namespace NVMe {
    class NvmeQueue {
        uint16_t queue_id = 0;

        phys_addr_t completion_base{};
        phys_addr_t submission_base{};

        NVME_COMPLETION_ENTRY* completion_queue{};
        NVME_COMMAND* submission_queue{};

        volatile uint32_t* completion_db{};
        volatile uint32_t* submission_db{};

        uint16_t c_queue_size = 0;
        uint16_t s_queue_size = 0;

        uint16_t cq_count = 0;
        uint16_t sq_count = 0;

        uint16_t next_command_id = 0;

        kernel::mutex_t queue_mutex{};

       public:
        bool completion_cycle_state = true;
        uint16_t cq_head = 0;
        uint16_t sq_tail = 0;

        NvmeQueue(
            uint16_t qid, phys_addr_t cq_base, phys_addr_t sq_base, virt_addr_t cq, virt_addr_t sq,
            volatile uint32_t* cq_db, volatile uint32_t* sq_db, uint16_t csz, uint16_t ssz
        );

        ~NvmeQueue() = default;

        NvmeQueue() = default;

        static long consume(NVME_COMMAND& cmd);

        void submit(NVME_COMMAND& cmd);

        void submit_wait(NVME_COMMAND& cmd, NVME_COMPLETION_ENTRY& complet);

        [[nodiscard]] uint16_t get_queue_id() const {
            return queue_id;
        }

        [[nodiscard]] uint16_t cq_size() const {
            return cq_count;
        }
        [[nodiscard]] uint16_t sq_size() const {
            return sq_count;
        }
        [[nodiscard]] phys_addr_t cq_base() const {
            return completion_base;
        }
        [[nodiscard]] phys_addr_t sq_base() const {
            return submission_base;
        }
    };

    class NvmeNamespace final : public BlockDevice {
       public:
        NvmeNamespace(uint32_t nsid, NvmeQueue* io_queue, const NVME_IDENTIFY_NAMESPACE_DATA* identify)
            : ns_id(nsid)
            , queue(io_queue) {
            uint8_t lba_format_index = identify->FLBAS.LbaFormatIndex;
            uint8_t lbads = identify->LBAF[lba_format_index].LBADS;
            sector_size = 1 << lbads;

            ncap = identify->NCAP;

            namespace_mutex.init();
        }

        KernelDevice* kd{};

        ssize_t read(uint64_t lba, size_t sector_count, void* buffer, size_t buffer_size) override;
        ssize_t write(uint64_t lba, size_t sector_count, void* buffer, size_t buffer_size) override;

        [[nodiscard]] size_t get_size() const override {
            return sector_size * ncap;
        }

        [[nodiscard]] size_t get_sector_size() const override {
            return sector_size;
        }

       private:
        uint32_t ns_id;
        NvmeQueue* queue;
        uint32_t sector_size;
        uint64_t ncap;
        kernel::mutex_t namespace_mutex;
    };

    class NvmeDriver {
        volatile NVME_CONTROLLER_REGISTERS* c_regs = nullptr;
        NvmeQueue admin_queue;
        NvmeQueue io_queue;
        NVME_IDENTIFY_CONTROLLER_DATA* controller_identity = nullptr;
        phys_addr_t controller_identity_phys{};

        KernelDevice* kd;

        uint16_t next_queue_id = 1;

        Vector<NvmeNamespace*> namespaces;

        __attribute__((always_inline)) void disable() const {
            c_regs->CC.EN = 0;
        }

        __attribute__((always_inline)) void enable() const {
            c_regs->CC.EN = 1;
        }

        __attribute__((always_inline)) uint16_t allocate_queue_id() {
            return next_queue_id++;
        }

        [[nodiscard]] __attribute__((always_inline)) volatile uint32_t* get_submission_doorbell(uint16_t qid) const {
            size_t stride_words = (4 << get_doorbell_stride()) / sizeof(uint32_t);
            return &c_regs->Doorbells[2 * qid * stride_words];
        }

        [[nodiscard]] volatile __attribute__((always_inline)) uint32_t* get_completion_doorbell(uint16_t qid) const {
            size_t stride_words = (4 << get_doorbell_stride()) / sizeof(uint32_t);
            return &c_regs->Doorbells[(2 * qid + 1) * stride_words];
        }

        __attribute__((always_inline)) void set_admin_submission_queue_size(uint16_t sz) const {
            c_regs->AQA.ASQS = sz - 1;
        }

        __attribute__((always_inline)) void set_admin_completion_queue_size(uint16_t sz) const {
            c_regs->AQA.ACQS = sz - 1;
        }

        [[nodiscard]] __attribute__((always_inline)) uint32_t get_doorbell_stride() const {
            return static_cast<uint32_t>(c_regs->CAP.DSTRD);
        }

        long identify_controller();

        long get_namespace_list(Vector<uint32_t>* namespace_ids);
        void shutdown();

        long delete_io_queue(NvmeQueue* queue_ptr);
        long create_io_queue(NvmeQueue* queue_ptr);

       public:
        enum driver_status {
            controller_not_ready,
            controller_error,
            controller_ready,
            controller_shutdown
        } d_status = controller_not_ready;

        explicit NvmeDriver(PCI::PCIDeviceHeader* pci_base_address);
        ~NvmeDriver();

        [[nodiscard]] const Vector<NvmeNamespace*>& get_namespaces() const {
            return namespaces;
        }
    };
}  // namespace NVMe

#endif  // NVME_H