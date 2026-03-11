//
// Created by Linus on 11.07.25.
//

#ifndef NVME_DEFS_H
#define NVME_DEFS_H

#include <vespera/types.h>

// For reference see: https://nvmexpress.org/specifications/ "NVM Express® Base Specification"

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

            u64 mqes : 16;  // RO - Maximum Queue Entries Supported (MQES)
            u64 cqr : 1;    // RO - Contiguous Queues Required (CQR)

            // Bit 17, 18 - AMS; RO - Arbitration Mechanism Supported (AMS)
            u64 ams_weighted_round_robin_with_urgent : 1;  // Bit 17: Weighted Round Robin with Urgent;
            u64 ams_vendor_specific : 1;                   // Bit 18: Vendor Specific.

            u64 reserved0 : 5;  // RO - bit 19 ~ 23
            u64 to : 8;         // RO - Timeout (TO)
            u64 dstrd : 4;      // RO - Doorbell Stride (DSTRD)
            u64 nssrs : 1;      // RO - NVM Subsystem Reset Supported (NSSRS)

            // Bit 37 ~ 44 - CSS; RO - Command Sets Supported (CSS)
            u64 css_nvm : 1;        // Bit 37: NVM command set
            u64 css_reserved0 : 1;  // Bit 38: Reserved
            u64 css_reserved1 : 1;  // Bit 39: Reserved
            u64 css_reserved2 : 1;  // Bit 40: Reserved
            u64 css_reserved3 : 1;  // Bit 41: Reserved
            u64 css_reserved4 : 1;  // Bit 42: Reserved
            u64 css_reserved5 : 1;  // Bit 43: Reserved
            u64 css_reserved6 : 1;  // Bit 44: Reserved

            u64 reserved2 : 3;  // RO - bit 45 ~ 47
            u64 mpsmin : 4;     // RO - Memory Page Size Minimum (MPSMIN)
            u64 mpsmax : 4;     // RO - Memory Page Size Maximum (MPSMAX)
            u64 reserved3 : 8;  // RO - bit 56 ~ 63

            // MSB
        } __attribute__((packed));

        u64 q_word;
    };

    union NVME_VERSION {
        struct {
            // LSB
            u32 reserved : 8;
            u32 mnr : 8;   // Minor Version Number (MNR)
            u32 mjr : 16;  // Major Version Number (MJR)
            // MSB
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CONTROLLER_CONFIGURATION {
        struct {
            // LSB
            u32 en : 1;         // RW - Enable (EN)
            u32 reserved0 : 3;  // RO
            u32 css : 3;        // RW - I/O  Command Set Selected (CSS)
            u32 mps : 4;        // RW - Memory Page Size (MPS)
            u32 ams : 3;        // RW - Arbitration Mechanism Selected (AMS)
            u32 shn : 2;        // RW - Shutdown Notification (SHN)
            u32 iosqes : 4;     // RW - I/O  Submission Queue Entry Size (IOSQES)
            u32 iocqes : 4;     // RW - I/O  Completion Queue Entry Size (IOCQES)
            u32 reserved1 : 8;  // RO
            // MSB
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CONTROLLER_STATUS {
        struct {
            u32 rdy : 1;    // RO - Ready (RDY)
            u32 cfs : 1;    // RO - Controller Fatal Status (CFS)
            u32 shst : 2;   // RO - Shutdown Status (SHST)
            u32 nssro : 1;  // RW1C - NVM Subsystem Reset Occurred (NSSRO)
            u32 pp : 1;     // RO - Processing Paused (PP)

            u32 reserved0 : 26;  // RO
        } __attribute__((packed));

        u32 d_word;
    };

    struct NVME_NVM_SUBSYSTEM_RESET {
        u32 nssrc;  // RW - NVM Subsystem Reset Control (NSSRC)
    };

    union NVME_ADMIN_QUEUE_ATTRIBUTES {
        struct {
            // LSB
            u32 asqs : 12;      // RW - Admin  Submission Queue Size (ASQS)
            u32 reserved0 : 4;  // RO
            u32 acqs : 12;      // RW - Admin  Completion Queue Size (ACQS)
            u32 reserved1 : 4;  // RO
            // MSB
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS {
        struct {
            // LSB
            u64 reserved0 : 12;  // RO
            u64 asqb : 52;       // RW - Admin Submission Queue Base (ASQB)
            // MSB
        } __attribute__((packed));

        u64 q_word;
    };

    union NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS {
        struct {
            // LSB
            u64 reserved0 : 12;  // RO
            u64 acqb : 52;       // RW - Admin Completion Queue Base (ACQB)
            // MSB
        } __attribute__((packed));

        u64 q_word;
    };

    union NVME_CONTROLLER_MEMORY_BUFFER_LOCATION {
        struct {
            // LSB
            u32 bir : 3;       // RO - Base Indicator Register (BIR)
            u32 reserved : 9;  // RO
            u32 ofst : 20;     // RO - Offset (OFST)
            // MSB
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CONTROLLER_MEMORY_BUFFER_SIZE {
        struct {
            // LSB
            u32 sqs : 1;       // RO - Submission Queue Support (SQS)
            u32 cqs : 1;       // RO - Completion Queue Support (CQS)
            u32 lists : 1;     // RO - PRP SGL List Support (LISTS)
            u32 rds : 1;       // RO - Read Data Support (RDS)
            u32 wds : 1;       // RO - Write Data Support (WDS)
            u32 reserved : 3;  // RO
            u32 szu : 4;       // RO - Size Units (SZU)
            u32 sz : 20;       // RO - Size (SZ)
            // MSB
        } __attribute__((packed));

        u32 d_word;
    };

    struct NVME_CONTROLLER_REGISTERS {
        NVME_CONTROLLER_CAPABILITIES cap;  // Controller Capabilities; 8 bytes
        NVME_VERSION vs;                   // Version
        u32 intms;                         // Interrupt Mask Set
        u32 intmc;                         // Interrupt Mask Clear
        NVME_CONTROLLER_CONFIGURATION cc;  // Controller Configuration
        u32 reserved0;
        NVME_CONTROLLER_STATUS csts;    // Controller Status
        NVME_NVM_SUBSYSTEM_RESET nssr;  // NVM Subsystem Reset (Optional)

        NVME_ADMIN_QUEUE_ATTRIBUTES aqa;               // Admin Queue Attributes
        NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS asq;  // Admin Submission Queue Base Address; 8 bytes
        NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS acq;  // Admin Completion Queue Base Address; 8 bytes

        NVME_CONTROLLER_MEMORY_BUFFER_LOCATION cmbloc;  // Controller Memory Buffer Location (Optional)
        NVME_CONTROLLER_MEMORY_BUFFER_SIZE cmbsz;       // Controller Memory Buffer Size (Optional)

        u32 reserved2[944];  // 40h ~ EFFh
        u32 reserved3[64];   // F00h ~ FFFh, Command Set Specific

        u32 doorbells[0];  // Start of the first Doorbell register. (Admin SQ Tail Doorbell)
    };

    struct NVME_HEALTH_INFO_LOG {
        union {
            struct {
                u8 available_space_low : 1;  // =1: Available spare capacity has fallen below the threshold.
                u8 temperature_threshold
                    : 1;  // =1: Device temperature is above the over-temp threshold or below the under-temp threshold.
                u8 reliability_degraded
                    : 1;           // =1: Device reliability degraded due to media errors or internal failures.
                u8 read_only : 1;  // =1: Media has been placed into read-only mode.
                u8 volatile_memory_backup_device_failed
                    : 1;          // =1: Volatile memory backup device failed (only valid if controller supports it).
                u8 reserved : 3;  // Reserved bits.
            };
            u8 raw;          // Raw critical warning byte.
        } critical_warning;  // Bitfield representing controller critical warning states.

        u8 temperature[2];   // Composite device temperature in Kelvin (controller + NVM). May trigger async event if
                             // thresholds are exceeded.
        u8 available_spare;  // Normalized percentage (0–100%) of remaining spare capacity.
        u8 available_spare_threshold;  // Threshold (0–100%). Async event may occur when available_spare drops below
                                       // this value.
        u8 percentage_used;            // Estimated percentage of NVM subsystem life used.
        u8 reserved0[26];              // Reserved.
        u128 data_unit_read;  // Number of 512-byte data units read by host (excluding metadata). Value is reported in
                              // thousands (1 = 1000 units of 512 bytes) and rounded up.
        u128 data_unit_written;   // Number of 512-byte data units written by host (excluding metadata). Value reported
                                  // in thousands; controller converts non-512 LBA sizes accordingly.
        u128 host_read_commands;  // Total number of read commands completed by controller (Compare + Read).
        u128 host_written_commands;       // Total number of write commands completed by controller.
        u128 controller_busy_time;        // Time controller was busy processing I/O commands.
                                          // Unit: minutes.
        u128 power_cycle;                 // Number of controller power cycles.
        u128 power_on_hours;              // Total number of hours the controller has been powered on
                                          // (excluding time spent in low power states).
        u128 unsafe_shutdowns;            // Number of unsafe shutdowns (power lost without shutdown notification).
        u128 media_errors;                // Count of unrecoverable data integrity errors
                                          // (e.g. uncorrectable ECC, CRC failure, LBA tag mismatch).
        u128 error_info_log_entry_count;  // Total number of Error Information log entries recorded by the controller.
        u32 warning_composite_temperature_time;   // Time in minutes device temperature was >= WCTEMP and < CCTEMP.
        u32 critical_composite_temperature_time;  // Time in minutes device temperature exceeded CCTEMP.
        u8 temperature_sensor1[2];                // Temperature reported by sensor 1 (Kelvin).
        u8 temperature_sensor2[2];                // Temperature reported by sensor 2 (Kelvin).
        u8 temperature_sensor3[2];                // Temperature reported by sensor 3 (Kelvin).
        u8 temperature_sensor4[2];                // Temperature reported by sensor 4 (Kelvin).
        u8 temperature_sensor5[2];                // Temperature reported by sensor 5 (Kelvin).
        u8 temperature_sensor6[2];                // Temperature reported by sensor 6 (Kelvin).
        u8 temperature_sensor7[2];                // Temperature reported by sensor 7 (Kelvin).
        u8 temperature_sensor8[2];                // Temperature reported by sensor 8 (Kelvin).

        u8 reserved1[296];  // Reserved (for future NVMe spec extensions).
    };

    enum DRIVER_STATUS { CONTROLLER_NOT_READY, CONTROLLER_ERROR, CONTROLLER_READY, CONTROLLER_SHUTDOWN };

    union NVME_COMMAND_DWORD0 {
        struct {
            u32 opc : 8;
            u32 fuse : 2;
            u32 reserved0 : 5;
            u32 psdt : 1;
            u32 cid : 16;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW10_IDENTIFY {
        struct {
            u32 cns : 8;
            u32 reserved : 8;
            u32 cntid : 16;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_IDENTIFY {
        struct {
            u16 nvmsetid;
            u16 reserved;
        };

        struct {
            u32 cnsid : 16;
            u32 reserved2 : 8;
            u32 csi : 8;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW10_ABORT {
        struct {
            u32 sqid : 8;
            u32 cid : 16;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW10_GET_FEATURES {
        struct {
            u32 fid : 8;
            u32 sel : 3;
            u32 reserved0 : 21;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW10_SET_FEATURES {
        struct {
            u32 fid : 8;
            u32 reserved0 : 23;
            u32 sv : 1;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_NUMBER_OF_QUEUES {
        struct {
            u32 nsq : 16;
            u32 ncq : 16;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_INTERRUPT_COALESCING {
        struct {
            u32 thr : 8;
            u32 time : 8;
            u32 reserved0 : 16;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_INTERRUPT_VECTOR_CONFIG {
        struct {
            u32 iv : 16;
            u32 cd : 1;
            u32 reserved0 : 15;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_LBA_RANGE_TYPE {
        struct {
            u32 num : 6;
            u32 reserved0 : 26;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_ARBITRATION {
        struct {
            u32 ab : 3;
            u32 reserved0 : 5;
            u32 lpw : 8;
            u32 mpw : 8;
            u32 hpw : 8;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_VOLATILE_WRITE_CACHE {
        struct {
            u32 wce : 1;
            u32 reserved0 : 31;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_ASYNC_EVENT_CONFIG {
        struct {
            u32 critical_warnings : 8;
            u32 ns_attribute_notices : 1;
            u32 fw_activation_notices : 1;
            u32 telemetry_log_notices : 1;
            u32 ana_change_notices : 1;
            u32 predictable_log_change_notices : 1;
            u32 lba_status_notices : 1;
            u32 endurance_event_notices : 1;
            u32 reserved0 : 12;
            u32 zone_descriptor_notices : 1;
            u32 reserved1 : 4;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_POWER_MANAGEMENT {
        struct {
            u32 ps : 5;
            u32 reserved0 : 27;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_AUTO_POWER_STATE_TRANSITION {
        struct {
            u32 apste : 1;
            u32 reserved0 : 31;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_TEMPERATURE_THRESHOLD {
        struct {
            u32 tmpth : 16;
            u32 tmpsel : 4;
            u32 thsel : 2;
            u32 reserved0 : 10;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            u32 ehm : 1;
            u32 mr : 1;
            u32 reserved : 30;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_WRITE_ATOMICITY_NORMAL {
        struct {
            u32 dn : 1;
            u32 reserved0 : 31;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_FEATURE_NON_OPERATIONAL_POWER_STATE {
        struct {
            u32 noppme : 1;
            u32 reserved0 : 31;
        } __attribute__((packed));

        u32 d_word;
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
        u32 d_word;
    };

    union NVME_CDW12_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            u32 hsize;
        };

        u32 d_word;
    };

    union NVME_CDW12_FEATURES {
        NVME_CDW12_FEATURE_HOST_MEMORY_BUFFER host_memory_buffer;
        u32 d_word;
    };

    union NVME_CDW13_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            u32 reserved : 4;
            u32 hmdlla : 28;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW13_FEATURES {
        NVME_CDW13_FEATURE_HOST_MEMORY_BUFFER host_memory_buffer;
        u32 d_word;
    };

    union NVME_CDW14_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            u32 hmdlua;
        };

        u32 as_ulong;
    };

    union NVME_CDW14_FEATURES {
        NVME_CDW14_FEATURE_HOST_MEMORY_BUFFER host_memory_buffer;
        u32 d_word;
    };

    union NVME_CDW15_FEATURE_HOST_MEMORY_BUFFER {
        struct {
            u32 hmdlec;
        };

        u32 as_ulong;
    };

    union NVME_CDW15_FEATURES {
        NVME_CDW15_FEATURE_HOST_MEMORY_BUFFER host_memory_buffer;
        u32 d_word;
    };

    union NVME_CDW10_GET_LOG_PAGE {
        struct {
            u32 lid : 8;
            u32 reserved0 : 8;
            u32 numd : 12;
            u32 reserved1 : 4;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW10_GET_LOG_PAGE_V13 {
        struct {
            u32 lid : 8;
            u32 lsp : 4;
            u32 reserved0 : 3;
            u32 rae : 1;
            u32 numdl : 16;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_GET_LOG_PAGE {
        struct {
            u32 numdu : 16;
            u32 log_specific_identifier : 16;
        } __attribute__((packed));

        u32 d_word;
    };

    struct NVME_CDW12_GET_LOG_PAGE {
        u32 lpol;
    };

    struct NVME_CDW13_GET_LOG_PAGE {
        u32 lpou;
    };

    struct NVME_CDW14_GET_LOG_PAGE {
        u32 bitfield;
    };

    union NVME_CDW10_CREATE_IO_QUEUE {
        struct {
            u32 qid : 16;
            u32 qsize : 16;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_CREATE_IO_SQ {
        struct {
            u32 pc : 1;
            u32 qprio : 2;
            u32 reserved0 : 13;
            u32 cqid : 16;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW11_CREATE_IO_CQ {
        struct {
            u32 pc : 1;   // Physically Contiguous (PC)
            u32 ien : 1;  // Interrupts Enabled (IEN)
            u32 reserved0 : 14;
            u32 iv : 16;  // Interrupt Vector (IV)
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW10_DELETE_IO_QUEUE {
        struct {
            u32 qid : 16;  // Queue Identifier
            u32 reserved0 : 16;
        } __attribute__((packed));
        u32 d_word;
    };

    union NVME_CONTEXT_ATTRIBUTES {
        struct {
            u32 access_frequency : 4;
            u32 access_latency : 2;
            u32 reserved0 : 2;
            u32 sequential_read_range : 1;
            u32 sequential_write_range : 1;
            u32 write_prepare : 1;
            u32 reserved1 : 13;
            u32 command_access_size : 8;
        } __attribute__((packed));

        u32 d_word;
    };

    struct NVME_LBA_RANGE {
        NVME_CONTEXT_ATTRIBUTES attributes;  // The use of this information is optional and the controller is not
                                             // required to perform any specific action.
        u32 logical_block_count;
        u64 starting_lba;
    };

    union NVME_CDW11_DATASET_MANAGEMENT {
        struct {
            u32 idr : 1;  // Integral Dataset for Read (IDR)
            u32 idw : 1;  // Integral Dataset for Write (IDW)
            u32 ad : 1;   // Deallocate (AD)
            u32 reserved : 29;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW10_DATASET_MANAGEMENT {
        struct {
            u32 nr : 8;  // Number of Ranges (NR)
            u32 reserved : 24;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW10_SECURITY_SEND_RECEIVE {
        struct {
            u32 reserved0 : 8;
            u32 spsp : 16;  // SP Specific (SPSP)
            u32 secp : 8;   // Security Protocol (SECP)
        } __attribute__((packed));

        u32 d_word;
    };

    struct NVME_CDW11_SECURITY_SEND {
        u32 tl;  // Transfer Length  (TL):
    };

    struct NVME_CDW11_SECURITY_RECEIVE {
        u32 al;  // Transfer Length  (AL)
    };

    union NVME_CDW10_FORMAT_NVM {
        struct {
            u32 lbaf : 4;  // LBA Format (LBAF)
            u32 ms : 1;    // Metadata Settings (MS)
            u32 pi : 3;    // Protection Information (PI)
            u32 pil : 1;   // Protection Information Location (PIL)
            u32 ses : 3;   // Secure Erase Settings (SES)

            u32 reserved : 20;
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW12_READ_WRITE {
        struct {
            u32 nlb : 16;  // Number of Logical Blocks (NLB)
            u32 reserved0 : 10;
            u32 prinfo : 4;  // Protection Information Field (PRINFO)
            u32 fua : 1;     // Force Unit Access (FUA)
            u32 lr : 1;      // Limited Retry (LR)
        } __attribute__((packed));

        u32 d_word;
    };

    union NVME_CDW13_READ_WRITE {
        struct {
            struct {
                u8 access_frequency : 4;
                u8 access_latency : 2;
                u8 sequential_request : 1;
                u8 incompressible : 1;
            } __attribute__((packed)) dsm;  // Dataset Management (DSM)

            u8 reserved0[3];
        };

        u32 d_word;
    };

    union NVME_CDW15_READ_WRITE {
        struct {
            u32 elbat : 16;   // Expected Logical Block Application Tag (ELBAT)
            u32 elbatm : 16;  // Expected Logical Block Application Tag Mask (ELBATM)
        } __attribute__((packed));

        u32 d_word;
    };

    struct NVME_COMMAND {
        NVME_COMMAND_DWORD0 cdw0;
        u32 nsid;
        u32 reserved0[2];
        u64 mptr;
        u64 prp1;
        u64 prp2;

        union {
            struct GENERAL {
                u32 cdw10;
                u32 cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } general;

            struct IDENTIFY {
                NVME_CDW10_IDENTIFY cdw10;
                NVME_CDW11_IDENTIFY cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } identify;

            struct ABORT {
                NVME_CDW10_ABORT cdw10;
                u32 cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } abort;

            struct GETFEATURES {
                NVME_CDW10_GET_FEATURES cdw10;
                NVME_CDW11_FEATURES cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
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
                u32 cdw15;
            } getlogpage;

            struct CREATEIOCQ {
                NVME_CDW10_CREATE_IO_QUEUE cdw10;
                NVME_CDW11_CREATE_IO_CQ cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } createiocq;

            struct CREATEIOSQ {
                NVME_CDW10_CREATE_IO_QUEUE cdw10;
                NVME_CDW11_CREATE_IO_SQ cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } createiosq;

            struct DELETEIOQ {
                NVME_CDW10_DELETE_IO_QUEUE cdw10;
                u32 cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } deleteioq;

            struct DATASETMANAGEMENT {
                NVME_CDW10_DATASET_MANAGEMENT cdw10;
                NVME_CDW11_DATASET_MANAGEMENT cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } datasetmanagement;

            struct SECURITYSEND {
                NVME_CDW10_SECURITY_SEND_RECEIVE cdw10;
                NVME_CDW11_SECURITY_SEND cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } securitysend;

            struct SECURITYRECEIVE {
                NVME_CDW10_SECURITY_SEND_RECEIVE cdw10;
                NVME_CDW11_SECURITY_RECEIVE cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } securityreceive;

            struct FORMATNVM {
                NVME_CDW10_FORMAT_NVM cdw10;
                u32 cdw11;
                u32 cdw12;
                u32 cdw13;
                u32 cdw14;
                u32 cdw15;
            } formatnvm;

            struct READWRITE {
                u32 lbalow;
                u32 lbahigh;
                NVME_CDW12_READ_WRITE cdw12;
                NVME_CDW13_READ_WRITE cdw13;
                u32 cdw14;
                NVME_CDW15_READ_WRITE cdw15;
            } readwrite;
        } u;
    } __attribute__((packed));

    struct NVME_COMPLETION_ENTRY {
        u32 dw0;
        u32 reserved;

        union {
            struct {
                u16 sqhd;  // SQ Head Pointer (SQHD)
                u16 sqid;  // SQ Identifier (SQID)
            };

            u32 d_word;
        } dw2;

        union {
            struct {
                u16 cid : 16;  // Command Identifier (CID)
                u16 p : 1;     // Phase Tag (P)
                u16 status : 15;
            } __attribute__((packed));

            u32 d_word;
        } dw3;
    };

    struct NVME_POWER_STATE_DESC {
        u16 mp;
        u8 reserved0;
        u8 mps : 1;
        u8 nops : 1;
        u8 reserved1 : 6;
        u32 enlat;
        u32 exlat;
        u8 rrt : 5;
        u8 reserved2 : 3;
        u8 rrl : 5;
        u8 reserved3 : 3;
        u8 rwt : 5;
        u8 reserved4 : 3;
        u8 rwl : 5;
        u8 reserved5 : 3;
        u16 idlp;
        u8 reserved6 : 6;
        u8 ips : 2;
        u8 reserved7;
        u16 actp;
        u8 apw : 3;
        u8 reserved8 : 3;
        u8 aps : 2;
        u8 reserved9[9];
    };

    struct NVME_IDENTIFY_CONTROLLER_DATA {
        u16 vid;     // PCI Vendor ID (VID)
        u16 ssvid;   // PCI Subsystem Vendor ID (SSVID)
        u8 sn[20];   // Serial Number (SN)
        u8 mn[40];   // Model Number (MN)
        u8 fr[8];    // Firmware Revision (FR)
        u8 rab;      // Recommended Arbitration Burst (RAB)
        u8 ieee[3];  // IEEE OUI Identifier (IEEE). Controller Vendor code.

        struct __attribute__((packed)) {
            u8 multi_pc_ie_ports : 1;
            u8 multi_controllers : 1;
            u8 sriov : 1;
            u8 anar : 1;
            u8 reserved : 4;
        } cmic;

        u8 mdts;
        u16 cntlid;
        u32 ver;
        u32 rtd3_r;
        u32 rtd3_e;

        struct __attribute__((packed)) {
            u32 reserved0 : 8;
            u32 namespace_attribute_changed : 1;
            u32 firmware_activation : 1;
            u32 reserved1 : 1;
            u32 asymmetric_access_changed : 1;
            u32 predictable_latency_aggregate_log_changed : 1;
            u32 lba_status_changed : 1;
            u32 endurance_group_aggregate_log_changed : 1;
            u32 reserved2 : 12;
            u32 zone_information : 1;
            u32 reserved3 : 4;
        } oaes;

        struct __attribute__((packed)) {
            u32 host_identifier128_bit : 1;
            u32 nopsp_mode : 1;
            u32 nvm_sets : 1;
            u32 read_recovery_levels : 1;
            u32 endurance_groups : 1;
            u32 predictable_latency_mode : 1;
            u32 tbkas : 1;
            u32 namespace_granularity : 1;
            u32 sq_associations : 1;
            u32 uuid_list : 1;
            u32 reserved0 : 22;
        } ctratt;

        struct __attribute__((packed)) {
            u16 read_recovery_level0 : 1;
            u16 read_recovery_level1 : 1;
            u16 read_recovery_level2 : 1;
            u16 read_recovery_level3 : 1;
            u16 read_recovery_level4 : 1;
            u16 read_recovery_level5 : 1;
            u16 read_recovery_level6 : 1;
            u16 read_recovery_level7 : 1;
            u16 read_recovery_level8 : 1;
            u16 read_recovery_level9 : 1;
            u16 read_recovery_level10 : 1;
            u16 read_recovery_level11 : 1;
            u16 read_recovery_level12 : 1;
            u16 read_recovery_level13 : 1;
            u16 read_recovery_level14 : 1;
            u16 read_recovery_level15 : 1;
        } rrls;

        u8 reserved0[9];
        u8 cntrltype;
        u8 fguid[16];
        u16 crdt1;
        u16 crdt2;
        u16 crdt3;
        u8 reserved0_1[106];
        u8 reserved_for_management[16];

        struct __attribute__((packed)) {
            u16 security_commands : 1;
            u16 format_nvm : 1;
            u16 firmware_commands : 1;
            u16 namespace_commands : 1;
            u16 device_self_test : 1;
            u16 directives : 1;
            u16 nv_me_mi_commands : 1;
            u16 virtualization_mgmt : 1;
            u16 door_bell_buffer_config : 1;
            u16 get_lba_status : 1;
            u16 reserved : 6;
        } oacs;

        u8 acl;
        u8 aerl;

        struct __attribute__((packed)) {
            u8 slot1_read_only : 1;
            u8 slot_count : 3;
            u8 activation_without_reset : 1;
            u8 reserved : 3;
        } frmw;

        struct __attribute__((packed)) {
            u8 smart_page_per_namespace : 1;
            u8 command_effects_log : 1;
            u8 log_page_extended_data : 1;
            u8 telemetry_support : 1;
            u8 persistent_event_log : 1;
            u8 reserved0 : 1;
            u8 telemetry_data_area4 : 1;
            u8 reserved1 : 1;
        } lpa;

        u8 elpe;
        u8 npss;

        struct __attribute__((packed)) {
            u8 command_format_in_spec : 1;
            u8 reserved : 7;
        } avscc;

        struct __attribute__((packed)) {
            u8 supported : 1;
            u8 reserved : 7;
        } apsta;

        u16 wctemp;  // byte 266:267. M - Warning Composite Temperature Threshold (WCTEMP)
        u16 cctemp;  // byte 268:269. M - Critical Composite Temperature Threshold (CCTEMP)
        u16 mtfa;    // byte 270:271. O - Maximum Time for Firmware Activation (MTFA)
        u32 hmpre;   // byte 272:275. O - Host Memory Buffer Preferred Size (HMPRE)
        u32 hmmin;   // byte 276:279. O - Host Memory Buffer Minimum Size (HMMIN)

        u8 tnvmcap[16];  // byte 280:295. O - Total NVM Capacity (TNVMCAP)
        u8 unvmcap[16];  // byte 296:311. O - Unallocated NVM Capacity (UNVMCAP)

        struct __attribute__((packed)) {
            u32 rpmb_unit_count : 3;
            u32 authentication_method : 3;
            u32 reserved0 : 10;
            u32 total_size : 8;
            u32 access_size : 8;
        } rpmbs;

        u16 edstt;
        u8 dsto;
        u8 fwug;
        u16 kas;

        struct __attribute__((packed)) {
            u16 supported : 1;
            u16 reserved : 15;
        } hctma;

        u16 mntmt;
        u16 mxtmt;

        struct __attribute__((packed)) {
            u32 crypto_erase : 1;
            u32 block_erase : 1;
            u32 overwrite : 1;
            u32 reserved : 26;
            u32 ndi : 1;
            u32 nodmmas : 2;
        } sanicap;

        u32 hmminds;
        u16 hmmaxd;
        u16 nsetidmax;
        u16 endgidmax;

        u8 anatt;

        struct __attribute__((packed)) {
            u8 optimized_state : 1;
            u8 non_optimized_state : 1;
            u8 inaccessible_state : 1;
            u8 persistent_loss_state : 1;
            u8 change_state : 1;
            u8 reserved : 1;
            u8 static_anagrpid : 1;
            u8 support_non_zero_anagrpid : 1;
        } anacap;

        u32 anagrpmax;
        u32 nanagrpid;
        u32 pels;

        u8 reserved1[156];

        struct __attribute__((packed)) {
            u8 required_entry_size : 4;
            u8 max_entry_size : 4;
        } sqes;

        struct __attribute__((packed)) {
            u8 required_entry_size : 4;
            u8 max_entry_size : 4;
        } cqes;

        u16 maxcmd;
        u32 nn;

        struct __attribute__((packed)) {
            u16 compare : 1;
            u16 write_uncorrectable : 1;
            u16 dataset_management : 1;
            u16 write_zeroes : 1;
            u16 feature_field : 1;
            u16 reservations : 1;
            u16 timestamp : 1;
            u16 verify : 1;
            u16 reserved : 8;
        } oncs;  // Optional NVM Command Support (ONCS)

        struct __attribute__((packed)) {
            u16 compare_and_write : 1;
            u16 reserved : 15;
        } fuses;  // Fused Operation Support (FUSES)

        struct __attribute__((packed)) {
            u8 format_apply_to_all : 1;
            u8 secure_erase_apply_to_all : 1;
            u8 cryptographic_erase_supported : 1;
            u8 format_support_nsid_all_f : 1;
            u8 reserved : 4;
        } fna;  // Format NVM Attributes (FNA)

        struct __attribute__((packed)) {
            u8 present : 1;
            u8 flush_behavior : 2;
            u8 reserved : 5;
        } vwc;  // Volatile Write Cache (VWC)

        u16 awun;   // Atomic Write Unit Normal (AWUN)
        u16 awupf;  // Atomic Write Unit Power Fail (AWUPF)

        struct __attribute__((packed)) {
            u8 command_format_in_spec : 1;
            u8 reserved : 7;
        } nvscc;  // NVM Vendor Specific Command Configuration (NVSCC)

        struct __attribute__((packed)) {
            u8 write_protect : 1;
            u8 until_power_cycle : 1;
            u8 permanent : 1;
            u8 reserved : 5;
        } nwpc;

        u16 acwu;  // Atomic Compare & Write Unit (ACWU)
        u8 reserved4[2];

        struct __attribute__((packed)) {
            u32 sgl_supported : 2;
            u32 keyed_sgl_data : 1;
            u32 reserved0 : 13;
            u32 bit_bucket_descr_supported : 1;
            u32 byte_aligned_contiguous_physical_buffer : 1;
            u32 sgl_length_larger_than_data_length : 1;
            u32 mptrsgl_descriptor : 1;
            u32 address_field_sgl_data_block : 1;
            u32 transport_sgl_data : 1;
            u32 reserved1 : 10;
        } sgls;  // SGL Support (SGLS)

        u32 mnan;
        u8 reserved6[224];
        u8 subnqn[256];
        u8 reserved7[768];
        u8 reserved8[256];

        NVME_POWER_STATE_DESC pds[32];
        u8 vs[1024];
    };

    union NVME_LBA_FORMAT {
        struct {
            u16 ms;
            u8 lbads;
            u8 rp : 2;
            u8 reserved0 : 6;
        };

        u32 d_word;
    };

    union NVM_RESERVATION_CAPABILITIES {
        struct {
            u8 ptpls : 1;
            u8 wes : 1;
            u8 eas : 1;
            u8 weros : 1;
            u8 earos : 1;
            u8 wears : 1;
            u8 eaars : 1;
            u8 ieks : 1;
        };

        u8 byte;
    };

    struct NVME_IDENTIFY_NAMESPACE_DATA {
        u64 nsze;
        u64 ncap;
        u64 nuse;

        struct {
            u8 thin_provisioning : 1;
            u8 name_space_atomic_write_unit : 1;
            u8 deallocated_or_unwritten_error : 1;
            u8 skip_reuse_ui : 1;
            u8 name_space_io_optimization : 1;
            u8 reserved : 3;
        } nsfeat;

        u8 nlbaf;

        struct {
            u8 lba_format_index : 4;
            u8 metadata_in_extended_data_lba : 1;
            u8 reserved : 3;
        } flbas;

        struct {
            u8 metadata_in_extended_data_lba : 1;
            u8 metadata_in_separate_buffer : 1;
            u8 reserved : 6;
        } mc;

        struct {
            u8 protection_info_type1 : 1;
            u8 protection_info_type2 : 1;
            u8 protection_info_type3 : 1;
            u8 info_at_beginning_of_metadata : 1;
            u8 info_at_end_of_metadata : 1;
            u8 reserved : 3;
        } dpc;

        struct {
            u8 protection_info_type_enabled : 3;
            u8 info_at_beginning_of_metadata : 1;
            u8 reserved : 4;
        } dps;

        struct {
            u8 shared_name_space : 1;
            u8 reserved : 7;
        } nmic;

        NVM_RESERVATION_CAPABILITIES rescap;

        struct {
            u8 percentage_remained : 7;
            u8 supported : 1;
        } fpi;

        struct {
            u8 read_behavior : 3;
            u8 write_zeroes : 1;
            u8 guard_field_with_crc : 1;
            u8 reserved : 3;
        } dlfeat;

        u16 nawun;
        u16 nawupf;
        u16 nacwu;
        u16 nabsn;
        u16 nabo;
        u16 nabspf;
        u16 noiob;

        u8 nvmcap[16];

        u16 npwg;
        u16 npwa;
        u16 npdg;
        u16 npda;
        u16 nows;

        u16 mssrl;
        u32 mcl;
        u8 msrc;

        u8 reserved2[11];

        u32 anagrpid;

        u8 reserved3[3];

        struct {
            u8 write_protected : 1;
            u8 reserved : 7;
        } nsattr;

        u16 nvmsetid;
        u16 endgid;

        u8 nguid[16];
        u8 eui64[8];

        NVME_LBA_FORMAT lbaf[16];

        u8 reserved4[192];
        u8 vs[3712];
    };
}  // namespace nvme

#endif  // NVME_DEFS_H
