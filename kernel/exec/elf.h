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

#include <vector.h>

/* Type for a 16-bit quantity.  */
typedef uint16_t Elf32_Half;
typedef uint16_t Elf64_Half;

/* Types for signed and unsigned 32-bit quantities.  */
typedef uint32_t Elf32_Word;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;

/* Types for signed and unsigned 64-bit quantities.  */
typedef uint64_t Elf32_Xword;
typedef int64_t Elf32_Sxword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

/* Type of addresses.  */
typedef uint32_t Elf32_Addr;
typedef uint64_t Elf64_Addr;

/* Type of file offsets.  */
typedef uint32_t Elf32_Off;
typedef uint64_t Elf64_Off;

/* Type for section indices, which are 16-bit quantities.  */
typedef uint16_t Elf32_Section;
typedef uint16_t Elf64_Section;

/* Type for version symbol information.  */
typedef Elf32_Half Elf32_Versym;
typedef Elf64_Half Elf64_Versym;


#define EI_NIDENT (16)

typedef struct
{
    unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
    Elf32_Half e_type; /* Object file type */
    Elf32_Half e_machine; /* Architecture */
    Elf32_Word e_version; /* Object file version */
    Elf32_Addr e_entry; /* Entry point virtual address */
    Elf32_Off e_phoff; /* Program header table file offset */
    Elf32_Off e_shoff; /* Section header table file offset */
    Elf32_Word e_flags; /* Processor-specific flags */
    Elf32_Half e_ehsize; /* ELF header size in bytes */
    Elf32_Half e_phentsize; /* Program header table entry size */
    Elf32_Half e_phnum; /* Program header table entry count */
    Elf32_Half e_shentsize; /* Section header table entry size */
    Elf32_Half e_shnum; /* Section header table entry count */
    Elf32_Half e_shstrndx; /* Section header string table index */
} Elf32_Ehdr;

typedef struct
{
    unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
    Elf64_Half e_type; /* Object file type */
    Elf64_Half e_machine; /* Architecture */
    Elf64_Word e_version; /* Object file version */
    Elf64_Addr e_entry; /* Entry point virtual address */
    Elf64_Off e_phoff; /* Program header table file offset */
    Elf64_Off e_shoff; /* Section header table file offset */
    Elf64_Word e_flags; /* Processor-specific flags */
    Elf64_Half e_ehsize; /* ELF header size in bytes */
    Elf64_Half e_phentsize; /* Program header table entry size */
    Elf64_Half e_phnum; /* Program header table entry count */
    Elf64_Half e_shentsize; /* Section header table entry size */
    Elf64_Half e_shnum; /* Section header table entry count */
    Elf64_Half e_shstrndx; /* Section header string table index */
} Elf64_Ehdr;


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

/* Fields in the e_ident array.  The EI_* macros are indices into the
   array.  The macros under each EI_* macro are the values the byte
   may have.  */

#define EI_MAG0		0		/* File identification byte 0 index */
#define ELFMAG0		0x7f		/* Magic number byte 0 */

#define EI_MAG1		1		/* File identification byte 1 index */
#define ELFMAG1		'E'		/* Magic number byte 1 */

#define EI_MAG2		2		/* File identification byte 2 index */
#define ELFMAG2		'L'		/* Magic number byte 2 */

#define EI_MAG3		3		/* File identification byte 3 index */
#define ELFMAG3		'F'		/* Magic number byte 3 */

/* Conglomeration of the identification bytes, for easy testing as a word.  */
#define	ELFMAG		"\177ELF"


/* Legal values for e_type (object file type).  */

#define ET_NONE		0		/* No file type */
#define ET_REL		1		/* Relocatable file */
#define ET_EXEC		2		/* Executable file */
#define ET_DYN		3		/* Shared object file */
#define ET_CORE		4		/* Core file */
#define	ET_NUM		5		/* Number of defined types */
#define ET_LOOS		0xfe00		/* OS-specific range start */
#define ET_HIOS		0xfeff		/* OS-specific range end */
#define ET_LOPROC	0xff00		/* Processor-specific range start */
#define ET_HIPROC	0xffff		/* Processor-specific range end */

/* Relocation table entry with addend (in section of type SHT_RELA).  */

typedef struct
{
    Elf32_Addr r_offset; /* Address */
    Elf32_Word r_info; /* Relocation type and symbol index */
    Elf32_Sword r_addend; /* Addend */
} Elf32_Rela;

