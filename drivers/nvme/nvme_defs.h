//
// Created by Linus on 11.07.25.
//

#ifndef NVME_DEFS_H
#define NVME_DEFS_H

#include <stdint.h>

namespace nvme {
    enum NVME_ADMIN_COMMANDS {
        NVME_ADMIN_COMMAND_DELETE_IO_SQ = 0x00,
        NVME_ADMIN_COMMAND_CREATE_IO_SQ = 0x01,
        NVME_ADMIN_COMMAND_GET_LOG_PAGE = 0x02,
        NVME_ADMIN_COMMAND_DELETE_IO_CQ = 0x04,
        NVME_ADMIN_COMMAND_CREATE_IO_CQ = 0x05,
        NVME_ADMIN_COMMAND_IDENTIFY = 0x06,
        NVME_ADMIN_COMMAND_ABORT = 0x08,
        NVME_ADMIN_COMMAND_SET_FEATURES = 0x09,
        NVME_ADMIN_COMMAND_GET_FEATURES = 0x0A,
        NVME_ADMIN_COMMAND_ASYNC_EVENT_REQUEST = 0x0C,
        NVME_ADMIN_COMMAND_NAMESPACE_MANAGEMENT = 0x0D,

        NVME_ADMIN_COMMAND_FIRMWARE_COMMIT = 0x10,
        NVME_ADMIN_COMMAND_FIRMWARE_IMAGE_DOWNLOAD = 0x11,
        NVME_ADMIN_COMMAND_NAMESPACE_ATTACHMENT = 0x15,

        NVME_ADMIN_COMMAND_FORMAT_NVM = 0x80,
        NVME_ADMIN_COMMAND_SECURITY_SEND = 0x81,
        NVME_ADMIN_COMMAND_SECURITY_RECEIVE = 0x82,
    };

    enum NVME_NVM_COMMANDS {
        NVME_NVM_COMMAND_FLUSH = 0x00,
        NVME_NVM_COMMAND_WRITE = 0x01,
        NVME_NVM_COMMAND_READ = 0x02,

        NVME_NVM_COMMAND_WRITE_UNCORRECTABLE = 0x04,
        NVME_NVM_COMMAND_COMPARE = 0x05,
        NVME_NVM_COMMAND_WRITE_ZEROES = 0x08,
        NVME_NVM_COMMAND_DATASET_MANAGEMENT = 0x09,
        NVME_NVM_COMMAND_RESERVATION_REGISTER = 0x0D,
        NVME_NVM_COMMAND_RESERVATION_REPORT = 0x0E,
        NVME_NVM_COMMAND_RESERVATION_ACQUIRE = 0x11,
        NVME_NVM_COMMAND_RESERVATION_RELEASE = 0x15,
    };

    enum NVME_IDENTIFY_CNS_CODES {
        NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE = 0,
        NVME_IDENTIFY_CNS_CONTROLLER = 1,
        NVME_IDENTIFY_CNS_ACTIVE_NAMESPACES = 2,
    };

    union NVME_CONTROLLER_CAPABILITIES {
        struct {
            // LSB

            uint64_t mqes : 16;  // RO - Maximum Queue Entries Supported (MQES)
            uint64_t cqr : 1;    // RO - Contiguous Queues Required (CQR)

            // Bit 17, 18 - AMS; RO - Arbitration Mechanism Supported (AMS)
            uint64_t ams_weighted_round_robin_with_urgent : 1;  // Bit 17: Weighted Round Robin with Urgent;
            uint64_t ams_vendor_specific : 1;                   // Bit 18: Vendor Specific.

            uint64_t reserved0 : 5;  // RO - bit 19 ~ 23
            uint64_t to : 8;         // RO - Timeout (TO)
            uint64_t dstrd : 4;      // RO - Doorbell Stride (DSTRD)
            uint64_t nssrs : 1;      // RO - NVM Subsystem Reset Supported (NSSRS)

            // Bit 37 ~ 44 - CSS; RO - Command Sets Supported (CSS)
            uint64_t css_nvm : 1;        // Bit 37: NVM command set
            uint64_t css_reserved0 : 1;  // Bit 38: Reserved
            uint64_t css_reserved1 : 1;  // Bit 39: Reserved
            uint64_t css_reserved2 : 1;  // Bit 40: Reserved
            uint64_t css_reserved3 : 1;  // Bit 41: Reserved
            uint64_t css_reserved4 : 1;  // Bit 42: Reserved
            uint64_t css_reserved5 : 1;  // Bit 43: Reserved
            uint64_t css_reserved6 : 1;  // Bit 44: Reserved

            uint64_t reserved2 : 3;  // RO - bit 45 ~ 47
            uint64_t mpsmin : 4;     // RO - Memory Page Size Minimum (MPSMIN)
            uint64_t mpsmax : 4;     // RO - Memory Page Size Maximum (MPSMAX)
            uint64_t reserved3 : 8;  // RO - bit 56 ~ 63

