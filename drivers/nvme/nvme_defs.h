//
// Created by Linus on 11.07.25.
//

#ifndef NVME_DEFS_H
#define NVME_DEFS_H

#include <cstdint>

namespace NVMe {
#define NVME_CAP_CMBS (1 << 57) // Controller memory buffer supported
#define NVME_CAP_PMRS (1 << 56) // Persistent memory region supported
#define NVME_CAP_BPS (1 << 45) // Boot partition support
#define NVME_CAP_NVM_CMD_SET (1UL << 37) // NVM command set supported
#define NVME_CAP_NSSRS (1UL << 36) // NVM subsystem reset supported
#define NVME_CAP_CQR (1 << 16) // Contiguous Queues Required

#define NVME_CAP_MPS_MASK 0xfU
#define NVME_CAP_MPSMAX(x) ((x >> 52) & NVME_CAP_MPS_MASK) // Max supported memory page size (2 ^ (12 + MPSMAX))
#define NVME_CAP_MPSMIN(x) ((x >> 48) & NVME_CAP_MPS_MASK) // Min supported memory page size (2 ^ (12 + MPSMIN))

#define NVME_CAP_DSTRD_MASK 0xfU
#define NVME_CAP_DSTRD(x) (((x) >> 32) & NVME_CAP_DSTRD_MASK) // Doorbell stride (2 ^ (2 + DSTRD)) bytes

#define NVME_CAP_MQES_MASK 0xffffU
#define NVME_CAP_MQES(x) ((x) & NVME_CAP_MQES_MASK) // Maximum queue entries supported

#define NVME_CFG_MPS_MASK 0xfUL
#define NVME_CFG_MPS(x) (((x) & NVME_CFG_MPS_MASK) << 7) // Host memory page size (2 ^ (12 + MPSMIN))
#define NVME_CFG_CSS_MASK 0b111U // Command set selected
#define NVME_CFG_CSS(x) (((x) & NVME_CFG_CSS_MASK) << 4)
#define NVME_CFG_ENABLE (1 << 0)

#define NVME_CFG_DEFAULT_IOCQES (4 << 20) // 16 bytes so log2(16) = 4
#define NVME_CFG_DEFAULT_IOSQES (6 << 16) // 64 bytes so log2(64) = 6

#define NVME_CSTS_FATAL (1 << 1)
#define NVME_CSTS_READY (1 << 0) // Set to 1 when the controller is ready to accept submission queue doorbell writes
#define NVME_CSTS_NSSRO (1 << 4) // NVM Subsystem reset occurred

#define NVME_AQA_AQS_MASK 0xfffU // Admin queue size mask
#define NVME_AQA_ACQS(x) (((x) & NVME_AQA_AQS_MASK) << 16) // Admin completion queue size
#define NVME_AQA_ASQS(x) (((x) & NVME_AQA_AQS_MASK) << 0) // Admin submission queue size

#define NVME_NSSR_RESET_VALUE 0x4E564D65 // "NVME", initiates a reset

#define NVME_OPCODE_READ 0x02
#define NVME_OPCODE_WRITE 0x01

#define PAGE_SIZE_4K 0x1000

    struct Registers {
        uint64_t cap;
        uint32_t version;
        uint32_t int_mask;
        uint32_t int_mask_clear;
        uint32_t config;
        uint32_t reserved;
        uint32_t status;
        uint32_t nvm_subsystem_reset;
        uint32_t admin_q_attr;
        uint64_t admin_submission_q;
        uint64_t admin_completion_q;
        uint32_t controller_mem_buffer_location;
        uint32_t controller_mem_buffer_size;
        uint32_t boot_partition_info;
        uint32_t boot_partition_read_select;
        uint64_t boot_partition_mem_buffer_location;
        uint64_t boot_partition_mem_space_control;
        uint64_t controller_mem_buffer_mem_space_control;
        uint32_t controller_memory_buffer_status;
    } __attribute__((packed));