typedef struct
{
    Elf64_Addr r_offset; /* Address */
    Elf64_Xword r_info; /* Relocation type and symbol index */
    Elf64_Sxword r_addend; /* Addend */
} Elf64_Rela;

/* RELR relocation table entry */

typedef Elf32_Word Elf32_Relr;
typedef Elf64_Xword Elf64_Relr;

/* How to extract and insert information held in the r_info field.  */

#define ELF32_R_SYM(val)		((val) >> 8)
#define ELF32_R_TYPE(val)		((val) & 0xff)
#define ELF32_R_INFO(sym, type)		(((sym) << 8) + ((type) & 0xff))

#define ELF64_R_SYM(i)			((i) >> 32)
#define ELF64_R_TYPE(i)			((i) & 0xffffffff)
#define ELF64_R_INFO(sym,type)		((((Elf64_Xword) (sym)) << 32) + (type))


typedef struct
{
    Elf32_Word p_type; /* Segment type */
    Elf32_Off p_offset; /* Segment file offset */
    Elf32_Addr p_vaddr; /* Segment virtual address */
    Elf32_Addr p_paddr; /* Segment physical address */
    Elf32_Word p_filesz; /* Segment size in file */
    Elf32_Word p_memsz; /* Segment size in memory */
    Elf32_Word p_flags; /* Segment flags */
    Elf32_Word p_align; /* Segment alignment */
} Elf32_Phdr;

typedef struct
{
    Elf64_Word p_type; /* Segment type */
    Elf64_Word p_flags; /* Segment flags */
    Elf64_Off p_offset; /* Segment file offset */
    Elf64_Addr p_vaddr; /* Segment virtual address */
    Elf64_Addr p_paddr; /* Segment physical address */
    Elf64_Xword p_filesz; /* Segment size in file */
    Elf64_Xword p_memsz; /* Segment size in memory */
    Elf64_Xword p_align; /* Segment alignment */
} Elf64_Phdr;

/* Legal values for p_type (segment type).  */

#define PT_NULL         0               /* Program header table entry unused */
#define PT_LOAD         1               /* Loadable program segment */
#define PT_DYNAMIC      2               /* Dynamic linking information */
#define PT_INTERP       3               /* Program interpreter */
#define PT_NOTE         4               /* Auxiliary information */
#define PT_SHLIB        5               /* Reserved */
#define PT_PHDR         6               /* Entry for header table itself */
#define PT_TLS          7               /* Thread-local storage segment */
#define PT_NUM          8               /* Number of defined types */
#define PT_LOOS         0x60000000      /* Start of OS-specific */
#define PT_GNU_EH_FRAME 0x6474e550      /* GCC .eh_frame_hdr segment */
#define PT_GNU_STACK    0x6474e551      /* Indicates stack executability */
#define PT_GNU_RELRO    0x6474e552      /* Read-only after relocation */
#define PT_GNU_PROPERTY 0x6474e553      /* GNU property */
#define PT_GNU_SFRAME   0x6474e554      /* SFrame segment.  */
#define PT_LOSUNW       0x6ffffffa
#define PT_SUNWBSS      0x6ffffffa      /* Sun Specific segment */
#define PT_SUNWSTACK    0x6ffffffb      /* Stack segment */
#define PT_HISUNW       0x6fffffff
#define PT_HIOS         0x6fffffff      /* End of OS-specific */
#define PT_LOPROC       0x70000000      /* Start of processor-specific */
#define PT_HIPROC       0x7fffffff      /* End of processor-specific */

/* Legal values for p_flags (segment flags).  */

#define PF_X            (1 << 0)        /* Segment is executable */
#define PF_W            (1 << 1)        /* Segment is writable */
#define PF_R            (1 << 2)        /* Segment is readable */
#define PF_MASKOS       0x0ff00000      /* OS-specific */
#define PF_MASKPROC     0xf0000000      /* Processor-specific */

/* Dynamic section entry.  */

typedef struct
{
    Elf32_Sword d_tag; /* Dynamic entry type */
    union
    {
        Elf32_Word d_val; /* Integer value */
        Elf32_Addr d_ptr; /* Address value */
    } d_un;
} Elf32_Dyn;

typedef struct
{
    Elf64_Sxword d_tag; /* Dynamic entry type */
    union
    {
        Elf64_Xword d_val; /* Integer value */
        Elf64_Addr d_ptr; /* Address value */
    } d_un;
} Elf64_Dyn;

/* Legal values for d_tag (dynamic entry type).  */

