/**
 * @file ata.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 11.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef VESPERAOS_ATA_H
#define VESPERAOS_ATA_H

#include <vespera/types.h>

struct IDENTIFY_DEVICE_DATA {
    struct {
        u16 reserved1 : 1;
        u16 retired3 : 1;
        u16 response_incomplete : 1;
        u16 retired2 : 3;
        u16 fixed_device : 1;
        u16 removable_media : 1;
        u16 retired1 : 7;
        u16 device_type : 1;
    } general_configuration;

    u16 num_cylinders;
    u16 specific_configuration;
    u16 num_heads;
    u16 retired1[2];
    u16 num_sectors_per_track;
    u16 vendor_unique1[3];
    u8 serial_number[20];
    u16 retired2[2];
    u16 obsolete1;
    u8 firmware_revision[8];
    u8 model_number[40];
    u8 maximum_block_transfer;
    u8 vendor_unique2;

    struct {
        u16 feature_supported : 1;
        u16 reserved : 15;
    } trusted_computing;

    struct {
        u8 current_long_physical_sector_alignment : 2;
        u8 reserved_byte49 : 6;
        u8 dma_supported : 1;
        u8 lba_supported : 1;
        u8 iordy_disable : 1;
        u8 iordy_supported : 1;
        u8 reserved1 : 1;
        u8 standby_timer_support : 1;
        u8 reserved2 : 2;
        u16 reserved_word50;
    } capabilities;

    u16 obsolete_words51[2];

    struct {
        u16 translation_fields_valid : 3;
        u16 reserved3 : 5;
        u16 free_fall_control_sensitivity : 8;
    };

    u16 number_of_current_cylinders;
    u16 number_of_current_heads;
    u16 current_sectors_per_track;
    u32 current_sector_capacity;
    u8 current_multi_sector_setting;

    struct {
        u8 multi_sector_setting_valid : 1;
        u8 reserved_byte59 : 3;
        u8 sanitize_feature_supported : 1;
        u8 crypto_scramble_ext_command_supported : 1;
        u8 overwrite_ext_command_supported : 1;
        u8 block_erase_ext_command_supported : 1;
    };

    u32 user_addressable_sectors;
    u16 obsolete_word62;

    struct {
        u16 multi_word_dma_support : 8;
        u16 multi_word_dma_active : 8;
    };

    struct {
        u16 advanced_pio_modes : 8;
        u16 reserved_byte64 : 8;
    };

    u16 minimum_mw_xfer_cycle_time;
    u16 recommended_mw_xfer_cycle_time;
    u16 minimum_pio_cycle_time;
    u16 minimum_pio_cycle_time_iordy;

    struct {
        u16 zoned_capabilities : 2;
        u16 non_volatile_write_cache : 1;
        u16 extended_user_addressable_sectors_supported : 1;
        u16 device_encrypts_all_user_data : 1;
        u16 read_zero_after_trim_supported : 1;
        u16 optional28_bit_commands_supported : 1;
        u16 ieee1667 : 1;
        u16 download_microcode_dma_supported : 1;
        u16 set_max_set_password_unlock_dma_supported : 1;
        u16 write_buffer_dma_supported : 1;
        u16 read_buffer_dma_supported : 1;
        u16 device_config_identify_set_dma_supported : 1;
        u16 lpsaerc_supported : 1;
        u16 deterministic_read_after_trim_supported : 1;
        u16 c_fast_spec_supported : 1;
    } additional_supported;

    u16 reserved_words70[5];

    struct {
        u16 queue_depth : 5;
        u16 reserved_word75 : 11;
    };

    struct {
        u16 reserved0 : 1;
        u16 sata_gen1 : 1;
        u16 sata_gen2 : 1;
        u16 sata_gen3 : 1;
        u16 reserved1 : 4;
        u16 ncq : 1;
        u16 hipm : 1;
        u16 phy_events : 1;
        u16 ncq_unload : 1;
        u16 ncq_priority : 1;
        u16 host_auto_ps : 1;
        u16 device_auto_ps : 1;
        u16 read_log_dma : 1;
        u16 reserved2 : 1;
        u16 current_speed : 3;
        u16 ncq_streaming : 1;
        u16 ncq_queue_mgmt : 1;
        u16 ncq_receive_send : 1;
        u16 devsl_pto_reduced_pwr_state : 1;
        u16 reserved3 : 8;
    } serial_ata_capabilities;

    struct {
        u16 reserved0 : 1;
        u16 non_zero_offsets : 1;
        u16 dma_setup_auto_activate : 1;
        u16 dipm : 1;
        u16 in_order_data : 1;
        u16 hardware_feature_control : 1;
        u16 software_settings_preservation : 1;
        u16 ncq_autosense : 1;
        u16 devslp : 1;
        u16 hybrid_information : 1;
        u16 reserved1 : 6;
    } serial_ata_features_supported;

    struct {
        u16 reserved0 : 1;
        u16 non_zero_offsets : 1;
        u16 dma_setup_auto_activate : 1;
        u16 dipm : 1;
        u16 in_order_data : 1;
        u16 hardware_feature_control : 1;
        u16 software_settings_preservation : 1;
        u16 device_auto_ps : 1;
        u16 devslp : 1;
        u16 hybrid_information : 1;
        u16 reserved1 : 6;
    } serial_ata_features_enabled;

    u16 major_revision;
    u16 minor_revision;

    struct {
        u16 smart_commands : 1;
        u16 security_mode : 1;
        u16 removable_media_feature : 1;
        u16 power_management : 1;
        u16 reserved1 : 1;
        u16 write_cache : 1;
        u16 look_ahead : 1;
        u16 release_interrupt : 1;
        u16 service_interrupt : 1;
        u16 device_reset : 1;
        u16 host_protected_area : 1;
        u16 obsolete1 : 1;
        u16 write_buffer : 1;
        u16 read_buffer : 1;
        u16 nop : 1;
        u16 obsolete2 : 1;
        u16 download_microcode : 1;
        u16 dma_queued : 1;
        u16 cfa : 1;
        u16 advanced_pm : 1;
        u16 msn : 1;
        u16 power_up_in_standby : 1;
        u16 manual_power_up : 1;
        u16 reserved2 : 1;
        u16 set_max : 1;
        u16 acoustics : 1;
        u16 big_lba : 1;
        u16 device_config_overlay : 1;
        u16 flush_cache : 1;
        u16 flush_cache_ext : 1;
        u16 word_valid83 : 2;
        u16 smart_error_log : 1;
        u16 smart_self_test : 1;
        u16 media_serial_number : 1;
        u16 media_card_pass_through : 1;
        u16 streaming_feature : 1;
        u16 gp_logging : 1;
        u16 write_fua : 1;
        u16 write_queued_fua : 1;
        u16 wwn64_bit : 1;
        u16 urg_read_stream : 1;
        u16 urg_write_stream : 1;
        u16 reserved_for_tech_report : 2;
        u16 idle_with_unload_feature : 1;
        u16 word_valid : 2;
    } command_set_support;

    struct {
        u16 smart_commands : 1;
        u16 security_mode : 1;
        u16 removable_media_feature : 1;
        u16 power_management : 1;
        u16 reserved1 : 1;
        u16 write_cache : 1;
        u16 look_ahead : 1;
        u16 release_interrupt : 1;
        u16 service_interrupt : 1;
        u16 device_reset : 1;
        u16 host_protected_area : 1;
        u16 obsolete1 : 1;
        u16 write_buffer : 1;
        u16 read_buffer : 1;
        u16 nop : 1;
        u16 obsolete2 : 1;
        u16 download_microcode : 1;
        u16 dma_queued : 1;
        u16 cfa : 1;
        u16 advanced_pm : 1;
        u16 msn : 1;
        u16 power_up_in_standby : 1;
        u16 manual_power_up : 1;
        u16 reserved2 : 1;
        u16 set_max : 1;
        u16 acoustics : 1;
        u16 big_lba : 1;
        u16 device_config_overlay : 1;
        u16 flush_cache : 1;
        u16 flush_cache_ext : 1;
        u16 resrved3 : 1;
        u16 words119_120_valid : 1;
        u16 smart_error_log : 1;
        u16 smart_self_test : 1;
        u16 media_serial_number : 1;
        u16 media_card_pass_through : 1;
        u16 streaming_feature : 1;
        u16 gp_logging : 1;
        u16 write_fua : 1;
        u16 write_queued_fua : 1;
        u16 wwn64_bit : 1;
        u16 urg_read_stream : 1;
        u16 urg_write_stream : 1;
        u16 reserved_for_tech_report : 2;
        u16 idle_with_unload_feature : 1;
        u16 reserved4 : 2;
    } command_set_active;

    struct {
        u16 ultra_dma_support : 8;
        u16 ultra_dma_active : 8;
    };

    struct {
        u16 time_required : 15;
        u16 extended_time_reported : 1;
    } normal_security_erase_unit;

    struct {
        u16 time_required : 15;
        u16 extended_time_reported : 1;
    } enhanced_security_erase_unit;

    u16 current_apm_level : 8;
    u16 reserved_word91 : 8;
    u16 master_password_id;
    u16 hardware_reset_result;
    u16 current_acoustic_value : 8;
    u16 recommended_acoustic_value : 8;

    u16 stream_min_request_size;
    u16 streaming_transfer_time_dma;
    u16 streaming_access_latency_dmapio;
    u32 streaming_perf_granularity;
    u32 max48_bit_lba[2];
    u16 streaming_transfer_time;
    u16 dsm_cap;

    struct {
        u16 logical_sectors_per_physical_sector : 4;
        u16 reserved0 : 8;
        u16 logical_sector_longer_than256_words : 1;
        u16 multiple_logical_sectors_per_physical_sector : 1;
        u16 reserved1 : 2;
    } physical_logical_sector_size;

    u16 inter_seek_delay;
    u16 world_wide_name[4];
    u16 reserved_for_world_wide_name128[4];
    u16 reserved_for_tlc_technical_report;
    u16 words_per_logical_sector[2];

    struct {
        u16 reserved_for_drq_technical_report : 1;
        u16 write_read_verify : 1;
        u16 write_uncorrectable_ext : 1;
        u16 read_write_log_dma_ext : 1;
        u16 download_microcode_mode3 : 1;
        u16 freefall_control : 1;
        u16 sense_data_reporting : 1;
        u16 extended_power_conditions : 1;
        u16 reserved0 : 6;
        u16 word_valid : 2;
    } command_set_support_ext;

    struct {
        u16 reserved_for_drq_technical_report : 1;
        u16 write_read_verify : 1;
        u16 write_uncorrectable_ext : 1;
        u16 read_write_log_dma_ext : 1;
        u16 download_microcode_mode3 : 1;
        u16 freefall_control : 1;
        u16 sense_data_reporting : 1;
        u16 extended_power_conditions : 1;
        u16 reserved0 : 6;
        u16 reserved1 : 2;
    } command_set_active_ext;

    u16 reserved_for_expanded_supportand_active[6];

    struct {
        u16 msn_support : 2;
        u16 reserved_word127 : 14;
    };

    struct {
        u16 security_supported : 1;
        u16 security_enabled : 1;
        u16 security_locked : 1;
        u16 security_frozen : 1;
        u16 security_count_expired : 1;
        u16 enhanced_security_erase_supported : 1;
        u16 reserved0 : 2;
        u16 security_level : 1;
        u16 reserved1 : 7;
    } security_status;

    u16 reserved_word129[31];

    struct {
        u16 maximum_current_in_ma : 12;
        u16 cfa_power_mode_1disabled : 1;
        u16 cfa_power_mode1_required : 1;
        u16 reserved0 : 1;
        u16 word160_supported : 1;
    } cfa_power_mode1;

    u16 reserved_for_cfa_word161[7];

    u16 nominal_form_factor : 4;
    u16 reserved_word168 : 12;

    struct {
        u16 supports_trim : 1;
        u16 reserved0 : 15;
    } data_set_management_feature;

    u16 additional_product_id[4];
    u16 reserved_for_cfa_word174[2];
    u16 current_media_serial_number[30];

    struct {
        u16 supported : 1;
        u16 reserved0 : 1;
        u16 write_same_suported : 1;
        u16 error_recovery_control_supported : 1;
        u16 feature_control_suported : 1;
        u16 data_tables_suported : 1;
        u16 reserved1 : 6;
        u16 vendor_specific : 4;
    } sct_command_transport;

    u16 reserved_word207[2];

    struct {
        u16 alignment_of_logical_within_physical : 14;
        u16 word209_supported : 1;
        u16 reserved0 : 1;
    } block_alignment;

    u16 write_read_verify_sector_count_mode3_only[2];
    u16 write_read_verify_sector_count_mode2_only[2];

    struct {
        u16 nv_cache_power_mode_enabled : 1;
        u16 reserved0 : 3;
        u16 nv_cache_feature_set_enabled : 1;
        u16 reserved1 : 3;
        u16 nv_cache_power_mode_version : 4;
        u16 nv_cache_feature_set_version : 4;
    } nv_cache_capabilities;

    u16 nv_cache_size_lsw;
    u16 nv_cache_size_msw;
    u16 nominal_media_rotation_rate;
    u16 reserved_word218;

    struct {
        u8 nv_cache_estimated_time_to_spin_up_in_seconds;
        u8 reserved;
    } nv_cache_options;

    u16 write_read_verify_sector_count_mode : 8;
    u16 reserved_word220 : 8;
    u16 reserved_word221;

    struct {
        u16 major_version : 12;
        u16 transport_type : 4;
    } transport_major_version;

    u16 transport_minor_version;
    u16 reserved_word224[6];

    u32 extended_number_of_user_addressable_sectors[2];
    u16 min_blocks_per_download_microcode_mode03;
    u16 max_blocks_per_download_microcode_mode03;

    u16 reserved_word236[19];

    struct {
        u16 signature : 8;
        u16 check_sum : 8;
    };
} __attribute__((packed));

struct ATA_SMART_ATTRIBUTE {
    u8 id;
    u16 flags;
    u8 current;
    u8 worst;
    u8 raw[6];
    u8 reserved;
} __attribute__((packed));

struct ATA_SMART_ATTRIBUTES_AREA {
    u16 version;                         // Byte 0-1
    ATA_SMART_ATTRIBUTE attributes[30];  // Byte 2-361 (30 * 12 = 360 Bytes)
} __attribute__((packed));

struct ATA_SMART_DATA {
    ATA_SMART_ATTRIBUTES_AREA vendor_specific_0;  // Byte 0-361:   Vendor specific (inkl. Attribute structs)
    u8 offline_collection_status;                 // Byte 362:     V - Off-line data collection status
    u8 self_test_execution_status;                // Byte 363:     X - Self-test execution status byte
    u8 vendor_specific_1[2];                      // Byte 364-365: X - Vendor specific
    u8 vendor_specific_2;                         // Byte 366:     X - Vendor specific
    u8 offline_collection_capability;             // Byte 367:     F - Off-line data collection capability
    u16 smart_capability;                         // Byte 368-369: F - SMART capability
    u8 error_logging_capability;                  // Byte 370:     F - Error logging capability
                                                  //               Bit 0: 1=Device error logging supported
                                                  //               Bit 7:1 Reserved
    u8 vendor_specific_3;                         // Byte 371:     X - Vendor specific
    u8 short_self_test_polling_minutes;  // Byte 372:     F - Short self-test routine recommended polling time (minutes)
    u8 extended_self_test_polling_minutes;    // Byte 373:     F - Extended self-test polling time (minutes), FFh = use
                                              // word at 375-376
    u8 conveyance_self_test_polling_minutes;  // Byte 374:     F - Conveyance self-test routine recommended polling time
                                              // (minutes)
    u16 extended_self_test_polling_minutes_word;  // Byte 375-376: F - Extended self-test routine recommended polling
                                                  // time (word)
    u8 reserved[9];                               // Byte 377-385: R - Reserved, shall be zero
    u8 vendor_specific_4[125];                    // Byte 386-510: X - Vendor specific
    u8 checksum;                                  // Byte 511:     V - Data structure checksum
} __attribute__((packed));

#endif  // VESPERAOS_ATA_H
