// elf.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 05.08.25.
//
// This file is part of VesperaOS.
// 
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef ELF_H
#define ELF_H

#include <cstdint>
#include <cstddef>
#include <vector.h>
struct kprocess_t;
#include "../proc/process_memory_manager.h"

/* These constants define the permissions on sections in the program
   header, p_flags. */
#define PF_R 0x4
#define PF_W 0x2
#define PF_X 0x1

#define	PT_NULL		0		/* Program header table entry unused */
#define PT_LOAD		1		/* Loadable program segment */
#define PT_DYNAMIC	2		/* Dynamic linking information */
#define PT_INTERP	3		/* Program interpreter */
#define PT_NOTE		4		/* Auxiliary information */
#define PT_SHLIB	5		/* Reserved */
#define PT_PHDR		6		/* Entry for header table itself */
#define PT_TLS		7		/* Thread-local storage segment */
#define	PT_NUM		8		/* Number of defined types */
#define PT_LOOS		0x60000000	/* Start of OS-specific */
#define PT_GNU_EH_FRAME	0x6474e550	/* GCC .eh_frame_hdr segment */
#define PT_GNU_STACK	0x6474e551	/* Indicates stack executability */
#define PT_GNU_RELRO	0x6474e552	/* Read-only after relocation */
#define PT_GNU_PROPERTY	0x6474e553	/* GNU property */
#define PT_GNU_SFRAME	0x6474e554	/* SFrame segment.  */
#define PT_LOSUNW	0x6ffffffa
#define PT_SUNWBSS	0x6ffffffa	/* Sun Specific segment */
#define PT_SUNWSTACK	0x6ffffffb	/* Stack segment */
#define PT_HISUNW	0x6fffffff
#define PT_HIOS		0x6fffffff	/* End of OS-specific */
#define PT_LOPROC	0x70000000	/* Start of processor-specific */
#define PT_HIPROC	0x7fffffff	/* End of processor-specific */

/* These constants define the various ELF target machines */
#define EM_NONE             0
#define EM_M32              1
#define EM_SPARC            2
#define EM_386              3
#define EM_68K              4
#define EM_88K              5
#define EM_486              6   /* Perhaps disused */
#define EM_860              7

#define EM_MIPS             8   /* MIPS R3000 (officially, big-endian only) */
#define EM_MIPS_RS4_BE      10  /* MIPS R4000 big-endian */
#define EM_PARISC           15  /* HPPA */
#define EM_SPARC32PLUS      18  /* Sun's "v8plus" */
#define EM_PPC              20  /* PowerPC */
#define EM_PPC64            21  /* PowerPC64 */
#define EM_ARM              40  /* ARM */
#define EM_SH               42  /* SuperH */
#define EM_SPARCV9          43  /* SPARC v9 64-bit */
#define EM_TRICORE          44  /* Infineon TriCore */
#define EM_IA_64            50  /* HP/Intel IA-64 */
#define EM_X86_64           62  /* AMD x86-64 */
#define EM_S390             22  /* IBM S/390 */
#define EM_CRIS             76  /* Axis Communications 32-bit embedded processor */
#define EM_AVR              83  /* AVR 8-bit microcontroller */
#define EM_V850             87  /* NEC v850 */
#define EM_H8_300H          47  /* Hitachi H8/300H */
#define EM_H8S              48  /* Hitachi H8S     */
#define EM_LATTICEMICO32    138 /* LatticeMico32 */
#define EM_OPENRISC         92  /* OpenCores OpenRISC */
#define EM_HEXAGON          164 /* Qualcomm Hexagon */
#define EM_RX               173 /* Renesas RX family */
#define EM_RISCV            243 /* RISC-V */
#define EM_NANOMIPS         249 /* Wave Computing nanoMIPS */
#define EM_LOONGARCH        258 /* LoongArch */


/* These constants define the different elf file types */
#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff


struct Elf64_Ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    // ...
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

class ElfLoader {
public:
    struct ElfLoadResult {
        uint64_t entry_point;
        bool success;
        const char *error_message;
    };

    ElfLoadResult load_elf_binary(const char *path, uintptr_t USERBASE, ProcessMemoryManager *mem);

private:
    struct ElfSegment {
        void *vaddr;
        void *data_ptr;
        size_t file_size;
        size_t memory_size;
        uint64_t flags;
    };

    struct ElfFileData {
        void *data;
        size_t size;
        const char *error_message;
    };

    struct SegmentMapping {
        uintptr_t page_start;
        size_t page_offset;
        size_t map_size;
        size_t file_size;
        size_t memory_size;
    };

    static bool validate_elf_header(const Elf64_Ehdr *header);

    static ElfFileData load_file_from_vfs(const char *path);

    static ElfLoadResult validate_elf_file(void *file_data);

    static SegmentMapping calculate_segment_mapping(const Elf64_Phdr &ph, uintptr_t base_addr);

    static ElfLoadResult map_and_load_segment(const Elf64_Phdr &ph, const void *file_data, uintptr_t base_addr,
                                              ProcessMemoryManager *mem);

    static ElfLoadResult
    process_loadable_segments(const void *file_data, const Elf64_Ehdr *header, uintptr_t base_addr,
                              ProcessMemoryManager *mem);
};

#endif //ELF_H
