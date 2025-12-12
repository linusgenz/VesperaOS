//
// Created by Linus on 11.07.25.
//

#ifndef NVME_DEFS_H
#define NVME_DEFS_H

#include <cstdint>

namespace NVMe
{
#define NVME_CAP_CMBS (1 << 57) // Controller memory buffer supported
#define NVME_CAP_PMRS (1 << 56) // Persistent memory region supported
#define NVME_CAP_BPS (1 << 45) // Boot partition support
#define NVME_CAP_NVM_CMD_SET (1UL << 37) // NVM command set supported
#define NVME_CAP_NSSRS (1UL << 36) // NVM subsystem reset supported
#define NVME_CAP_CQR (1 << 16) // Contiguous Queues Required

#define NVME_CAP_MPS_MASK 0xfU
#define NVME_CAP_MPSMAX(x) (((x) >> 52) & NVME_CAP_MPS_MASK) // Max supported memory page size (2 ^ (12 + MPSMAX))
#define NVME_CAP_MPSMIN(x) (((x) >> 48) & NVME_CAP_MPS_MASK) // Min supported memory page size (2 ^ (12 + MPSMIN))

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

    struct Registers
    {
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

    enum DriverStatus
    {
        ControllerNotReady,
        ControllerError,
        ControllerReady,
    };

    struct NvmeCreateIoCompletionQueueCommand
    {
        struct
        {
            uint32_t queue_id : 16;
            uint32_t queue_size : 16;
        } __attribute__((packed));

        struct
        {
            uint32_t contiguous : 1;
            uint32_t int_enable : 1;
            uint32_t reserved : 14;
            uint32_t int_vector : 16;
        } __attribute__((packed));
    };

    struct NvmeCreateIoSubmissionQueueCommand
    {
        struct
        {
            uint32_t queue_id : 16;
            uint32_t queue_size : 16;
        } __attribute__((packed));

        struct
        {
            uint32_t contiguous : 1;
            uint32_t priority : 2;
            uint32_t reserved : 13;
            uint32_t cq_id : 16;
        } __attribute__((packed));
    };

    struct NvmeIdentifyCommand
    {
        enum
        {
            CnsNamespace = 0,
            CnsController = 1,
            CnsNamespaceList = 2,
        };

        struct
        {
            uint32_t cns : 8;
            uint32_t reserved : 8;
            uint32_t cnt_id : 16;
        } __attribute__((packed));

        uint32_t nvm_set_id;
    };

    struct NvmeDeleteIoQueueCommand
    {
        uint32_t queue_id;
    };

    struct NvmeSetFeaturesCommand
    {
        enum
        {
            FeatureIdNumberOfQueues = 0x7,
        };

        struct
        {
            uint32_t feature_id : 8;
            uint32_t reserved : 23;
            uint32_t save : 1;
        } __attribute__((packed));

        uint32_t dw11;
        uint32_t dw12;
        uint32_t dw13;
    };

    struct NvmeReadCommand
    {
        uint64_t start_lba;

        struct
        {
            uint32_t block_num : 16;
            uint32_t reserved : 10;
            uint32_t pr_info : 4;
            uint32_t force_unit_access : 1;
            uint32_t limited_retry : 1;
        } __attribute__((packed));
    };

    struct NvmeWriteCommand
    {
        uint64_t start_lba;

        struct
        {
            uint32_t block_num : 16;
            uint32_t reserved2 : 4;
            uint32_t directive_type : 4;
            uint32_t reserved : 2;
            uint32_t pr_info : 4;
            uint32_t force_unit_access : 1;
            uint32_t limited_retry : 1;
        } __attribute__((packed));
    };

    struct NvmeCommand
    {
        struct
        {
            uint32_t opcode : 8;
            uint32_t fuse : 2;
            uint32_t reserved : 4;
            uint32_t psdt : 2;
            uint32_t command_id : 16;
        } __attribute__((packed));

        uint32_t ns_id;
        uint64_t reserved2;
        uint64_t metadata_ptr;
        uint64_t prp1;
        uint64_t prp2;

        union
        {
            struct
            {
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

    struct NvmeCompletion
    {
        uint32_t dw0;
        uint32_t reserved;

        struct
        {
            uint32_t sq_head : 16;
            uint32_t sq_id : 16;
        } __attribute__((packed));

        struct
        {
            uint32_t command_id : 16;
            uint32_t phase_tag : 1;
            uint32_t status : 15;
        } __attribute__((packed));
    };

    class NvmeQueue
    {
        uint16_t queue_id = 0;

        uintptr_t completion_base{};
        uintptr_t submission_base{};

        NvmeCompletion* completion_queue{};
        NvmeCommand* submission_queue{};

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

        NvmeQueue(uint16_t qid, uintptr_t cq_base, uintptr_t sq_base, void* cq, void* sq, uint32_t* cq_db,
                  uint32_t* sq_db, uint16_t csz, uint16_t ssz);

        ~NvmeQueue() = default;

        NvmeQueue() = default;

        static long Consume(NvmeCommand& cmd);

        void Submit(NvmeCommand& cmd);

        void SubmitWait(NvmeCommand& cmd, NvmeCompletion& complet);

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

    enum AdminCommands
    {
        AdminCmdDeleteIOSubmissionQueue = 0x0,
        AdminCmdCreateIOSubmissionQueue = 0x1,
        AdminCmdGetLogPage = 0x2,
        AdminCmdDeleteIOCompletionQueue = 0x4,
        AdminCmdCreateIOCompletionQueue = 0x5,
        AdminCmdIdentify = 0x6,
        AdminCmdSetFeatures = 0x9,
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