            // MSB
        } __attribute__((packed));

        uint64_t q_word;
    };

    union NVME_VERSION {
        struct {
            // LSB
            uint32_t reserved : 8;
            uint32_t mnr : 8;   // Minor Version Number (MNR)
            uint32_t mjr : 16;  // Major Version Number (MJR)
            // MSB
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CONTROLLER_CONFIGURATION {
        struct {
            // LSB
            uint32_t en : 1;         // RW - Enable (EN)
            uint32_t reserved0 : 3;  // RO
            uint32_t css : 3;        // RW - I/O  Command Set Selected (CSS)
            uint32_t mps : 4;        // RW - Memory Page Size (MPS)
            uint32_t ams : 3;        // RW - Arbitration Mechanism Selected (AMS)
            uint32_t shn : 2;        // RW - Shutdown Notification (SHN)
            uint32_t iosqes : 4;     // RW - I/O  Submission Queue Entry Size (IOSQES)
            uint32_t iocqes : 4;     // RW - I/O  Completion Queue Entry Size (IOCQES)
            uint32_t reserved1 : 8;  // RO
            // MSB
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CONTROLLER_STATUS {
        struct {
            uint32_t rdy : 1;    // RO - Ready (RDY)
            uint32_t cfs : 1;    // RO - Controller Fatal Status (CFS)
            uint32_t shst : 2;   // RO - Shutdown Status (SHST)
            uint32_t nssro : 1;  // RW1C - NVM Subsystem Reset Occurred (NSSRO)
            uint32_t pp : 1;     // RO - Processing Paused (PP)

            uint32_t reserved0 : 26;  // RO
        } __attribute__((packed));

        uint32_t d_word;
    };

    struct NVME_NVM_SUBSYSTEM_RESET {
        uint32_t nssrc;  // RW - NVM Subsystem Reset Control (NSSRC)
    };

    union NVME_ADMIN_QUEUE_ATTRIBUTES {
        struct {
            // LSB
            uint32_t asqs : 12;      // RW - Admin  Submission Queue Size (ASQS)
            uint32_t reserved0 : 4;  // RO
            uint32_t acqs : 12;      // RW - Admin  Completion Queue Size (ACQS)
            uint32_t reserved1 : 4;  // RO
            // MSB
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS {
        struct {
            // LSB
            uint64_t reserved0 : 12;  // RO
            uint64_t asqb : 52;       // RW - Admin Submission Queue Base (ASQB)
            // MSB
        } __attribute__((packed));

        uint64_t q_word;
    };

    union NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS {
        struct {
            // LSB
            uint64_t reserved0 : 12;  // RO
            uint64_t acqb : 52;       // RW - Admin Completion Queue Base (ACQB)
            // MSB
        } __attribute__((packed));

        uint64_t q_word;
    };

    union NVME_CONTROLLER_MEMORY_BUFFER_LOCATION {
        struct {
            // LSB
            uint32_t bir : 3;       // RO - Base Indicator Register (BIR)
            uint32_t reserved : 9;  // RO
            uint32_t ofst : 20;     // RO - Offset (OFST)
            // MSB
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CONTROLLER_MEMORY_BUFFER_SIZE {
        struct {
            // LSB
            uint32_t sqs : 1;       // RO - Submission Queue Support (SQS)
            uint32_t cqs : 1;       // RO - Completion Queue Support (CQS)
            uint32_t lists : 1;     // RO - PRP SGL List Support (LISTS)
            uint32_t rds : 1;       // RO - Read Data Support (RDS)
            uint32_t wds : 1;       // RO - Write Data Support (WDS)
            uint32_t reserved : 3;  // RO
            uint32_t szu : 4;       // RO - Size Units (SZU)
            uint32_t sz : 20;       // RO - Size (SZ)
            // MSB
        } __attribute__((packed));

        uint32_t d_word;
    };

    struct NVME_CONTROLLER_REGISTERS {
        NVME_CONTROLLER_CAPABILITIES cap;  // Controller Capabilities; 8 bytes
        NVME_VERSION vs;                   // Version
        uint32_t intms;                    // Interrupt Mask Set
        uint32_t intmc;                    // Interrupt Mask Clear
        NVME_CONTROLLER_CONFIGURATION cc;  // Controller Configuration
        uint32_t reserved0;
        NVME_CONTROLLER_STATUS csts;    // Controller Status
        NVME_NVM_SUBSYSTEM_RESET nssr;  // NVM Subsystem Reset (Optional)

        NVME_ADMIN_QUEUE_ATTRIBUTES aqa;               // Admin Queue Attributes
        NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS asq;  // Admin Submission Queue Base Address; 8 bytes
        NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS acq;  // Admin Completion Queue Base Address; 8 bytes

        NVME_CONTROLLER_MEMORY_BUFFER_LOCATION cmbloc;  // Controller Memory Buffer Location (Optional)
        NVME_CONTROLLER_MEMORY_BUFFER_SIZE cmbsz;       // Controller Memory Buffer Size (Optional)

        uint32_t reserved2[944];  // 40h ~ EFFh
        uint32_t reserved3[64];   // F00h ~ FFFh, Command Set Specific

        uint32_t doorbells[0];  // Start of the first Doorbell register. (Admin SQ Tail Doorbell)
    };

    enum DRIVER_STATUS { CONTROLLER_NOT_READY, CONTROLLER_ERROR, CONTROLLER_READY, CONTROLLER_SHUTDOWN };

    union NVME_COMMAND_DWORD0 {
        struct {
            uint32_t opc : 8;
            uint32_t fuse : 2;
            uint32_t reserved0 : 5;
            uint32_t psdt : 1;
            uint32_t cid : 16;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW10_IDENTIFY {
        struct {
            uint32_t cns : 8;
            uint32_t reserved : 8;
            uint32_t cntid : 16;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_IDENTIFY {
        struct {
            uint16_t nvmsetid;
            uint16_t reserved;
        };

        struct {
            uint32_t cnsid : 16;
            uint32_t reserved2 : 8;
            uint32_t csi : 8;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW10_ABORT {
        struct {
            uint32_t sqid : 8;
            uint32_t cid : 16;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW10_GET_FEATURES {
        struct {
            uint32_t fid : 8;
            uint32_t sel : 3;
            uint32_t reserved0 : 21;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW10_SET_FEATURES {
        struct {
            uint32_t fid : 8;
            uint32_t reserved0 : 23;
            uint32_t sv : 1;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_NUMBER_OF_QUEUES {
        struct {
            uint32_t nsq : 16;
            uint32_t ncq : 16;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_INTERRUPT_COALESCING {
        struct {
            uint32_t thr : 8;
            uint32_t time : 8;
            uint32_t reserved0 : 16;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_INTERRUPT_VECTOR_CONFIG {
        struct {
            uint32_t iv : 16;
            uint32_t cd : 1;
            uint32_t reserved0 : 15;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_LBA_RANGE_TYPE {
        struct {
            uint32_t num : 6;
            uint32_t reserved0 : 26;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_ARBITRATION {
        struct {
            uint32_t ab : 3;
            uint32_t reserved0 : 5;
            uint32_t lpw : 8;
            uint32_t mpw : 8;
            uint32_t hpw : 8;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_VOLATILE_WRITE_CACHE {
        struct {
            uint32_t wce : 1;
            uint32_t reserved0 : 31;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_ASYNC_EVENT_CONFIG {
        struct {
            uint32_t critical_warnings : 8;
            uint32_t ns_attribute_notices : 1;
            uint32_t fw_activation_notices : 1;
            uint32_t telemetry_log_notices : 1;
            uint32_t ana_change_notices : 1;
            uint32_t predictable_log_change_notices : 1;
            uint32_t lba_status_notices : 1;
            uint32_t endurance_event_notices : 1;
            uint32_t reserved0 : 12;
            uint32_t zone_descriptor_notices : 1;
            uint32_t reserved1 : 4;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_POWER_MANAGEMENT {
        struct {
            uint32_t ps : 5;
            uint32_t reserved0 : 27;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_AUTO_POWER_STATE_TRANSITION {
        struct {
            uint32_t apste : 1;
            uint32_t reserved0 : 31;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_TEMPERATURE_THRESHOLD {
        struct {
            uint32_t tmpth : 16;
            uint32_t tmpsel : 4;
            uint32_t thsel : 2;
            uint32_t reserved0 : 10;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            uint32_t ehm : 1;
            uint32_t mr : 1;
            uint32_t reserved : 30;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_WRITE_ATOMICITY_NORMAL {
        struct {
            uint32_t dn : 1;
            uint32_t reserved0 : 31;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURE_NON_OPERATIONAL_POWER_STATE {
        struct {
            uint32_t noppme : 1;
            uint32_t reserved0 : 31;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_FEATURES {
        NVME_CDW11_FEATURE_NUMBER_OF_QUEUES number_of_queues;
        NVME_CDW11_FEATURE_INTERRUPT_COALESCING interrupt_coalescing;
        NVME_CDW11_FEATURE_INTERRUPT_VECTOR_CONFIG interrupt_vector_config;
        NVME_CDW11_FEATURE_LBA_RANGE_TYPE lba_range_type;
        NVME_CDW11_FEATURE_ARBITRATION arbitration;
        NVME_CDW11_FEATURE_VOLATILE_WRITE_CACHE volatile_write_cache;
        NVME_CDW11_FEATURE_ASYNC_EVENT_CONFIG async_event_config;
        NVME_CDW11_FEATURE_POWER_MANAGEMENT power_management;
        NVME_CDW11_FEATURE_AUTO_POWER_STATE_TRANSITION auto_power_state_transition;
        NVME_CDW11_FEATURE_TEMPERATURE_THRESHOLD temperature_threshold;
        NVME_CDW11_FEATURE_HOST_MEMORY_BUFFER host_memory_buffer;
        NVME_CDW11_FEATURE_WRITE_ATOMICITY_NORMAL write_atomicity_normal;
        NVME_CDW11_FEATURE_NON_OPERATIONAL_POWER_STATE non_operational_power_state;
        uint32_t d_word;
    };

    union NVME_CDW12_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            uint32_t hsize;
        };

        uint32_t d_word;
    };

    union NVME_CDW12_FEATURES {
        NVME_CDW12_FEATURE_HOST_MEMORY_BUFFER host_memory_buffer;
        uint32_t d_word;
    };

    union NVME_CDW13_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            uint32_t reserved : 4;
            uint32_t hmdlla : 28;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW13_FEATURES {
        NVME_CDW13_FEATURE_HOST_MEMORY_BUFFER host_memory_buffer;
        uint32_t d_word;
    };

    union NVME_CDW14_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            uint32_t hmdlua;
        };

        uint32_t as_ulong;
    };

    union NVME_CDW14_FEATURES {
        NVME_CDW14_FEATURE_HOST_MEMORY_BUFFER host_memory_buffer;
        uint32_t d_word;
    };

    union NVME_CDW15_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            uint32_t hmdlec;
        };

        uint32_t as_ulong;
    };

    union NVME_CDW15_FEATURES {
        NVME_CDW15_FEATURE_HOST_MEMORY_BUFFER host_memory_buffer;
        uint32_t d_word;
    };

    union NVME_CDW10_GET_LOG_PAGE {
        struct {
            uint32_t lid : 8;
            uint32_t reserved0 : 8;
            uint32_t numd : 12;
            uint32_t reserved1 : 4;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW10_GET_LOG_PAGE_V13 {
        struct {
            uint32_t lid : 8;
            uint32_t lsp : 4;
            uint32_t reserved0 : 3;
            uint32_t rae : 1;
            uint32_t numdl : 16;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_GET_LOG_PAGE {
        struct {
            uint32_t numdu : 16;
            uint32_t log_specific_identifier : 16;
        } __attribute__((packed));

        uint32_t d_word;
    };

    struct NVME_CDW12_GET_LOG_PAGE {
        uint32_t lpol;
    };

    struct NVME_CDW13_GET_LOG_PAGE {
        uint32_t lpou;
    };

    struct NVME_CDW14_GET_LOG_PAGE {
        uint32_t bitfield;
    };

    union NVME_CDW10_CREATE_IO_QUEUE {
        struct {
            uint32_t qid : 16;
            uint32_t qsize : 16;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_CREATE_IO_SQ {
        struct {
            uint32_t pc : 1;
            uint32_t qprio : 2;
            uint32_t reserved0 : 13;
            uint32_t cqid : 16;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW11_CREATE_IO_CQ {
        struct {
            uint32_t pc : 1;   // Physically Contiguous (PC)
            uint32_t ien : 1;  // Interrupts Enabled (IEN)
            uint32_t reserved0 : 14;
            uint32_t iv : 16;  // Interrupt Vector (IV)
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW10_DELETE_IO_QUEUE {
        struct {
            uint32_t qid : 16;  // Queue Identifier
            uint32_t reserved0 : 16;
        } __attribute__((packed));
        uint32_t d_word;
    };

    union NVME_CONTEXT_ATTRIBUTES {
        struct {
            uint32_t access_frequency : 4;
            uint32_t access_latency : 2;
            uint32_t reserved0 : 2;
            uint32_t sequential_read_range : 1;
            uint32_t sequential_write_range : 1;
            uint32_t write_prepare : 1;
            uint32_t reserved1 : 13;
            uint32_t command_access_size : 8;
        } __attribute__((packed));

        uint32_t d_word;
    };

    struct NVME_LBA_RANGE {
        NVME_CONTEXT_ATTRIBUTES attributes;
        uint32_t logical_block_count;
        uint64_t starting_lba;
    };

    union NVME_CDW11_DATASET_MANAGEMENT {
        struct {
            uint32_t idr : 1;  // Integral Dataset for Read (IDR)
            uint32_t idw : 1;  // Integral Dataset for Write (IDW)
            uint32_t ad : 1;   // Deallocate (AD)
            uint32_t reserved : 29;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW10_DATASET_MANAGEMENT {
        struct {
            uint32_t nr : 8;  // Number of Ranges (NR)
            uint32_t reserved : 24;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW10_SECURITY_SEND_RECEIVE {
        struct {
            uint32_t reserved0 : 8;
            uint32_t spsp : 16;  // SP Specific (SPSP)
            uint32_t secp : 8;   // Security Protocol (SECP)
        } __attribute__((packed));

        uint32_t d_word;
    };

    struct NVME_CDW11_SECURITY_SEND {
        uint32_t tl;  // Transfer Length  (TL):
    };

    struct NVME_CDW11_SECURITY_RECEIVE {
        uint32_t al;  // Transfer Length  (AL)
    };

    union NVME_CDW10_FORMAT_NVM {
        struct {
            uint32_t lbaf : 4;  // LBA Format (LBAF)
            uint32_t ms : 1;    // Metadata Settings (MS)
            uint32_t pi : 3;    // Protection Information (PI)
            uint32_t pil : 1;   // Protection Information Location (PIL)
            uint32_t ses : 3;   // Secure Erase Settings (SES)

            uint32_t reserved : 20;
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW12_READ_WRITE {
        struct {
            uint32_t nlb : 16;  // Number of Logical Blocks (NLB)
            uint32_t reserved0 : 10;
            uint32_t prinfo : 4;  // Protection Information Field (PRINFO)
            uint32_t fua : 1;     // Force Unit Access (FUA)
            uint32_t lr : 1;      // Limited Retry (LR)
        } __attribute__((packed));

        uint32_t d_word;
    };

    union NVME_CDW13_READ_WRITE {
        struct {
            struct {
                uint8_t access_frequency : 4;
                uint8_t access_latency : 2;
                uint8_t sequential_request : 1;
                uint8_t incompressible : 1;
            } __attribute__((packed)) dsm;  // Dataset Management (DSM)

            uint8_t reserved0[3];
        };

        uint32_t d_word;
    };

    union NVME_CDW15_READ_WRITE {
        struct {
            uint32_t elbat : 16;   // Expected Logical Block Application Tag (ELBAT)
            uint32_t elbatm : 16;  // Expected Logical Block Application Tag Mask (ELBATM)
        } __attribute__((packed));

        uint32_t d_word;
    };

    struct NVME_COMMAND {
        NVME_COMMAND_DWORD0 cdw0;
        uint32_t nsid;
        uint32_t reserved0[2];
        uint64_t mptr;
        uint64_t prp1;
        uint64_t prp2;

        union {
            struct GENERAL {
                uint32_t cdw10;
                uint32_t cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } general;

            struct IDENTIFY {
                NVME_CDW10_IDENTIFY cdw10;
                NVME_CDW11_IDENTIFY cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } identify;

            struct ABORT {
                NVME_CDW10_ABORT cdw10;
                uint32_t cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } abort;

            struct GETFEATURES {
                NVME_CDW10_GET_FEATURES cdw10;
                NVME_CDW11_FEATURES cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } getfeatures;

            struct SETFEATURES {
                NVME_CDW10_SET_FEATURES cdw10;
                NVME_CDW11_FEATURES cdw11;
                NVME_CDW12_FEATURES cdw12;
                NVME_CDW13_FEATURES cdw13;
                NVME_CDW14_FEATURES cdw14;
                NVME_CDW15_FEATURES cdw15;
            } setfeatures;

            struct GETLOGPAGE {
                union {
                    NVME_CDW10_GET_LOG_PAGE cdw10;
                    NVME_CDW10_GET_LOG_PAGE_V13 cdw10_v13;
                };

                NVME_CDW11_GET_LOG_PAGE cdw11;
                NVME_CDW12_GET_LOG_PAGE cdw12;
                NVME_CDW13_GET_LOG_PAGE cdw13;
                NVME_CDW14_GET_LOG_PAGE cdw14;
                uint32_t cdw15;
            } getlogpage;

            struct CREATEIOCQ {
                NVME_CDW10_CREATE_IO_QUEUE cdw10;
                NVME_CDW11_CREATE_IO_CQ cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } createiocq;

            struct CREATEIOSQ {
                NVME_CDW10_CREATE_IO_QUEUE cdw10;
                NVME_CDW11_CREATE_IO_SQ cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } createiosq;

            struct DELETEIOQ {
                NVME_CDW10_DELETE_IO_QUEUE cdw10;
                uint32_t cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } deleteioq;

            struct DATASETMANAGEMENT {
                NVME_CDW10_DATASET_MANAGEMENT cdw10;
                NVME_CDW11_DATASET_MANAGEMENT cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } datasetmanagement;

            struct SECURITYSEND {
                NVME_CDW10_SECURITY_SEND_RECEIVE cdw10;
                NVME_CDW11_SECURITY_SEND cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } securitysend;

            struct SECURITYRECEIVE {
                NVME_CDW10_SECURITY_SEND_RECEIVE cdw10;
                NVME_CDW11_SECURITY_RECEIVE cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } securityreceive;

            struct FORMATNVM {
                NVME_CDW10_FORMAT_NVM cdw10;
                uint32_t cdw11;
                uint32_t cdw12;
                uint32_t cdw13;
                uint32_t cdw14;
                uint32_t cdw15;
            } formatnvm;

            struct READWRITE {
                uint32_t lbalow;
                uint32_t lbahigh;
                NVME_CDW12_READ_WRITE cdw12;
                NVME_CDW13_READ_WRITE cdw13;
                uint32_t cdw14;
                NVME_CDW15_READ_WRITE cdw15;
            } readwrite;
        } u;
    } __attribute__((packed));

    struct NVME_COMPLETION_ENTRY {
        uint32_t dw0;
        uint32_t reserved;

        union {
            struct {
                uint16_t sqhd;  // SQ Head Pointer (SQHD)
                uint16_t sqid;  // SQ Identifier (SQID)
            };

            uint32_t d_word;
        } dw2;

        union {
            struct {
                uint16_t cid : 16;  // Command Identifier (CID)
                uint16_t p : 1;     // Phase Tag (P)
                uint16_t status : 15;
            } __attribute__((packed));

            uint32_t d_word;
        } dw3;
    };

    struct NVME_POWER_STATE_DESC {
        uint16_t mp;
        uint8_t reserved0;
        uint8_t mps : 1;
        uint8_t nops : 1;
        uint8_t reserved1 : 6;
        uint32_t enlat;
        uint32_t exlat;
        uint8_t rrt : 5;
        uint8_t reserved2 : 3;
        uint8_t rrl : 5;
        uint8_t reserved3 : 3;
        uint8_t rwt : 5;
        uint8_t reserved4 : 3;
        uint8_t rwl : 5;
        uint8_t reserved5 : 3;
        uint16_t idlp;
        uint8_t reserved6 : 6;
        uint8_t ips : 2;
        uint8_t reserved7;
        uint16_t actp;
        uint8_t apw : 3;
        uint8_t reserved8 : 3;
        uint8_t aps : 2;
        uint8_t reserved9[9];
    };

    struct NVME_IDENTIFY_CONTROLLER_DATA {
        uint16_t vid;
        uint16_t ssvid;
        uint8_t sn[20];
        uint8_t mn[40];
        uint8_t fr[8];
        uint8_t rab;
        uint8_t ieee[3];

        struct __attribute__((packed)) {
            uint8_t multi_pc_ie_ports : 1;
            uint8_t multi_controllers : 1;
            uint8_t sriov : 1;
            uint8_t anar : 1;
            uint8_t reserved : 4;
        } cmic;

        uint8_t mdts;
        uint16_t cntlid;
        uint32_t ver;
        uint32_t rtd3_r;
        uint32_t rtd3_e;

        struct __attribute__((packed)) {
            uint32_t reserved0 : 8;
            uint32_t namespace_attribute_changed : 1;
            uint32_t firmware_activation : 1;
            uint32_t reserved1 : 1;
            uint32_t asymmetric_access_changed : 1;
            uint32_t predictable_latency_aggregate_log_changed : 1;
            uint32_t lba_status_changed : 1;
            uint32_t endurance_group_aggregate_log_changed : 1;
            uint32_t reserved2 : 12;
            uint32_t zone_information : 1;
            uint32_t reserved3 : 4;
        } oaes;

        struct __attribute__((packed)) {
            uint32_t host_identifier128_bit : 1;
            uint32_t nopsp_mode : 1;
            uint32_t nvm_sets : 1;
            uint32_t read_recovery_levels : 1;
            uint32_t endurance_groups : 1;
            uint32_t predictable_latency_mode : 1;
            uint32_t tbkas : 1;
            uint32_t namespace_granularity : 1;
            uint32_t sq_associations : 1;
            uint32_t uuid_list : 1;
            uint32_t reserved0 : 22;
        } ctratt;

        struct __attribute__((packed)) {
            uint16_t read_recovery_level0 : 1;
            uint16_t read_recovery_level1 : 1;
            uint16_t read_recovery_level2 : 1;
            uint16_t read_recovery_level3 : 1;
            uint16_t read_recovery_level4 : 1;
            uint16_t read_recovery_level5 : 1;
            uint16_t read_recovery_level6 : 1;
            uint16_t read_recovery_level7 : 1;
            uint16_t read_recovery_level8 : 1;
            uint16_t read_recovery_level9 : 1;
            uint16_t read_recovery_level10 : 1;
            uint16_t read_recovery_level11 : 1;
            uint16_t read_recovery_level12 : 1;
            uint16_t read_recovery_level13 : 1;
            uint16_t read_recovery_level14 : 1;
            uint16_t read_recovery_level15 : 1;
        } rrls;

        uint8_t reserved0[9];
        uint8_t cntrltype;
        uint8_t fguid[16];
        uint16_t crdt1;
        uint16_t crdt2;
        uint16_t crdt3;
        uint8_t reserved0_1[106];
        uint8_t reserved_for_management[16];

        struct __attribute__((packed)) {
            uint16_t security_commands : 1;
            uint16_t format_nvm : 1;
            uint16_t firmware_commands : 1;
            uint16_t namespace_commands : 1;
            uint16_t device_self_test : 1;
            uint16_t directives : 1;
            uint16_t nv_me_mi_commands : 1;
            uint16_t virtualization_mgmt : 1;
            uint16_t door_bell_buffer_config : 1;
            uint16_t get_lba_status : 1;
            uint16_t reserved : 6;
        } oacs;

        uint8_t acl;
        uint8_t aerl;

        struct __attribute__((packed)) {
            uint8_t slot1_read_only : 1;
            uint8_t slot_count : 3;
            uint8_t activation_without_reset : 1;
            uint8_t reserved : 3;
        } frmw;

        struct __attribute__((packed)) {
            uint8_t smart_page_per_namespace : 1;
            uint8_t command_effects_log : 1;
            uint8_t log_page_extended_data : 1;
            uint8_t telemetry_support : 1;
            uint8_t persistent_event_log : 1;
            uint8_t reserved0 : 1;
            uint8_t telemetry_data_area4 : 1;
            uint8_t reserved1 : 1;
        } lpa;

        uint8_t elpe;
        uint8_t npss;

        struct __attribute__((packed)) {
            uint8_t command_format_in_spec : 1;
            uint8_t reserved : 7;
        } avscc;

        struct __attribute__((packed)) {
            uint8_t supported : 1;
            uint8_t reserved : 7;
        } apsta;

        uint16_t wctemp;
        uint16_t cctemp;
        uint16_t mtfa;
        uint32_t hmpre;
        uint32_t hmmin;

        uint8_t tnvmcap[16];
        uint8_t unvmcap[16];

        struct __attribute__((packed)) {
            uint32_t rpmb_unit_count : 3;
            uint32_t authentication_method : 3;
            uint32_t reserved0 : 10;
            uint32_t total_size : 8;
            uint32_t access_size : 8;
        } rpmbs;

        uint16_t edstt;
        uint8_t dsto;
        uint8_t fwug;
        uint16_t kas;

        struct __attribute__((packed)) {
            uint16_t supported : 1;
            uint16_t reserved : 15;
        } hctma;

        uint16_t mntmt;
        uint16_t mxtmt;

        struct __attribute__((packed)) {
            uint32_t crypto_erase : 1;
            uint32_t block_erase : 1;
            uint32_t overwrite : 1;
            uint32_t reserved : 26;
            uint32_t ndi : 1;
            uint32_t nodmmas : 2;
        } sanicap;

        uint32_t hmminds;
        uint16_t hmmaxd;
        uint16_t nsetidmax;
        uint16_t endgidmax;

        uint8_t anatt;

        struct __attribute__((packed)) {
            uint8_t optimized_state : 1;
            uint8_t non_optimized_state : 1;
            uint8_t inaccessible_state : 1;
            uint8_t persistent_loss_state : 1;
            uint8_t change_state : 1;
            uint8_t reserved : 1;
            uint8_t static_anagrpid : 1;
            uint8_t support_non_zero_anagrpid : 1;
        } anacap;

        uint32_t anagrpmax;
        uint32_t nanagrpid;
        uint32_t pels;

        uint8_t reserved1[156];

        struct __attribute__((packed)) {
            uint8_t required_entry_size : 4;
            uint8_t max_entry_size : 4;
        } sqes;

        struct __attribute__((packed)) {
            uint8_t required_entry_size : 4;
            uint8_t max_entry_size : 4;
        } cqes;

        uint16_t maxcmd;
        uint32_t nn;

        struct __attribute__((packed)) {
            uint16_t compare : 1;
            uint16_t write_uncorrectable : 1;
            uint16_t dataset_management : 1;
            uint16_t write_zeroes : 1;
            uint16_t feature_field : 1;
            uint16_t reservations : 1;
            uint16_t timestamp : 1;
            uint16_t verify : 1;
            uint16_t reserved : 8;
        } oncs;

        struct __attribute__((packed)) {
            uint16_t compare_and_write : 1;
            uint16_t reserved : 15;
        } fuses;

        struct __attribute__((packed)) {
            uint8_t format_apply_to_all : 1;
            uint8_t secure_erase_apply_to_all : 1;
            uint8_t cryptographic_erase_supported : 1;
            uint8_t format_support_nsid_all_f : 1;
            uint8_t reserved : 4;
        } fna;

        struct __attribute__((packed)) {
            uint8_t present : 1;
            uint8_t flush_behavior : 2;
            uint8_t reserved : 5;
        } vwc;

        uint16_t awun;
        uint16_t awupf;

        struct __attribute__((packed)) {
            uint8_t command_format_in_spec : 1;
            uint8_t reserved : 7;
        } nvscc;

        struct __attribute__((packed)) {
            uint8_t write_protect : 1;
            uint8_t until_power_cycle : 1;
            uint8_t permanent : 1;
            uint8_t reserved : 5;
        } nwpc;

        uint16_t acwu;
        uint8_t reserved4[2];

        struct __attribute__((packed)) {
            uint32_t sgl_supported : 2;
            uint32_t keyed_sgl_data : 1;
            uint32_t reserved0 : 13;
            uint32_t bit_bucket_descr_supported : 1;
            uint32_t byte_aligned_contiguous_physical_buffer : 1;
            uint32_t sgl_length_larger_than_data_length : 1;
            uint32_t mptrsgl_descriptor : 1;
            uint32_t address_field_sgl_data_block : 1;
            uint32_t transport_sgl_data : 1;
            uint32_t reserved1 : 10;
        } sgls;

        uint32_t mnan;
        uint8_t reserved6[224];
        uint8_t subnqn[256];
        uint8_t reserved7[768];
        uint8_t reserved8[256];

        NVME_POWER_STATE_DESC pds[32];
        uint8_t vs[1024];
    };

    union NVME_LBA_FORMAT {
        struct {
            uint16_t ms;
            uint8_t lbads;
            uint8_t
            rp : 2;
            uint8_t reserved0 : 6;
        };

        uint32_t d_word;
    };

    union NVM_RESERVATION_CAPABILITIES {
        struct {
            uint8_t ptpls : 1;
            uint8_t wes : 1;
            uint8_t eas : 1;
            uint8_t weros : 1;
            uint8_t earos : 1;
            uint8_t wears : 1;
            uint8_t eaars : 1;
            uint8_t ieks : 1;
        };

        uint8_t byte;
    };

    struct NVME_IDENTIFY_NAMESPACE_DATA {
        uint64_t nsze;
        uint64_t ncap;
        uint64_t nuse;

        struct {
            uint8_t thin_provisioning : 1;
            uint8_t name_space_atomic_write_unit : 1;
            uint8_t deallocated_or_unwritten_error : 1;
            uint8_t skip_reuse_ui : 1;
            uint8_t name_space_io_optimization : 1;
            uint8_t reserved : 3;
        } nsfeat;

        uint8_t nlbaf;

        struct {
            uint8_t lba_format_index : 4;
            uint8_t metadata_in_extended_data_lba : 1;
            uint8_t reserved : 3;
        } flbas;

        struct {
            uint8_t metadata_in_extended_data_lba : 1;
            uint8_t metadata_in_separate_buffer : 1;
            uint8_t reserved : 6;
        } mc;

        struct {
            uint8_t protection_info_type1 : 1;
            uint8_t protection_info_type2 : 1;
            uint8_t protection_info_type3 : 1;
            uint8_t info_at_beginning_of_metadata : 1;
            uint8_t info_at_end_of_metadata : 1;
            uint8_t reserved : 3;
        } dpc;

        struct {
            uint8_t protection_info_type_enabled : 3;
            uint8_t info_at_beginning_of_metadata : 1;
            uint8_t reserved : 4;
        } dps;

        struct {
            uint8_t shared_name_space : 1;
            uint8_t reserved : 7;
        } nmic;

        NVM_RESERVATION_CAPABILITIES rescap;

        struct {
            uint8_t percentage_remained : 7;
            uint8_t supported : 1;
        } fpi;

        struct {
            uint8_t read_behavior : 3;
            uint8_t write_zeroes : 1;
            uint8_t guard_field_with_crc : 1;
            uint8_t reserved : 3;
        } dlfeat;

        uint16_t nawun;
        uint16_t nawupf;
        uint16_t nacwu;
        uint16_t nabsn;
        uint16_t nabo;
        uint16_t nabspf;
        uint16_t noiob;

        uint8_t nvmcap[16];

        uint16_t npwg;
        uint16_t npwa;
        uint16_t npdg;
        uint16_t npda;
        uint16_t nows;

        uint16_t mssrl;
        uint32_t mcl;
        uint8_t msrc;

        uint8_t reserved2[11];

        uint32_t anagrpid;

        uint8_t reserved3[3];

        struct {
            uint8_t write_protected : 1;
            uint8_t reserved : 7;
        } nsattr;

        uint16_t nvmsetid;
        uint16_t endgid;

        uint8_t nguid[16];
        uint8_t eui64[8];

        NVME_LBA_FORMAT lbaf[16];

        uint8_t reserved4[192];
        uint8_t vs[3712];
    };
}  // namespace nvme

#endif  // NVME_DEFS_H
