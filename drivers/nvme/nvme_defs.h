//
// Created by Linus on 11.07.25.
//

#ifndef NVME_DEFS_H
#define NVME_DEFS_H

#include <cstdint>

namespace NVMe
{
#define PAGE_SIZE_4K 0x1000


    enum NVME_ADMIN_COMMANDS
    {
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

    enum NVME_NVM_COMMANDS
    {
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

    enum NVME_IDENTIFY_CNS_CODES
    {
        NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE = 0,
        NVME_IDENTIFY_CNS_CONTROLLER = 1,
        NVME_IDENTIFY_CNS_ACTIVE_NAMESPACES = 2,
    };

    union NVME_CONTROLLER_CAPABILITIES
    {
        struct
        {
            //LSB

            uint64_t MQES : 16; // RO - Maximum Queue Entries Supported (MQES)
            uint64_t CQR : 1; // RO - Contiguous Queues Required (CQR)

            // Bit 17, 18 - AMS; RO - Arbitration Mechanism Supported (AMS)
            uint64_t AMS_WeightedRoundRobinWithUrgent : 1; // Bit 17: Weighted Round Robin with Urgent;
            uint64_t AMS_VendorSpecific : 1; // Bit 18: Vendor Specific.

            uint64_t Reserved0 : 5; // RO - bit 19 ~ 23
            uint64_t TO : 8; // RO - Timeout (TO)
            uint64_t DSTRD : 4; // RO - Doorbell Stride (DSTRD)
            uint64_t NSSRS : 1; // RO - NVM Subsystem Reset Supported (NSSRS)

            // Bit 37 ~ 44 - CSS; RO - Command Sets Supported (CSS)
            uint64_t CSS_NVM : 1; // Bit 37: NVM command set
            uint64_t CSS_Reserved0 : 1; // Bit 38: Reserved
            uint64_t CSS_Reserved1 : 1; // Bit 39: Reserved
            uint64_t CSS_Reserved2 : 1; // Bit 40: Reserved
            uint64_t CSS_Reserved3 : 1; // Bit 41: Reserved
            uint64_t CSS_Reserved4 : 1; // Bit 42: Reserved
            uint64_t CSS_Reserved5 : 1; // Bit 43: Reserved
            uint64_t CSS_Reserved6 : 1; // Bit 44: Reserved

            uint64_t Reserved2 : 3; // RO - bit 45 ~ 47
            uint64_t MPSMIN : 4; // RO - Memory Page Size Minimum (MPSMIN)
            uint64_t MPSMAX : 4; // RO - Memory Page Size Maximum (MPSMAX)
            uint64_t Reserved3 : 8; // RO - bit 56 ~ 63

            //MSB
        }__attribute__((packed));

        uint64_t QWord;
    };

