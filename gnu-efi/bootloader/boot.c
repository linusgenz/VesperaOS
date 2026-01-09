#include "../inc/efi.h"
#include "../inc/efilib.h"
#include <elf.h>
#include "../../include/graphics.h"

struct BootInfo;
typedef void (*KernelEntry)(struct BootInfo*);


typedef struct BootInfo
{
    Framebuffer* fb;
    FONT* font;
    EFI_MEMORY_DESCRIPTOR* mmap;
    UINTN mmap_size;
    UINTN mmap_desc_size;
    VOID* rsdp;
} BootInfo;

typedef struct BootstrapInfo
{
    BootInfo bi;
    void* phys_start;
    UINTN phys_size;
    KernelEntry kernel_entry;
} BootstrapInfo;

#define BOOTSTRAP_PATH L"\\bootstrap.elf"
#define KERNEL_PATH L"\\kernel.elf"
Framebuffer framebuffer;

EFI_FILE* LoadFile(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SysTbl)
{
    EFI_FILE* LoadedFile;

    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    SysTbl->BootServices->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
    SysTbl->BootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid,
                                         (void**)&FileSystem);

    if (Directory == NULL)
    {
        FileSystem->OpenVolume(FileSystem, &Directory);
    }

    EFI_STATUS s = Directory->Open(Directory, &LoadedFile, Path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
    if (s != EFI_SUCCESS)
    {
        return NULL;
    }
    return LoadedFile;
}

int memcmp(const void* aptr, const unsigned char* bptr, size_t n)
{
    const unsigned char *a = aptr, *b = bptr;
    for (size_t i = 0; i < n; i++)
    {
        if (a[i] < b[i]) return -1;
        else if (a[i] > b[i]) return 1;
    }
    return 0;
}

UINTN strcmp(CHAR8* a, CHAR8* b, UINTN length)
{
    for (UINTN i = 0; i < length; i++)
    {
        if (*a != *b) return 0;
    }
    return 1;
}

Framebuffer* InitializeGOP()
{
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    EFI_STATUS status;

    status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (void**)&gop);
    if (EFI_ERROR(status))
    {
        Print(L"Unable to locate GOP\n\r");
        return NULL;
    }
    else
    {
        Print(L"GOP located\n\r");
    }

    framebuffer.base_address = (void*)gop->Mode->FrameBufferBase;
    framebuffer.buffer_size = gop->Mode->FrameBufferSize;
    framebuffer.width = gop->Mode->Info->HorizontalResolution;
    framebuffer.height = gop->Mode->Info->VerticalResolution;
    framebuffer.pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;

    Print(L"Framebuffer address %p size %d, width %d height %d pixelsperline %d\r\n",
          framebuffer.base_address,
          framebuffer.buffer_size,
          framebuffer.width,
          framebuffer.height,
          framebuffer.pixels_per_scanline
    );

    if (gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
    {
        Print(L"Pixel format is RGB 8:8:8\n\r");
    }
    else if (gop->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor)
    {
        Print(L"Pixel format is BGR 8:8:8\n\r");
    }
    else
    {
        Print(L"Unknown pixel format\n\r");
    }

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
    UINTN SizeOfInfo;
    status = gop->QueryMode(gop, gop->Mode->Mode, &SizeOfInfo, &info);

    if (status == EFI_SUCCESS)
    {
        Print(L"Current mode: %dx%d\r\n", info->HorizontalResolution, info->VerticalResolution);
    }

    return &framebuffer;
}

