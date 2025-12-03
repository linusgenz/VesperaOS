//
// Created by linus on 07.07.25.
//

#ifndef NVME_H
#define NVME_H
#include "../pci/pci.h"
#include <vector.h>
#include "../../kernel/devices/blockdevice.h"
#include "nvme_defs.h"

namespace NVMe {

    class NvmeNamespace final : public BlockDevice {
    public:
        NvmeNamespace(uint32_t nsid, NvmeQueue* ioQueue, uint32_t lbaSize)
            : nsID(nsid), queue(ioQueue), sectorSize(lbaSize) {}

        bool read(uint64_t lba, uint32_t sectorCount, void* buffer) override;
        bool write(uint64_t lba, uint32_t sectorCount, void* buffer) override;

        uint32_t get_sector_size() override {
            return sectorSize;
        }

    private:
        uint32_t nsID;
        NvmeQueue* queue;
        uint32_t sectorSize;
    };

    class NvmeDriver {
        Registers *c_regs = nullptr;
        NvmeQueue admin_queue;
        NvmeQueue io_queue;
        ControllerIdentity *controller_identity = nullptr;
        uintptr_t controller_identity_phys = 0;

        uint16_t next_queue_id = 1;

        Vector<NvmeNamespace*> namespaces;

        __attribute__((always_inline)) void Disable() const {
            c_regs->config &= ~NVME_CFG_ENABLE;
        }

        __attribute__((always_inline)) void Enable() const {
            c_regs->config |= NVME_CFG_ENABLE;
        }

        __attribute__((always_inline)) uint16_t AllocateQueueID() {
            return next_queue_id++;
        }

        __attribute__((always_inline)) uint32_t *GetSubmissionDoorbell(uint16_t qid) {
            return reinterpret_cast<uint32_t *>(
                reinterpret_cast<uintptr_t>(c_regs) + 0x1000 + (2 * qid) * (4 << GetDoorbellStride()));
        }

        __attribute__((always_inline)) uint32_t *GetCompletionDoorbell(uint16_t qid) {
            return reinterpret_cast<uint32_t *>(
                reinterpret_cast<uintptr_t>(c_regs) + 0x1000 + (2 * qid + 1) * (4 << GetDoorbellStride()));
        }

        __attribute__((always_inline)) void SetAdminSubmissionQueueSize(uint16_t sz) const {
            c_regs->admin_q_attr = (c_regs->admin_q_attr & ~NVME_AQA_ASQS(NVME_AQA_AQS_MASK)) | NVME_AQA_ASQS(sz - 1);
        }

        __attribute__((always_inline)) void SetAdminCompletionQueueSize(uint16_t sz) const {
            c_regs->admin_q_attr = (c_regs->admin_q_attr & ~NVME_AQA_ACQS(NVME_AQA_AQS_MASK)) | NVME_AQA_ACQS(sz - 1);
        }

        [[nodiscard]] __attribute__((always_inline)) uint32_t GetDoorbellStride() const {
            return NVME_CAP_DSTRD(c_regs->cap);
        }

        long IdentifyController();

        long GetNamespaceList(Vector<uint32_t> *namespace_ids);

        long CreateIOQueue(NvmeQueue *queue_ptr);

    public:
        enum DriverStatus {
            ControllerNotReady,
            ControllerError,
            ControllerReady
        } d_status = ControllerNotReady;

        explicit NvmeDriver(PCI::PCIDeviceHeader *pci_base_address);

        [[nodiscard]] const Vector<NvmeNamespace*>& get_namespaces() const { return namespaces; }
    };
}

#endif //NVME_H