    union NVME_VERSION
    {
        struct
        {
            //LSB
            uint32_t Reserved : 8;
            uint32_t MNR : 8; // Minor Version Number (MNR)
            uint32_t MJR : 16; // Major Version Number (MJR)
            //MSB
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CONTROLLER_CONFIGURATION
    {
        struct
        {
            //LSB
            uint32_t EN : 1; // RW - Enable (EN)
            uint32_t Reserved0 : 3; // RO
            uint32_t CSS : 3; // RW - I/O  Command Set Selected (CSS)
            uint32_t MPS : 4; // RW - Memory Page Size (MPS)
            uint32_t AMS : 3; // RW - Arbitration Mechanism Selected (AMS)
            uint32_t SHN : 2; // RW - Shutdown Notification (SHN)
            uint32_t IOSQES : 4; // RW - I/O  Submission Queue Entry Size (IOSQES)
            uint32_t IOCQES : 4; // RW - I/O  Completion Queue Entry Size (IOCQES)
            uint32_t Reserved1 : 8; // RO
            //MSB
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CONTROLLER_STATUS
    {
        struct
        {
            uint32_t RDY : 1; // RO - Ready (RDY)
            uint32_t CFS : 1; // RO - Controller Fatal Status (CFS)
            uint32_t SHST : 2; // RO - Shutdown Status (SHST)
            uint32_t NSSRO : 1; // RW1C - NVM Subsystem Reset Occurred (NSSRO)
            uint32_t PP : 1; // RO - Processing Paused (PP)

            uint32_t Reserved0 : 26; // RO
        }__attribute__((packed));

        uint32_t DWord;
    };

    struct NVME_NVM_SUBSYSTEM_RESET
    {
        uint32_t NSSRC; // RW - NVM Subsystem Reset Control (NSSRC)
    };

    union NVME_ADMIN_QUEUE_ATTRIBUTES
    {
        struct
        {
            //LSB
            uint32_t ASQS : 12; // RW - Admin  Submission Queue Size (ASQS)
            uint32_t Reserved0 : 4; // RO
            uint32_t ACQS : 12; // RW - Admin  Completion Queue Size (ACQS)
            uint32_t Reserved1 : 4; // RO
            //MSB
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS
    {
        struct
        {
            //LSB
            uint64_t Reserved0 : 12; // RO
            uint64_t ASQB : 52; // RW - Admin Submission Queue Base (ASQB)
            //MSB
        }__attribute__((packed));

        uint64_t QWord;
    };

    union NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS
    {
        struct
        {
            //LSB
            uint64_t Reserved0 : 12; // RO
            uint64_t ACQB : 52; // RW - Admin Completion Queue Base (ACQB)
            //MSB
        }__attribute__((packed));

        uint64_t QWord;
    };

    union NVME_CONTROLLER_MEMORY_BUFFER_LOCATION
    {
        struct
        {
            //LSB
            uint32_t BIR : 3; // RO - Base Indicator Register (BIR)
            uint32_t Reserved : 9; // RO
            uint32_t OFST : 20; // RO - Offset (OFST)
            //MSB
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CONTROLLER_MEMORY_BUFFER_SIZE
    {
        struct
        {
            //LSB
            uint32_t SQS : 1; // RO - Submission Queue Support (SQS)
            uint32_t CQS : 1; // RO - Completion Queue Support (CQS)
            uint32_t LISTS : 1; // RO - PRP SGL List Support (LISTS)
            uint32_t RDS : 1; // RO - Read Data Support (RDS)
            uint32_t WDS : 1; // RO - Write Data Support (WDS)
            uint32_t Reserved : 3; // RO
            uint32_t SZU : 4; // RO - Size Units (SZU)
            uint32_t SZ : 20; // RO - Size (SZ)
            //MSB
        }__attribute__((packed));

        uint32_t DWord;
    };

    struct NVME_CONTROLLER_REGISTERS
    {
        NVME_CONTROLLER_CAPABILITIES CAP; // Controller Capabilities; 8 bytes
        NVME_VERSION VS; // Version
        uint32_t INTMS; // Interrupt Mask Set
        uint32_t INTMC; // Interrupt Mask Clear
        NVME_CONTROLLER_CONFIGURATION CC; // Controller Configuration
        uint32_t Reserved0;
        NVME_CONTROLLER_STATUS CSTS; // Controller Status
        NVME_NVM_SUBSYSTEM_RESET NSSR; // NVM Subsystem Reset (Optional)

        NVME_ADMIN_QUEUE_ATTRIBUTES AQA; // Admin Queue Attributes
        NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS ASQ; // Admin Submission Queue Base Address; 8 bytes
        NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS ACQ; // Admin Completion Queue Base Address; 8 bytes

        NVME_CONTROLLER_MEMORY_BUFFER_LOCATION CMBLOC; // Controller Memory Buffer Location (Optional)
        NVME_CONTROLLER_MEMORY_BUFFER_SIZE CMBSZ; // Controller Memory Buffer Size (Optional)

        uint32_t Reserved2[944]; // 40h ~ EFFh
        uint32_t Reserved3[64]; // F00h ~ FFFh, Command Set Specific

        uint32_t Doorbells[0]; // Start of the first Doorbell register. (Admin SQ Tail Doorbell)
    };

    enum DriverStatus
    {
        ControllerNotReady,
        ControllerError,
        ControllerReady,
    };

    union NVME_COMMAND_DWORD0
    {
        struct
        {
            uint32_t OPC : 8;
            uint32_t FUSE : 2;
            uint32_t Reserved0 : 5;
            uint32_t PSDT : 1;
            uint32_t CID : 16;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW10_IDENTIFY
    {
        struct
        {
            uint32_t CNS : 8;
            uint32_t Reserved : 8;
            uint32_t CNTID : 16;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_IDENTIFY
    {
        struct
        {
            uint16_t NVMSETID;
            uint16_t Reserved;
        };

        struct
        {
            uint32_t CNSID : 16;
            uint32_t Reserved2 : 8;
            uint32_t CSI : 8;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW10_ABORT
    {
        struct
        {
            uint32_t SQID : 8;
            uint32_t CID : 16;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW10_GET_FEATURES
    {
        struct
        {
            uint32_t FID : 8;
            uint32_t SEL : 3;
            uint32_t Reserved0 : 21;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW10_SET_FEATURES
    {
        struct
        {
            uint32_t FID : 8;
            uint32_t Reserved0 : 23;
            uint32_t SV : 1;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_NUMBER_OF_QUEUES
    {
        struct
        {
            uint32_t NSQ : 16;
            uint32_t NCQ : 16;
        } __attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_INTERRUPT_COALESCING
    {
        struct
        {
            uint32_t THR : 8;
            uint32_t TIME : 8;
            uint32_t Reserved0 : 16;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_INTERRUPT_VECTOR_CONFIG
    {
        struct
        {
            uint32_t IV : 16;
            uint32_t CD : 1;
            uint32_t Reserved0 : 15;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_LBA_RANGE_TYPE
    {
        struct
        {
            uint32_t NUM : 6;
            uint32_t Reserved0 : 26;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_ARBITRATION
    {
        struct
        {
            uint32_t AB : 3;
            uint32_t Reserved0 : 5;
            uint32_t LPW : 8;
            uint32_t MPW : 8;
            uint32_t HPW : 8;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_VOLATILE_WRITE_CACHE
    {
        struct
        {
            uint32_t WCE : 1;
            uint32_t Reserved0 : 31;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_ASYNC_EVENT_CONFIG
    {
        struct
        {
            uint32_t CriticalWarnings : 8;
            uint32_t NsAttributeNotices : 1;
            uint32_t FwActivationNotices : 1;
            uint32_t TelemetryLogNotices : 1;
            uint32_t ANAChangeNotices : 1;
            uint32_t PredictableLogChangeNotices : 1;
            uint32_t LBAStatusNotices : 1;
            uint32_t EnduranceEventNotices : 1;
            uint32_t Reserved0 : 12;
            uint32_t ZoneDescriptorNotices : 1;
            uint32_t Reserved1 : 4;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_POWER_MANAGEMENT
    {
        struct
        {
            uint32_t PS : 5;
            uint32_t Reserved0 : 27;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_AUTO_POWER_STATE_TRANSITION
    {
        struct
        {
            uint32_t APSTE : 1;
            uint32_t Reserved0 : 31;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_TEMPERATURE_THRESHOLD
    {
        struct
        {
            uint32_t TMPTH : 16;
            uint32_t TMPSEL : 4;
            uint32_t THSEL : 2;
            uint32_t Reserved0 : 10;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_HOST_MEMORY_BUFFER
    {
        struct
        {
            uint32_t EHM : 1;
            uint32_t MR : 1;
            uint32_t Reserved : 30;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_WRITE_ATOMICITY_NORMAL
    {
        struct
        {
            uint32_t DN : 1;
            uint32_t Reserved0 : 31;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURE_NON_OPERATIONAL_POWER_STATE
    {
        struct
        {
            uint32_t NOPPME : 1;
            uint32_t Reserved0 : 31;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_FEATURES
    {
        NVME_CDW11_FEATURE_NUMBER_OF_QUEUES NumberOfQueues;
        NVME_CDW11_FEATURE_INTERRUPT_COALESCING InterruptCoalescing;
        NVME_CDW11_FEATURE_INTERRUPT_VECTOR_CONFIG InterruptVectorConfig;
        NVME_CDW11_FEATURE_LBA_RANGE_TYPE LbaRangeType;
        NVME_CDW11_FEATURE_ARBITRATION Arbitration;
        NVME_CDW11_FEATURE_VOLATILE_WRITE_CACHE VolatileWriteCache;
        NVME_CDW11_FEATURE_ASYNC_EVENT_CONFIG AsyncEventConfig;
        NVME_CDW11_FEATURE_POWER_MANAGEMENT PowerManagement;
        NVME_CDW11_FEATURE_AUTO_POWER_STATE_TRANSITION AutoPowerStateTransition;
        NVME_CDW11_FEATURE_TEMPERATURE_THRESHOLD TemperatureThreshold;
        NVME_CDW11_FEATURE_HOST_MEMORY_BUFFER HostMemoryBuffer;
        NVME_CDW11_FEATURE_WRITE_ATOMICITY_NORMAL WriteAtomicityNormal;
        NVME_CDW11_FEATURE_NON_OPERATIONAL_POWER_STATE NonOperationalPowerState;
        uint32_t DWord;
    };

    union NVME_CDW12_FEATURE_HOST_MEMORY_BUFFER
    {
        struct
        {
            uint32_t HSIZE;
        };

        uint32_t DWord;
    };

    union NVME_CDW12_FEATURES
    {
        NVME_CDW12_FEATURE_HOST_MEMORY_BUFFER HostMemoryBuffer;
        uint32_t DWord;
    };

    union NVME_CDW13_FEATURE_HOST_MEMORY_BUFFER
    {
        struct
        {
            uint32_t Reserved : 4;
            uint32_t HMDLLA : 28;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW13_FEATURES
    {
        NVME_CDW13_FEATURE_HOST_MEMORY_BUFFER HostMemoryBuffer;
        uint32_t DWord;
    };

    union NVME_CDW14_FEATURE_HOST_MEMORY_BUFFER
    {
        struct
        {
            uint32_t HMDLUA;
        };

        uint32_t AsUlong;
    };

    union NVME_CDW14_FEATURES
    {
        NVME_CDW14_FEATURE_HOST_MEMORY_BUFFER HostMemoryBuffer;
        uint32_t DWord;
    };

    union NVME_CDW15_FEATURE_HOST_MEMORY_BUFFER
    {
        struct
        {
            uint32_t HMDLEC;
        };

        uint32_t AsUlong;
    };

    union NVME_CDW15_FEATURES
    {
        NVME_CDW15_FEATURE_HOST_MEMORY_BUFFER HostMemoryBuffer;
        uint32_t DWord;
    };

    union NVME_CDW10_GET_LOG_PAGE
    {
        struct
        {
            uint32_t LID : 8;
            uint32_t Reserved0 : 8;
            uint32_t NUMD : 12;
            uint32_t Reserved1 : 4;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW10_GET_LOG_PAGE_V13
    {
        struct
        {
            uint32_t LID : 8;
            uint32_t LSP : 4;
            uint32_t Reserved0 : 3;
            uint32_t RAE : 1;
            uint32_t NUMDL : 16;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_GET_LOG_PAGE
    {
        struct
        {
            uint32_t NUMDU : 16;
            uint32_t LogSpecificIdentifier : 16;
        }__attribute__((packed));

        uint32_t DWord;
    };

    struct NVME_CDW12_GET_LOG_PAGE
    {
        uint32_t LPOL;
    };

    struct NVME_CDW13_GET_LOG_PAGE
    {
        uint32_t LPOU;
    };

    struct NVME_CDW14_GET_LOG_PAGE
    {
        uint32_t Bitfield;
    };

    union NVME_CDW10_CREATE_IO_QUEUE
    {
        struct
        {
            uint32_t QID : 16;
            uint32_t QSIZE : 16;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_CREATE_IO_SQ
    {
        struct
        {
            uint32_t PC : 1;
            uint32_t QPRIO : 2;
            uint32_t Reserved0 : 13;
            uint32_t CQID : 16;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW11_CREATE_IO_CQ
    {
        struct
        {
            uint32_t PC : 1; // Physically Contiguous (PC)
            uint32_t IEN : 1; // Interrupts Enabled (IEN)
            uint32_t Reserved0 : 14;
            uint32_t IV : 16; // Interrupt Vector (IV)
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CONTEXT_ATTRIBUTES
    {
        struct
        {
            uint32_t AccessFrequency : 4;
            uint32_t AccessLatency : 2;
            uint32_t Reserved0 : 2;
            uint32_t SequentialReadRange : 1;
            uint32_t SequentialWriteRange : 1;
            uint32_t WritePrepare : 1;
            uint32_t Reserved1 : 13;
            uint32_t CommandAccessSize : 8;
        }__attribute__((packed));

        uint32_t DWord;
    };

    struct NVME_LBA_RANGE
    {
        NVME_CONTEXT_ATTRIBUTES Attributes;
        uint32_t LogicalBlockCount;
        uint64_t StartingLBA;
    };

    union NVME_CDW11_DATASET_MANAGEMENT
    {
        struct
        {
            uint32_t IDR : 1; // Integral Dataset for Read (IDR)
            uint32_t IDW : 1; // Integral Dataset for Write (IDW)
            uint32_t AD : 1; // Deallocate (AD)
            uint32_t Reserved : 29;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW10_DATASET_MANAGEMENT
    {
        struct
        {
            uint32_t NR : 8; // Number of Ranges (NR)
            uint32_t Reserved : 24;
        }__attribute__((packed));

        uint32_t DWord;
    };


    union NVME_CDW10_SECURITY_SEND_RECEIVE
    {
        struct
        {
            uint32_t Reserved0 : 8;
            uint32_t SPSP : 16; // SP Specific (SPSP)
            uint32_t SECP : 8; // Security Protocol (SECP)
        }__attribute__((packed));

        uint32_t DWord;
    };


    struct NVME_CDW11_SECURITY_SEND
    {
        uint32_t TL; // Transfer Length  (TL):
    };

    struct NVME_CDW11_SECURITY_RECEIVE
    {
        uint32_t AL; // Transfer Length  (AL)
    };

    union NVME_CDW10_FORMAT_NVM
    {
        struct
        {
            uint32_t LBAF : 4; // LBA Format (LBAF)
            uint32_t MS : 1; // Metadata Settings (MS)
            uint32_t PI : 3; // Protection Information (PI)
            uint32_t PIL : 1; // Protection Information Location (PIL)
            uint32_t SES : 3; // Secure Erase Settings (SES)

            uint32_t Reserved : 20;
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW12_READ_WRITE
    {
        struct
        {
            uint32_t NLB : 16; // Number of Logical Blocks (NLB)
            uint32_t Reserved0 : 10;
            uint32_t PRINFO : 4; // Protection Information Field (PRINFO)
            uint32_t FUA : 1; // Force Unit Access (FUA)
            uint32_t LR : 1; // Limited Retry (LR)
        }__attribute__((packed));

        uint32_t DWord;
    };

    union NVME_CDW13_READ_WRITE
    {
        struct
        {
            struct
            {
                uint8_t AccessFrequency : 4;
                uint8_t AccessLatency : 2;
                uint8_t SequentialRequest : 1;
                uint8_t Incompressible : 1;
            }__attribute__((packed)) DSM; // Dataset Management (DSM)

            uint8_t Reserved0[3];
        };

        uint32_t DWord;
    };

    union NVME_CDW15_READ_WRITE
    {
        struct
        {
            uint32_t ELBAT : 16; // Expected Logical Block Application Tag (ELBAT)
            uint32_t ELBATM : 16; // Expected Logical Block Application Tag Mask (ELBATM)
        }__attribute__((packed));

        uint32_t DWord;
    };

    struct NVME_COMMAND
    {
        NVME_COMMAND_DWORD0 CDW0;
        uint32_t NSID;
        uint32_t Reserved0[2];
        uint64_t MPTR;
        uint64_t PRP1;
        uint64_t PRP2;

        union
        {
            struct GENERAL
            {
                uint32_t CDW10;
                uint32_t CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } GENERAL;

            struct IDENTIFY
            {
                NVME_CDW10_IDENTIFY CDW10;
                NVME_CDW11_IDENTIFY CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } IDENTIFY;

            struct ABORT
            {
                NVME_CDW10_ABORT CDW10;
                uint32_t CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } ABORT;

            struct GETFEATURES
            {
                NVME_CDW10_GET_FEATURES CDW10;
                NVME_CDW11_FEATURES CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } GETFEATURES;

            struct SETFEATURES
            {
                NVME_CDW10_SET_FEATURES CDW10;
                NVME_CDW11_FEATURES CDW11;
                NVME_CDW12_FEATURES CDW12;
                NVME_CDW13_FEATURES CDW13;
                NVME_CDW14_FEATURES CDW14;
                NVME_CDW15_FEATURES CDW15;
            } SETFEATURES;

            struct GETLOGPAGE
            {
                union
                {
                    NVME_CDW10_GET_LOG_PAGE CDW10;
                    NVME_CDW10_GET_LOG_PAGE_V13 CDW10_V13;
                };

                NVME_CDW11_GET_LOG_PAGE CDW11;
                NVME_CDW12_GET_LOG_PAGE CDW12;
                NVME_CDW13_GET_LOG_PAGE CDW13;
                NVME_CDW14_GET_LOG_PAGE CDW14;
                uint32_t CDW15;
            } GETLOGPAGE;

            struct CREATEIOCQ
            {
                NVME_CDW10_CREATE_IO_QUEUE CDW10;
                NVME_CDW11_CREATE_IO_CQ CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } CREATEIOCQ;

            struct CREATEIOSQ
            {
                NVME_CDW10_CREATE_IO_QUEUE CDW10;
                NVME_CDW11_CREATE_IO_SQ CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } CREATEIOSQ;

            struct DATASETMANAGEMENT
            {
                NVME_CDW10_DATASET_MANAGEMENT CDW10;
                NVME_CDW11_DATASET_MANAGEMENT CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } DATASETMANAGEMENT;

            struct SECURITYSEND
            {
                NVME_CDW10_SECURITY_SEND_RECEIVE CDW10;
                NVME_CDW11_SECURITY_SEND CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } SECURITYSEND;

            struct SECURITYRECEIVE
            {
                NVME_CDW10_SECURITY_SEND_RECEIVE CDW10;
                NVME_CDW11_SECURITY_RECEIVE CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } SECURITYRECEIVE;

            struct FORMATNVM
            {
                NVME_CDW10_FORMAT_NVM CDW10;
                uint32_t CDW11;
                uint32_t CDW12;
                uint32_t CDW13;
                uint32_t CDW14;
                uint32_t CDW15;
            } FORMATNVM;

            struct READWRITE
            {
                uint32_t LBALOW;
                uint32_t LBAHIGH;
                NVME_CDW12_READ_WRITE CDW12;
                NVME_CDW13_READ_WRITE CDW13;
                uint32_t CDW14;
                NVME_CDW15_READ_WRITE CDW15;
            } READWRITE;
        } u;
    }__attribute__((packed));

    struct NVME_COMPLETION_ENTRY
    {
        uint32_t DW0;
        uint32_t Reserved;

        union
        {
            struct
            {
                uint16_t SQHD; // SQ Head Pointer (SQHD)
                uint16_t SQID; // SQ Identifier (SQID)
            };

            uint32_t DWord;
        } DW2;

        union
        {
            struct
            {
                uint16_t CID : 16; // Command Identifier (CID)
                uint16_t P : 1; // Phase Tag (P)
                uint16_t Status : 15;
            }__attribute__((packed));

            uint32_t DWord;
        } DW3;
    };

    class NvmeQueue
    {
        uint16_t queue_id = 0;

        uintptr_t completion_base{};
        uintptr_t submission_base{};

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

        NvmeQueue(uint16_t qid, uintptr_t cq_base, uintptr_t sq_base, void* cq, void* sq, volatile uint32_t* cq_db,
                  volatile uint32_t* sq_db, uint16_t csz, uint16_t ssz);

        ~NvmeQueue() = default;

        NvmeQueue() = default;

        static long Consume(NVME_COMMAND& cmd);

        void Submit(NVME_COMMAND& cmd);

        void SubmitWait(NVME_COMMAND& cmd, NVME_COMPLETION_ENTRY& complet);

        [[nodiscard]] uint16_t CQSize() const { return cq_count; }
        [[nodiscard]] uint16_t SQSize() const { return sq_count; }
        [[nodiscard]] uintptr_t CQBase() const { return completion_base; }
        [[nodiscard]] uintptr_t SQBase() const { return submission_base; }
    };

    struct NVME_POWER_STATE_DESC
    {
        uint16_t MP;
        uint8_t Reserved0;
        uint8_t MPS : 1;
        uint8_t NOPS : 1;
        uint8_t Reserved1 : 6;
        uint32_t ENLAT;
        uint32_t EXLAT;
        uint8_t RRT : 5;
        uint8_t Reserved2 : 3;
        uint8_t RRL : 5;
        uint8_t Reserved3 : 3;
        uint8_t RWT : 5;
        uint8_t Reserved4 : 3;
        uint8_t RWL : 5;
        uint8_t Reserved5 : 3;
        uint16_t IDLP;
        uint8_t Reserved6 : 6;
        uint8_t IPS : 2;
        uint8_t Reserved7;
        uint16_t ACTP;
        uint8_t APW : 3;
        uint8_t Reserved8 : 3;
        uint8_t APS : 2;
        uint8_t Reserved9[9];
    };


    struct NVME_IDENTIFY_CONTROLLER_DATA
    {
        uint16_t VID;
        uint16_t SSVID;
        uint8_t SN[20];
        uint8_t MN[40];
        uint8_t FR[8];
        uint8_t RAB;
        uint8_t IEEE[3];

        struct __attribute__((packed))
        {
            uint8_t MultiPCIePorts : 1;
            uint8_t MultiControllers : 1;
            uint8_t SRIOV : 1;
            uint8_t ANAR : 1;
            uint8_t Reserved : 4;
        } CMIC;

        uint8_t MDTS;
        uint16_t CNTLID;
        uint32_t VER;
        uint32_t RTD3R;
        uint32_t RTD3E;

        struct __attribute__((packed))
        {
            uint32_t Reserved0 : 8;
            uint32_t NamespaceAttributeChanged : 1;
            uint32_t FirmwareActivation : 1;
            uint32_t Reserved1 : 1;
            uint32_t AsymmetricAccessChanged : 1;
            uint32_t PredictableLatencyAggregateLogChanged : 1;
            uint32_t LbaStatusChanged : 1;
            uint32_t EnduranceGroupAggregateLogChanged : 1;
            uint32_t Reserved2 : 12;
            uint32_t ZoneInformation : 1;
            uint32_t Reserved3 : 4;
        } OAES;

        struct __attribute__((packed))
        {
            uint32_t HostIdentifier128Bit : 1;
            uint32_t NOPSPMode : 1;
            uint32_t NVMSets : 1;
            uint32_t ReadRecoveryLevels : 1;
            uint32_t EnduranceGroups : 1;
            uint32_t PredictableLatencyMode : 1;
            uint32_t TBKAS : 1;
            uint32_t NamespaceGranularity : 1;
            uint32_t SQAssociations : 1;
            uint32_t UUIDList : 1;
            uint32_t Reserved0 : 22;
        } CTRATT;

        struct __attribute__((packed))
        {
            uint16_t ReadRecoveryLevel0 : 1;
            uint16_t ReadRecoveryLevel1 : 1;
            uint16_t ReadRecoveryLevel2 : 1;
            uint16_t ReadRecoveryLevel3 : 1;
            uint16_t ReadRecoveryLevel4 : 1;
            uint16_t ReadRecoveryLevel5 : 1;
            uint16_t ReadRecoveryLevel6 : 1;
            uint16_t ReadRecoveryLevel7 : 1;
            uint16_t ReadRecoveryLevel8 : 1;
            uint16_t ReadRecoveryLevel9 : 1;
            uint16_t ReadRecoveryLevel10 : 1;
            uint16_t ReadRecoveryLevel11 : 1;
            uint16_t ReadRecoveryLevel12 : 1;
            uint16_t ReadRecoveryLevel13 : 1;
            uint16_t ReadRecoveryLevel14 : 1;
            uint16_t ReadRecoveryLevel15 : 1;
        } RRLS;

        uint8_t Reserved0[9];
        uint8_t CNTRLTYPE;
        uint8_t FGUID[16];
        uint16_t CRDT1;
        uint16_t CRDT2;
        uint16_t CRDT3;
        uint8_t Reserved0_1[106];
        uint8_t ReservedForManagement[16];

        struct __attribute__((packed))
        {
            uint16_t SecurityCommands : 1;
            uint16_t FormatNVM : 1;
            uint16_t FirmwareCommands : 1;
            uint16_t NamespaceCommands : 1;
            uint16_t DeviceSelfTest : 1;
            uint16_t Directives : 1;
            uint16_t NVMeMICommands : 1;
            uint16_t VirtualizationMgmt : 1;
            uint16_t DoorBellBufferConfig : 1;
            uint16_t GetLBAStatus : 1;
            uint16_t Reserved : 6;
        } OACS;

        uint8_t ACL;
        uint8_t AERL;

        struct __attribute__((packed))
        {
            uint8_t Slot1ReadOnly : 1;
            uint8_t SlotCount : 3;
            uint8_t ActivationWithoutReset : 1;
            uint8_t Reserved : 3;
        } FRMW;

        struct __attribute__((packed))
        {
            uint8_t SmartPagePerNamespace : 1;
            uint8_t CommandEffectsLog : 1;
            uint8_t LogPageExtendedData : 1;
            uint8_t TelemetrySupport : 1;
            uint8_t PersistentEventLog : 1;
            uint8_t Reserved0 : 1;
            uint8_t TelemetryDataArea4 : 1;
            uint8_t Reserved1 : 1;
        } LPA;

        uint8_t ELPE;
        uint8_t NPSS;

        struct __attribute__((packed))
        {
            uint8_t CommandFormatInSpec : 1;
            uint8_t Reserved : 7;
        } AVSCC;

        struct __attribute__((packed))
        {
            uint8_t Supported : 1;
            uint8_t Reserved : 7;
        } APSTA;

        uint16_t WCTEMP;
        uint16_t CCTEMP;
        uint16_t MTFA;
        uint32_t HMPRE;
        uint32_t HMMIN;

        uint8_t TNVMCAP[16];
        uint8_t UNVMCAP[16];

        struct __attribute__((packed))
        {
            uint32_t RPMBUnitCount : 3;
            uint32_t AuthenticationMethod : 3;
            uint32_t Reserved0 : 10;
            uint32_t TotalSize : 8;
            uint32_t AccessSize : 8;
        } RPMBS;

        uint16_t EDSTT;
        uint8_t DSTO;
        uint8_t FWUG;
        uint16_t KAS;

        struct __attribute__((packed))
        {
            uint16_t Supported : 1;
            uint16_t Reserved : 15;
        } HCTMA;

        uint16_t MNTMT;
        uint16_t MXTMT;

        struct __attribute__((packed))
        {
            uint32_t CryptoErase : 1;
            uint32_t BlockErase : 1;
            uint32_t Overwrite : 1;
            uint32_t Reserved : 26;
            uint32_t NDI : 1;
            uint32_t NODMMAS : 2;
        } SANICAP;

        uint32_t HMMINDS;
        uint16_t HMMAXD;
        uint16_t NSETIDMAX;
        uint16_t ENDGIDMAX;

        uint8_t ANATT;

        struct __attribute__((packed))
        {
            uint8_t OptimizedState : 1;
            uint8_t NonOptimizedState : 1;
            uint8_t InaccessibleState : 1;
            uint8_t PersistentLossState : 1;
            uint8_t ChangeState : 1;
            uint8_t Reserved : 1;
            uint8_t StaticANAGRPID : 1;
            uint8_t SupportNonZeroANAGRPID : 1;
        } ANACAP;

        uint32_t ANAGRPMAX;
        uint32_t NANAGRPID;
        uint32_t PELS;

        uint8_t Reserved1[156];

        struct __attribute__((packed))
        {
            uint8_t RequiredEntrySize : 4;
            uint8_t MaxEntrySize : 4;
        } SQES;

        struct __attribute__((packed))
        {
            uint8_t RequiredEntrySize : 4;
            uint8_t MaxEntrySize : 4;
        } CQES;

        uint16_t MAXCMD;
        uint32_t NN;

        struct __attribute__((packed))
        {
            uint16_t Compare : 1;
            uint16_t WriteUncorrectable : 1;
            uint16_t DatasetManagement : 1;
            uint16_t WriteZeroes : 1;
            uint16_t FeatureField : 1;
            uint16_t Reservations : 1;
            uint16_t Timestamp : 1;
            uint16_t Verify : 1;
            uint16_t Reserved : 8;
        } ONCS;

        struct __attribute__((packed))
        {
            uint16_t CompareAndWrite : 1;
            uint16_t Reserved : 15;
        } FUSES;

        struct __attribute__((packed))
        {
            uint8_t FormatApplyToAll : 1;
            uint8_t SecureEraseApplyToAll : 1;
            uint8_t CryptographicEraseSupported : 1;
            uint8_t FormatSupportNSIDAllF : 1;
            uint8_t Reserved : 4;
        } FNA;

        struct __attribute__((packed))
        {
            uint8_t Present : 1;
            uint8_t FlushBehavior : 2;
            uint8_t Reserved : 5;
        } VWC;

        uint16_t AWUN;
        uint16_t AWUPF;

        struct __attribute__((packed))
        {
            uint8_t CommandFormatInSpec : 1;
            uint8_t Reserved : 7;
        } NVSCC;

        struct __attribute__((packed))
        {
            uint8_t WriteProtect : 1;
            uint8_t UntilPowerCycle : 1;
            uint8_t Permanent : 1;
            uint8_t Reserved : 5;
        } NWPC;

        uint16_t ACWU;
        uint8_t Reserved4[2];

        struct __attribute__((packed))
        {
            uint32_t SGLSupported : 2;
            uint32_t KeyedSGLData : 1;
            uint32_t Reserved0 : 13;
            uint32_t BitBucketDescrSupported : 1;
            uint32_t ByteAlignedContiguousPhysicalBuffer : 1;
            uint32_t SGLLengthLargerThanDataLength : 1;
            uint32_t MPTRSGLDescriptor : 1;
            uint32_t AddressFieldSGLDataBlock : 1;
            uint32_t TransportSGLData : 1;
            uint32_t Reserved1 : 10;
        } SGLS;

        uint32_t MNAN;
        uint8_t Reserved6[224];
        uint8_t SUBNQN[256];
        uint8_t Reserved7[768];
        uint8_t Reserved8[256];

        NVME_POWER_STATE_DESC PDS[32];
        uint8_t VS[1024];
    };

    union NVME_LBA_FORMAT
    {
        struct
        {
            uint16_t MS;
            uint8_t LBADS;
            uint8_t RP : 2;
            uint8_t Reserved0 : 6;
        };

        uint32_t DWord;
    };

    union NVM_RESERVATION_CAPABILITIES
    {
        struct
        {
            uint8_t PTPLS : 1;
            uint8_t WES : 1;
            uint8_t EAS : 1;
            uint8_t WEROS : 1;
            uint8_t EAROS : 1;
            uint8_t WEARS : 1;
            uint8_t EAARS : 1;
            uint8_t IEKS : 1;
        };

        uint8_t Byte;
    };

    struct NVME_IDENTIFY_NAMESPACE_DATA
    {
        uint64_t NSZE;
        uint64_t NCAP;
        uint64_t NUSE;

        struct
        {
            uint8_t ThinProvisioning : 1;
            uint8_t NameSpaceAtomicWriteUnit : 1;
            uint8_t DeallocatedOrUnwrittenError : 1;
            uint8_t SkipReuseUI : 1;
            uint8_t NameSpaceIoOptimization : 1;
            uint8_t Reserved : 3;
        } NSFEAT;

        uint8_t NLBAF;

        struct
        {
            uint8_t LbaFormatIndex : 4;
            uint8_t MetadataInExtendedDataLBA : 1;
            uint8_t Reserved : 3;
        } FLBAS;

        struct
        {
            uint8_t MetadataInExtendedDataLBA : 1;
            uint8_t MetadataInSeparateBuffer : 1;
            uint8_t Reserved : 6;
        } MC;

        struct
        {
            uint8_t ProtectionInfoType1 : 1;
            uint8_t ProtectionInfoType2 : 1;
            uint8_t ProtectionInfoType3 : 1;
            uint8_t InfoAtBeginningOfMetadata : 1;
            uint8_t InfoAtEndOfMetadata : 1;
            uint8_t Reserved : 3;
        } DPC;

        struct
        {
            uint8_t ProtectionInfoTypeEnabled : 3;
            uint8_t InfoAtBeginningOfMetadata : 1;
            uint8_t Reserved : 4;
        } DPS;

        struct
        {
            uint8_t SharedNameSpace : 1;
            uint8_t Reserved : 7;
        } NMIC;

        NVM_RESERVATION_CAPABILITIES RESCAP;

        struct
        {
            uint8_t PercentageRemained : 7;
            uint8_t Supported : 1;
        } FPI;

        struct
        {
            uint8_t ReadBehavior : 3;
            uint8_t WriteZeroes : 1;
            uint8_t GuardFieldWithCRC : 1;
            uint8_t Reserved : 3;
        } DLFEAT;

        uint16_t NAWUN;
        uint16_t NAWUPF;
        uint16_t NACWU;
        uint16_t NABSN;
        uint16_t NABO;
        uint16_t NABSPF;
        uint16_t NOIOB;

        uint8_t NVMCAP[16];

        uint16_t NPWG;
        uint16_t NPWA;
        uint16_t NPDG;
        uint16_t NPDA;
        uint16_t NOWS;

        uint16_t MSSRL;
        uint32_t MCL;
        uint8_t MSRC;

        uint8_t Reserved2[11];

        uint32_t ANAGRPID;

        uint8_t Reserved3[3];

        struct
        {
            uint8_t WriteProtected : 1;
            uint8_t Reserved : 7;
        } NSATTR;

        uint16_t NVMSETID;
        uint16_t ENDGID;

        uint8_t NGUID[16];
        uint8_t EUI64[8];

        NVME_LBA_FORMAT LBAF[16];

        uint8_t Reserved4[192];
        uint8_t VS[3712];
    };
}

#endif //NVME_DEFS_H