FONT* LoadFont(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SysTbl)
{
    EFI_FILE* fontFile = LoadFile(Directory, Path, ImageHandle, SysTbl);
    if (!fontFile) return NULL;

    // Erstmal Magic lesen
    uint32_t magic;
    UINTN magicSize = sizeof(magic);
    fontFile->Read(fontFile, &magicSize, &magic);

    FONT* loadedFont;
    SysTbl->BootServices->AllocatePool(EfiLoaderData, sizeof(FONT), (void**)&loadedFont);

    if ((magic & 0xFFFF) == ((PSF1_MAGIC1 << 8) | PSF1_MAGIC0))
    {
        // --- PSF1 ---
        fontFile->SetPosition(fontFile, 0);
        PSF1_HEADER* hdr;
        SysTbl->BootServices->AllocatePool(EfiLoaderData, sizeof(PSF1_HEADER), (void**)&hdr);
        UINTN hdrSize = sizeof(PSF1_HEADER);
        fontFile->Read(fontFile, &hdrSize, hdr);

        UINTN glyphBufferSize = hdr->charsize * ((hdr->mode == 1) ? 512 : 256);
        void* glyphBuffer;
        fontFile->SetPosition(fontFile, sizeof(PSF1_HEADER));
        SysTbl->BootServices->AllocatePool(EfiLoaderData, glyphBufferSize, (void**)&glyphBuffer);
        fontFile->Read(fontFile, &glyphBufferSize, glyphBuffer);

        loadedFont->header = hdr;
        loadedFont->glyphBuffer = glyphBuffer;
        loadedFont->type = 1;
        loadedFont->width = 8;
        loadedFont->height = hdr->charsize;
        loadedFont->charsize = hdr->charsize;
    }
    else if (magic == PSF2_MAGIC)
    {
        // --- PSF2 ---
        fontFile->SetPosition(fontFile, 0);
        PSF2_HEADER* hdr;
        SysTbl->BootServices->AllocatePool(EfiLoaderData, sizeof(PSF2_HEADER), (void**)&hdr);
        UINTN hdrSize = sizeof(PSF2_HEADER);
        fontFile->Read(fontFile, &hdrSize, hdr);

        UINTN glyphBufferSize = hdr->charsize * hdr->length;
        void* glyphBuffer;
        fontFile->SetPosition(fontFile, hdr->headersize);
        SysTbl->BootServices->AllocatePool(EfiLoaderData, glyphBufferSize, (void**)&glyphBuffer);
        fontFile->Read(fontFile, &glyphBufferSize, glyphBuffer);

        loadedFont->header = hdr;
        loadedFont->glyphBuffer = glyphBuffer;
        loadedFont->type = 2;
        loadedFont->width = hdr->width;
        loadedFont->height = hdr->height;
        loadedFont->charsize = hdr->charsize;
    }
    else
    {
        return NULL;
    }

    return loadedFont;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SysTbl)
{
    InitializeLib(ImageHandle, SysTbl);
    Print(L"Bootloader successfully started... \n\r");

    EFI_FILE* Kernel = LoadFile(NULL, BOOTSTRAP_PATH, ImageHandle, SysTbl);
    if (Kernel == NULL)
    {
        Print(L"Could not load bootstrap \n\r");
    }
    else
    {
        Print(L"Bootstrap Loaded Successfully \n\r");
    }

    Elf64_Ehdr header;
    {
        UINTN FileInfoSize;
        EFI_FILE_INFO* FileInfo;
        Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, NULL);
        SysTbl->BootServices->AllocatePool(EfiLoaderData, FileInfoSize, (void**)&FileInfo);
        Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, (void**)&FileInfo);

        UINTN size = sizeof(header);
        Kernel->Read(Kernel, &size, &header);
    }

    if (
        memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_type != ET_EXEC ||
        header.e_machine != EM_X86_64 ||
        header.e_version != EV_CURRENT
    )
    {
        Print(L"Bootstraper format is bad\r\n");
    }
    else
    {
        Print(L"Bootstraper header successfully verified\r\n");
    }

    Elf64_Phdr* phdrs;
    {
        Kernel->SetPosition(Kernel, header.e_phoff);
        UINTN size = header.e_phnum * header.e_phentsize;
        SysTbl->BootServices->AllocatePool(EfiLoaderData, size, (void**)&phdrs);
        Kernel->Read(Kernel, &size, phdrs);
    }

    for (
        Elf64_Phdr* phdr = phdrs;
        (char*)phdr < (char*)phdrs + header.e_phnum * header.e_phentsize;
        phdr = (Elf64_Phdr*)((char*)phdr + header.e_phentsize)
    )
    {
        switch (phdr->p_type)
        {
        case PT_LOAD:
            {
                int pages = (phdr->p_memsz + 0x1000 - 1) / 0x1000;
                Elf64_Addr segment = phdr->p_paddr;
                SysTbl->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

                Kernel->SetPosition(Kernel, phdr->p_offset);
                UINTN size = phdr->p_filesz;
                Kernel->Read(Kernel, &size, (void*)segment);
                break;
            }
        }
    }

    Print(L"Bootstraper loaded\n\r");

    EFI_FILE* KernelFile = LoadFile(NULL, KERNEL_PATH, ImageHandle, SysTbl);
    if (!KernelFile)
    {
        Print(L"Could not load kernel.elf\n\r");
        while(1);
    }

    Elf64_Ehdr kernelHeader;
    {
        UINTN size = sizeof(kernelHeader);
        KernelFile->Read(KernelFile, &size, &kernelHeader);
    }

    // Prüfen ELF Magic etc.
    if (memcmp(&kernelHeader.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
        kernelHeader.e_ident[EI_CLASS] != ELFCLASS64 ||
        kernelHeader.e_type != ET_EXEC ||
        kernelHeader.e_machine != EM_X86_64)
    {
        Print(L"Kernel ELF invalid\n\r");
        while(1);
    }

    // Programmheader laden
    Elf64_Phdr* kernelPhdrs;
    {
        UINTN size = kernelHeader.e_phnum * kernelHeader.e_phentsize;
        SysTbl->BootServices->AllocatePool(EfiLoaderData, size, (void**)&kernelPhdrs);
        KernelFile->SetPosition(KernelFile, kernelHeader.e_phoff);
        KernelFile->Read(KernelFile, &size, kernelPhdrs);
    }

    // Berechne phys_start und phys_size
    Elf64_Addr phys_start = (Elf64_Addr)-1;
    Elf64_Addr phys_end   = 0;

    for (Elf64_Phdr* phdr = kernelPhdrs;
         (char*)phdr < (char*)kernelPhdrs + kernelHeader.e_phnum * kernelHeader.e_phentsize;
         phdr = (Elf64_Phdr*)((char*)phdr + kernelHeader.e_phentsize))
    {
        if (phdr->p_type != PT_LOAD) continue;

        if (phdr->p_paddr < phys_start) phys_start = phdr->p_paddr;
        if (phdr->p_paddr + phdr->p_memsz > phys_end) phys_end = phdr->p_paddr + phdr->p_memsz;

        int pages = (phdr->p_memsz + 0x1000 - 1) / 0x1000;
        Elf64_Addr segment = phdr->p_paddr;
        SysTbl->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

        KernelFile->SetPosition(KernelFile, phdr->p_offset);
        UINTN filesz = phdr->p_filesz;
        KernelFile->Read(KernelFile, &filesz, (void*)segment);
    }

    UINTN phys_size = phys_end - phys_start;

    InitializeGOP();

    FONT* font = LoadFont(NULL, L"zap-light24.psf", ImageHandle, SysTbl);
    if (font == NULL)
    {
        Print(L"Font invalid or not found");
    }
    else
    {
        if (font->type == 1)
        {
            Print(L"PSF1 Font found. char size = %d width: %u height: %u\n\r", font->charsize, font->height,
                  font->width);
        }
        else if (font->type == 2)
        {
            Print(L"PSF2 Font found. char size = %d width: %u height: %u\n\r", font->charsize, font->height,
                  font->width);
        }
    }

    EFI_CONFIGURATION_TABLE* config_table = ST->ConfigurationTable;
    VOID* rsdp = NULL;
    EFI_GUID Acpi2TableGuid = ACPI_20_TABLE_GUID;

    for (UINTN index = 0; index < ST->NumberOfTableEntries; index++)
    {
        if (CompareGuid(&config_table[index].VendorGuid, &Acpi2TableGuid))
        {
            if (strcmp((CHAR8*)"RSD PTR ", (CHAR8*)config_table->VendorTable, 8))
            {
                rsdp = (void*)config_table->VendorTable;
            }
        }
        config_table++;
    }


    void (*bootstrap_start)(BootstrapInfo*) = ((__attribute__((sysv_abi)) void (*)(BootstrapInfo*))header.e_entry);

    BootstrapInfo bootInfo;
    EFI_MEMORY_DESCRIPTOR* Map = NULL;
    UINTN MapSize, MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
    {
        SysTbl->BootServices->GetMemoryMap(&MapSize, Map, &MapKey, &DescriptorSize, &DescriptorVersion);
        SysTbl->BootServices->AllocatePool(EfiLoaderData, MapSize, (void**)&Map);
        SysTbl->BootServices->GetMemoryMap(&MapSize, Map, &MapKey, &DescriptorSize, &DescriptorVersion);
    }

    bootInfo.bi.fb = &framebuffer;
    bootInfo.bi.font = font;
    bootInfo.bi.mmap = Map;
    bootInfo.bi.mmap_size = MapSize;
    bootInfo.bi.mmap_desc_size = DescriptorSize;
    bootInfo.bi.rsdp = rsdp;
    bootInfo.phys_start = (void*)phys_start;
    bootInfo.phys_size = phys_size;
    bootInfo.kernel_entry = (KernelEntry)kernelHeader.e_entry;

    // https://www.youtube.com/watch?v=wbsfyRY_Yoc comment

    Print(L"Entry: %p\n", header.e_entry);
    SysTbl->BootServices->ExitBootServices(ImageHandle, MapKey);

    bootstrap_start(&bootInfo);

    while (1);

    return EFI_SUCCESS; // Exit the UEFI application
}
