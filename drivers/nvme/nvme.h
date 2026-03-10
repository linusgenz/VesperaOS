//
// Created by linus on 07.07.25.
//

#ifndef NVME_H
#define NVME_H
#include <vespera/devices/block.h>
#include "../pci/pci.h"
#include "nvme_defs.h"
#include "vespera/devices/device_manager.h"
#include <klib/vector.h>

namespace nvme {
    class NvmeQueue {
        u16 queue_id_ = 0;

        phys_addr_t completion_base_{};
        phys_addr_t submission_base_{};

        NVME_COMPLETION_ENTRY* completion_queue_{};
        NVME_COMMAND* submission_queue_{};

        volatile u32* completion_db_{};
        volatile u32* submission_db_{};

        u16 cq_count_ = 0;
        u16 sq_count_ = 0;

        u16 next_command_id_ = 0;

        kernel::Mutex queue_mutex_{};

       public:
        bool completion_cycle_state = true;
        u16 cq_head = 0;
        u16 sq_tail = 0;

        NvmeQueue(
            u16 qid, phys_addr_t cq_base, phys_addr_t sq_base, virt_addr_t cq, virt_addr_t sq,
            volatile u32* cq_db, volatile u32* sq_db, u16 csz, u16 ssz
        );

        ~NvmeQueue() = default;

        NvmeQueue() = default;

        static long consume(NVME_COMMAND& cmd);

        void submit(NVME_COMMAND& cmd);

        void submit_wait(NVME_COMMAND& cmd, NVME_COMPLETION_ENTRY& complet);

        [[nodiscard]] u16 get_queue_id() const {
            return queue_id_;
        }

        [[nodiscard]] u16 cq_size() const {
            return cq_count_;
        }
        [[nodiscard]] u16 sq_size() const {
            return sq_count_;
        }
        [[nodiscard]] phys_addr_t cq_base() const {
            return completion_base_;
        }
        [[nodiscard]] phys_addr_t sq_base() const {
            return submission_base_;
        }
    };

    class NvmeNamespace final : public BlockDevice {
       public:
        NvmeNamespace(u32 nsid, NvmeQueue* io_queue, const NVME_IDENTIFY_NAMESPACE_DATA* identify)
            : ns_id_(nsid)
            , queue_(io_queue), ncap_(identify->ncap) {
            const u8 lba_format_index = identify->flbas.lba_format_index;
            u8 lbads = identify->lbaf[lba_format_index].lbads;
            sector_size_ = 1 << lbads;

            namespace_mutex_.init();
        }

        KernelDevice* kd{};

        isize read(u64 lba, usize sector_count, void* buffer, usize buffer_size) override;
        isize write(u64 lba, usize sector_count, void* buffer, usize buffer_size) override;

        [[nodiscard]] usize get_size() const override {
            return sector_size_ * ncap_;
        }

        [[nodiscard]] usize get_sector_size() const override {
            return sector_size_;
        }

       private:
        u32 ns_id_;
        NvmeQueue* queue_;
        u32 sector_size_;
        u64 ncap_;
        kernel::Mutex namespace_mutex_;
    };

    class NvmeDriver final : public IDriverLifecycle, public ISmartDevice {
        volatile NVME_CONTROLLER_REGISTERS* c_regs_ = nullptr;
        NvmeQueue admin_queue_;
        NvmeQueue io_queue_;
        NVME_IDENTIFY_CONTROLLER_DATA* controller_identity_ = nullptr;
        phys_addr_t controller_identity_phys_{};

        KernelDevice* kd_;

        u16 next_queue_id_ = 1;

        Vector<NvmeNamespace*> namespaces_;

        __attribute__((always_inline)) void disable() const {
            c_regs_->cc.en = 0;
        }

        __attribute__((always_inline)) void enable() const {
            c_regs_->cc.en = 1;
        }

        __attribute__((always_inline)) u16 allocate_queue_id() {
            return next_queue_id_++;
        }

        [[nodiscard]] __attribute__((always_inline)) volatile u32* get_submission_doorbell(u16 qid) const {
            usize stride_words = (4 << get_doorbell_stride()) / sizeof(u32);
            return &c_regs_->doorbells[static_cast<usize>(2) * qid * stride_words];
        }

        [[nodiscard]] volatile __attribute__((always_inline)) u32* get_completion_doorbell(u16 qid) const {
            usize stride_words = (4 << get_doorbell_stride()) / sizeof(u32);
            return &c_regs_->doorbells[(2 * qid + 1) * stride_words];
        }

        __attribute__((always_inline)) void set_admin_submission_queue_size(u16 sz) const {
            c_regs_->aqa.asqs = sz - 1;
        }

        __attribute__((always_inline)) void set_admin_completion_queue_size(u16 sz) const {
            c_regs_->aqa.acqs = sz - 1;
        }

        [[nodiscard]] __attribute__((always_inline)) u32 get_doorbell_stride() const {
            return static_cast<u32>(c_regs_->cap.dstrd);
        }

        long identify_controller();

        long get_namespace_list(Vector<u32>* namespace_ids);
        void shutdown();

        long delete_io_queue(NvmeQueue* queue_ptr);
        long create_io_queue(NvmeQueue* queue_ptr);

       public:
        DRIVER_STATUS d_status = CONTROLLER_NOT_READY;

        explicit NvmeDriver(pci::PCI_DEVICE_HEADER* pci_base_address);
        ~NvmeDriver() override;

        void on_shutdown() override { shutdown(); }
        void on_suspend()  override { /* optional */ }

        bool smart_read_data(u8* out_buf) override;
        bool smart_get_attributes(SmartAttributes* out) override;

        [[nodiscard]] const Vector<NvmeNamespace*>& get_namespaces() const {
            return namespaces_;
        }
    };
}  // namespace NVMe

#endif  // NVME_H