    enum DriverStatus {
        ControllerNotReady,
        ControllerError,
        ControllerReady,
    };

    struct NvmeCreateIoCompletionQueueCommand {
        struct {
            uint32_t queue_id: 16;
            uint32_t queue_size: 16;
        } __attribute__((packed));

        struct {
            uint32_t contiguous: 1;
            uint32_t int_enable: 1;
            uint32_t reserved: 14;
            uint32_t int_vector: 16;
        } __attribute__((packed));
    };

    struct NvmeCreateIoSubmissionQueueCommand {
        struct {
            uint32_t queue_id: 16;
            uint32_t queue_size: 16;
        } __attribute__((packed));

        struct {
            uint32_t contiguous: 1;
            uint32_t priority: 2;
            uint32_t reserved: 13;
            uint32_t cq_id: 16;
        } __attribute__((packed));
    };

    struct NvmeIdentifyCommand {
        enum {
            CnsNamespace = 0,
            CnsController = 1,
            CnsNamespaceList = 2,
        };

        struct {
            uint32_t cns: 8;
            uint32_t reserved: 8;
            uint32_t cnt_id: 16;
        } __attribute__((packed));

        uint32_t nvm_set_id;
    };

    struct NvmeDeleteIoQueueCommand {
        uint32_t queue_id;
    };

    struct NvmeSetFeaturesCommand {
        enum {
            FeatureIdNumberOfQueues = 0x7,
        };

        struct {
            uint32_t feature_id: 8;
            uint32_t reserved: 23;
            uint32_t save: 1;
        } __attribute__((packed));

        uint32_t dw11;
        uint32_t dw12;
        uint32_t dw13;
    };

    struct NvmeReadCommand {
        uint64_t start_lba;

        struct {
            uint32_t block_num: 16;
            uint32_t reserved: 10;
            uint32_t pr_info: 4;
            uint32_t force_unit_access: 1;
            uint32_t limited_retry: 1;
        } __attribute__((packed));
    };

    struct NvmeWriteCommand {
        uint64_t start_lba;

        struct {
            uint32_t block_num: 16;
            uint32_t reserved2: 4;
            uint32_t directive_type: 4;
            uint32_t reserved: 2;
            uint32_t pr_info: 4;
            uint32_t force_unit_access: 1;
            uint32_t limited_retry: 1;
        } __attribute__((packed));
    };

    struct NvmeCommand {
        struct {
            uint32_t opcode: 8;
            uint32_t fuse: 2;
            uint32_t reserved: 4;
            uint32_t psdt: 2;
            uint32_t command_id: 16;
        } __attribute__((packed));

        uint32_t ns_id;
        uint64_t reserved2;
        uint64_t metadata_ptr;
        uint64_t prp1;
        uint64_t prp2;

        union {
            struct {
                uint32_t cmd_dwords[6];
            };

            NvmeIdentifyCommand identify;
            NvmeCreateIoCompletionQueueCommand create_io_cq;
            NvmeCreateIoSubmissionQueueCommand create_io_sq;
            NvmeDeleteIoQueueCommand delete_io_q;
            NvmeSetFeaturesCommand set_features;
            NvmeReadCommand read;
            NvmeWriteCommand write;
        };
    };

    struct NvmeCompletion {
        uint32_t dw0;
        uint32_t reserved;

        struct {
            uint32_t sq_head: 16;
            uint32_t sq_id: 16;
        } __attribute__((packed));

        struct {
            uint32_t command_id: 16;
            uint32_t phase_tag: 1;
            uint32_t status: 15;
        } __attribute__((packed));
    };

    class NvmeQueue {
        uint16_t queue_id = 0;

        uintptr_t completion_base;
        uintptr_t submission_base;

        NvmeCompletion *completion_queue;
        NvmeCommand *submission_queue;

        volatile uint32_t *completion_db;
        volatile uint32_t *submission_db;