#define DT_NULL		0		/* Marks end of dynamic section */
#define DT_NEEDED	1		/* Name of needed library */
#define DT_PLTRELSZ	2		/* Size in bytes of PLT relocs */
#define DT_PLTGOT	3		/* Processor defined value */
#define DT_HASH		4		/* Address of symbol hash table */
#define DT_STRTAB	5		/* Address of string table */
#define DT_SYMTAB	6		/* Address of symbol table */
#define DT_RELA		7		/* Address of Rela relocs */
#define DT_RELASZ	8		/* Total size of Rela relocs */
#define DT_RELAENT	9		/* Size of one Rela reloc */
#define DT_STRSZ	10		/* Size of string table */
#define DT_SYMENT	11		/* Size of one symbol table entry */
#define DT_INIT		12		/* Address of init function */
#define DT_FINI		13		/* Address of termination function */
#define DT_SONAME	14		/* Name of shared object */
#define DT_RPATH	15		/* Library search path (deprecated) */
#define DT_SYMBOLIC	16		/* Start symbol search here */
#define DT_REL		17		/* Address of Rel relocs */
#define DT_RELSZ	18		/* Total size of Rel relocs */
#define DT_RELENT	19		/* Size of one Rel reloc */
#define DT_PLTREL	20		/* Type of reloc in PLT */
#define DT_DEBUG	21		/* For debugging; unspecified */
#define DT_TEXTREL	22		/* Reloc might modify .text */
#define DT_JMPREL	23		/* Address of PLT relocs */
#define	DT_BIND_NOW	24		/* Process relocations of object */
#define	DT_INIT_ARRAY	25		/* Array with addresses of init fct */
#define	DT_FINI_ARRAY	26		/* Array with addresses of fini fct */
#define	DT_INIT_ARRAYSZ	27		/* Size in bytes of DT_INIT_ARRAY */
#define	DT_FINI_ARRAYSZ	28		/* Size in bytes of DT_FINI_ARRAY */
#define DT_RUNPATH	29		/* Library search path */
#define DT_FLAGS	30		/* Flags for the object being loaded */
#define DT_ENCODING	32		/* Start of encoded range */
#define DT_PREINIT_ARRAY 32		/* Array with addresses of preinit fct*/
#define DT_PREINIT_ARRAYSZ 33		/* size in bytes of DT_PREINIT_ARRAY */
#define DT_SYMTAB_SHNDX	34		/* Address of SYMTAB_SHNDX section */
#define DT_RELRSZ	35		/* Total size of RELR relative relocations */
#define DT_RELR		36		/* Address of RELR relative relocations */
#define DT_RELRENT	37		/* Size of one RELR relative relocaction */
#define	DT_NUM		38		/* Number used */
#define DT_LOOS		0x6000000d	/* Start of OS-specific */
#define DT_HIOS		0x6ffff000	/* End of OS-specific */
#define DT_LOPROC	0x70000000	/* Start of processor-specific */
#define DT_HIPROC	0x7fffffff	/* End of processor-specific */
#define	DT_PROCNUM	DT_MIPS_NUM	/* Most used by any processor */


/* AMD x86-64 relocations.  */
#define R_X86_64_NONE		0	/* No reloc */
#define R_X86_64_64		1	/* Direct 64 bit  */
#define R_X86_64_PC32		2	/* PC relative 32 bit signed */
#define R_X86_64_GOT32		3	/* 32 bit GOT entry */
#define R_X86_64_PLT32		4	/* 32 bit PLT address */
#define R_X86_64_COPY		5	/* Copy symbol at runtime */
#define R_X86_64_GLOB_DAT	6	/* Create GOT entry */
#define R_X86_64_JUMP_SLOT	7	/* Create PLT entry */
#define R_X86_64_RELATIVE	8	/* Adjust by program base */
#define R_X86_64_GOTPCREL	9	/* 32 bit signed PC relative
					   offset to GOT */
#define R_X86_64_32		10	/* Direct 32 bit zero extended */
#define R_X86_64_32S		11	/* Direct 32 bit sign extended */
#define R_X86_64_16		12	/* Direct 16 bit zero extended */
#define R_X86_64_PC16		13	/* 16 bit sign extended pc relative */
#define R_X86_64_8		14	/* Direct 8 bit sign extended  */
#define R_X86_64_PC8		15	/* 8 bit sign extended pc relative */
#define R_X86_64_DTPMOD64	16	/* ID of module containing symbol */
#define R_X86_64_DTPOFF64	17	/* Offset in module's TLS block */
#define R_X86_64_TPOFF64	18	/* Offset in initial TLS block */
#define R_X86_64_TLSGD		19	/* 32 bit signed PC relative offset
					   to two GOT entries for GD symbol */