        uint16_t c_queue_size = 0;
        uint16_t s_queue_size = 0;

        uint16_t cq_count = 0;
        uint16_t sq_count = 0;

        uint16_t next_command_id = 0;

    public:
        bool completion_cycle_state = true;
        uint16_t cq_head = 0;
        uint16_t sq_tail = 0;

        NvmeQueue(uint16_t qid, uintptr_t cq_base, uintptr_t sq_base, void *cq, void *sq, uint32_t *cq_db,
                  uint32_t *sq_db, uint16_t csz, uint16_t ssz);

        NvmeQueue() = default;

        long Consume(NvmeCommand &cmd);

        void Submit(NvmeCommand &cmd);

        void SubmitWait(NvmeCommand &cmd, NvmeCompletion &complet);

        __attribute__((always_inline)) uint16_t CQSize() { return cq_count; }
        __attribute__((always_inline)) uint16_t SQSize() { return sq_count; }
        __attribute__((always_inline)) uintptr_t CQBase() { return completion_base; }
        __attribute__((always_inline)) uintptr_t SQBase() { return submission_base; }
    };

    struct ControllerIdentity {
        uint16_t vendor_id;
        uint16_t subsystem_vendor_id;
        char serial_number[20];
        char model_number[40];
        char firmware_revision[8];
        uint8_t recommended_arbitration_burst;
        uint8_t ieee[3];
        uint8_t cmic;
        uint8_t maximum_data_transfer_size;
        uint16_t controller_id;
        uint32_t version;
        uint32_t rtd3_resume_latency;
        uint32_t rtd3_entry_latency;
        uint32_t oaes;
        uint32_t controller_attributes;
        uint16_t rrls;
        uint8_t reserved[9];
        uint8_t controller_type;
        uint8_t f_guid[16];
        uint16_t crdt[3];
        uint8_t reserved2[122];
        uint16_t oacs;
        uint8_t acl;
        uint8_t aerl;
        uint8_t firmware_updates;
        uint8_t log_page_attributes;
        uint8_t error_log_page_entries;
        uint8_t number_of_power_states;
        uint8_t apsta;
        uint16_t wc_temp;
        uint16_t cc_temp;
        uint16_t mtfa;
        uint32_t host_memory_buffer_preferred_size;
        uint32_t host_memory_buffer_minimum_size;
        uint8_t unused[232];
        uint8_t sq_entry_size;
        uint8_t cq_entry_size;
        uint16_t max_cmd;
        uint32_t num_namespaces;
        uint8_t unused2[248];
        int8_t name[256];
        uint8_t unused3[3072];
    };

    static_assert(sizeof(ControllerIdentity) == 4096);

    enum AdminCommands {
        AdminCmdDeleteIOSubmissionQueue = 0x0,
        AdminCmdCreateIOSubmissionQueue = 0x1,
        AdminCmdGetLogPage = 0x2,
        AdminCmdDeleteIOCompletionQueue = 0x4,
        AdminCmdCreateIOCompletionQueue = 0x5,
        AdminCmdIdentify = 0x6,
        AdminCmdSetFeatures = 0x9,
    };

    union NvmeLbaFormat {
        uint32_t dw;

        struct {
            uint32_t metadata_size: 16;
            uint32_t lba_data_size: 8;
            uint32_t relative_performance: 2;
            uint32_t reserved: 6;
        } __attribute__((packed));
    };

    struct NamespaceIdentity {
        uint64_t namespace_size;
        uint64_t ns_cap;
        uint64_t ns_use;
        uint8_t ns_feat;
        uint8_t num_lba_formats;
        uint8_t fmt_lba_size;
        uint8_t unused[101];
        NvmeLbaFormat lba_formats[16];
        uint8_t reserved[192];
        uint8_t vendor[3712];
    } __attribute__((packed));
}

#endif //NVME_DEFS_H