#define R_X86_64_TLSLD		20	/* 32 bit signed PC relative offset
					   to two GOT entries for LD symbol */
#define R_X86_64_DTPOFF32	21	/* Offset in TLS block */
#define R_X86_64_GOTTPOFF	22	/* 32 bit signed PC relative offset
					   to GOT entry for IE symbol */
#define R_X86_64_TPOFF32	23	/* Offset in initial TLS block */
#define R_X86_64_PC64		24	/* PC relative 64 bit */
#define R_X86_64_GOTOFF64	25	/* 64 bit offset to GOT */
#define R_X86_64_GOTPC32	26	/* 32 bit signed pc relative
					   offset to GOT */
#define R_X86_64_GOT64		27	/* 64-bit GOT entry offset */
#define R_X86_64_GOTPCREL64	28	/* 64-bit PC relative offset
					   to GOT entry */
#define R_X86_64_GOTPC64	29	/* 64-bit PC relative offset to GOT */
#define R_X86_64_GOTPLT64	30 	/* like GOT64, says PLT entry needed */
#define R_X86_64_PLTOFF64	31	/* 64-bit GOT relative offset
					   to PLT entry */
#define R_X86_64_SIZE32		32	/* Size of symbol plus 32-bit addend */
#define R_X86_64_SIZE64		33	/* Size of symbol plus 64-bit addend */
#define R_X86_64_GOTPC32_TLSDESC 34	/* GOT offset for TLS descriptor.  */
#define R_X86_64_TLSDESC_CALL   35	/* Marker for call through TLS
					   descriptor.  */
#define R_X86_64_TLSDESC        36	/* TLS descriptor.  */
#define R_X86_64_IRELATIVE	37	/* Adjust indirectly by program base */
#define R_X86_64_RELATIVE64	38	/* 64-bit adjust by program base */
/* 39 Reserved was R_X86_64_PC32_BND */
/* 40 Reserved was R_X86_64_PLT32_BND */
#define R_X86_64_GOTPCRELX	41	/* Load from 32 bit signed pc relative
					   offset to GOT entry without REX
					   prefix, relaxable.  */
#define R_X86_64_REX_GOTPCRELX	42	/* Load from 32 bit signed pc relative
					   offset to GOT entry with REX prefix,
					   relaxable.  */
#define R_X86_64_NUM		43

class Realm;

class ElfLoader
{
public:
    struct LoadResult
    {
        void* entry_point;
        uintptr_t load_base;
        uintptr_t load_end;
        uintptr_t vaddr_base;
        uintptr_t load_bias;
        bool success;
        const char* error_message;

        bool is_pie;
    };

    static LoadResult load(const char* path, uintptr_t preferred_base, const Realm* realm);
    static bool apply_relocations(const Elf64_Ehdr* header, const void* file_data, uintptr_t load_bias);

private:
    struct ElfSegment
    {
        void* vaddr;
        void* data_ptr;
        size_t file_size;
        size_t memory_size;
        uint64_t flags;
    };

    struct FileData
    {
        void* data;
        size_t size;
        const char* error_message;
    };

    struct AddressRange
    {
        uintptr_t vaddr_min;
        uintptr_t vaddr_max;
        size_t total_size;
    };

    struct SegmentMapping
    {
        const uintptr_t page_start;
        const size_t page_offset;
        const size_t map_size;
        const size_t file_size;
        const size_t memory_size;
    };

    static bool validate_elf_header(const Elf64_Ehdr* header);

    static bool validate_magic(const Elf64_Ehdr* header);
    static bool validate_type(const Elf64_Ehdr* header);
    static bool validate_architecture(const Elf64_Ehdr* header);
    static FileData load_file_from_vfs(const char* path);
    static bool calculate_address_range(const Elf64_Ehdr* header, const void* file_data, AddressRange& range);

    static uintptr_t calculate_load_bias(const Elf64_Ehdr* header, const AddressRange& range, uintptr_t preferred_base);
    static SegmentMapping calculate_segment_mapping(const Elf64_Phdr& ph, uintptr_t base_addr);
    static bool load_segment(const Elf64_Phdr& phdr, const void* file_data, uintptr_t load_bias, const Realm* realm);
    static bool process_all_segments(const Elf64_Ehdr* header, const void* file_data, uintptr_t load_bias,
                                     const Realm* realm);
    static uintptr_t align_down(uintptr_t v, size_t align);
    static uintptr_t align_up(uintptr_t v, size_t align);
};

#endif //ELF_H
