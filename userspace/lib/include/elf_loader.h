// elf_loader.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 13.08.26.
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
//
// ---------------------------------------------------------------------------
// Freistehender ELF64-Loader-Kern: Mapping, DT_*-Scan, Relokation,
// Symbolsuche. Wird sowohl von dlfcn.c (Runtime dlopen()) als auch von
// ld-vespera.so (Bootstrap-Interpreter fuer PT_INTERP-Programme) genutzt.
//
// Diese Datei setzt voraus, dass folgende Symbole beim Linken verfuegbar
// sind (siehe Forward-Declarations unten):
//   - memcpy, memset                (freistehende oder vesplib-Variante)
//   - elf_loader_sys_mmap, elf_loader_sys_mprotect, elf_loader_sys_munmap
// Letztere drei sind bewusst NICHT direkt mmap/mprotect/munmap, sondern ein
// duenner Name, den der Aufrufer (dlfcn.c bzw. ld_main.c) auf den jeweils
// passenden Syscall-Wrapper mapped - so bleibt diese Datei unabhaengig
// davon, ob gerade vesplib-Wrapper oder rohe Syscalls verfuegbar sind.
// ---------------------------------------------------------------------------

#ifndef VESPERA_ELF_LOADER_H
#define VESPERA_ELF_LOADER_H

#include <stdint.h>
#include <stddef.h>

#include "elf.h"

#ifdef __cplusplus
extern "C" {
#endif

// Kein <stddef.h>, um diese Datei frei von zusaetzlichen Header-
// Abhaengigkeiten zu halten (relevant fuer den Bootstrap-Fall in
// ld-vespera.so, wo vor der Selbst-Relokation nichts vorausgesetzt werden
// soll). NULL wird daher hier lokal definiert, falls es nicht schon ueber
// einen anderen eingebundenen Header vorhanden ist.
#ifndef NULL
#define NULL ((void*)0)
#endif

#define ELF_LOADER_MAX_NEEDED 32

// PROT_*-Konventionen fuer elf_loader_sys_mmap/elf_loader_sys_mprotect.
// Werte entsprechen den ueblichen sys/mman.h-Konstanten; hier lokal
// definiert, damit diese Datei ohne sys/mman.h uebersetzbar bleibt.
#define ELF_LOADER_PROT_NONE  0x0
#define ELF_LOADER_PROT_READ  0x1
#define ELF_LOADER_PROT_WRITE 0x2
#define ELF_LOADER_PROT_EXEC  0x4

extern void* memcpy(void* dst, const void* src, size_t n);
extern void* memset(void* dst, uint8_t c, size_t n);

// Syscall-nahe Primitiven. addr!=0 + fixed!=0 => an exakt dieser Adresse
// mappen (MAP_FIXED-Aequivalent). Rueckgabe (void*)-1 bei Fehler.
void* elf_loader_sys_mmap(void* addr, uint64_t size, int prot, int fixed);
int   elf_loader_sys_mprotect(void* addr, uint64_t size, int prot);
int   elf_loader_sys_munmap(void* addr, uint64_t size);

// ---------------------------------------------------------------------------
// Ein einzelnes geladenes ELF-Objekt (Hauptprogramm oder Shared Object).
// Enthaelt alles, was fuer Relokation und Symbolsuche gebraucht wird.
// dlfcn.c bettet dies in sein dl_object_t ein bzw. nutzt es 1:1; ld_main.c
// haelt ein flaches Array davon fuer die DT_NEEDED-Kette.
// ---------------------------------------------------------------------------
typedef struct elf_loaded_object {
    uint64_t    load_bias;
    uint64_t    map_base;
    uint64_t    map_size;
    Elf64_Ehdr* ehdr;

    Elf64_Sym*  dynsym;
    const char* dynstr;
    uint64_t    dynsym_count;

    uint32_t*   hash;       // DT_HASH, falls vorhanden (sonst NULL)
    uint32_t*   gnu_hash;   // DT_GNU_HASH, falls vorhanden (sonst NULL)

    void*       init_fn;    // DT_INIT
    void**      init_array; // DT_INIT_ARRAY
    uint64_t    init_array_count;

    // Aufloesungskette fuer Symbolsuche (DT_NEEDED-Abhaengigkeiten). Der
    // Aufrufer befuellt dies (Speicherverwaltung bleibt bei ihm - dlfcn.c
    // nutzt sein festes Slot-Array, ld_main.c ein eigenes flaches Array).
    struct elf_loaded_object* needed[ELF_LOADER_MAX_NEEDED];
    uint32_t    needed_count;
} elf_loaded_object_t;

// Ergebnis von elf_loader_scan_dynamic(): alles aus PT_DYNAMIC, was fuer
// Relokation und DT_NEEDED-Aufloesung noetig ist, aber noch keine
// Objekt-Referenzen braucht (die DT_NEEDED-Eintraege sind zunaechst nur
// String-Offsets - Aufloesung/Laden der jeweiligen Datei bleibt Sache des
// Aufrufers, da das File-I/O erfordert).
typedef struct {
    uint64_t rela_addr, rela_size, rela_ent;
    uint64_t jmprel_addr, jmprel_size;
    uint64_t needed_off[ELF_LOADER_MAX_NEEDED];
    uint32_t needed_count;
} elf_dyn_scan_result_t;

// --- Alignment-Helfer -------------------------------------------------------

uint64_t elf_loader_align_down(uint64_t v, uint64_t align);
uint64_t elf_loader_align_up(uint64_t v, uint64_t align);

// --- Validierung -------------------------------------------------------------

// Prueft Magic/Klasse/Maschine/Typ (ET_DYN oder ET_EXEC). Gibt 1 bei
// gueltigem x86_64-ELF64 zurueck, sonst 0.
int elf_loader_validate_ehdr(const Elf64_Ehdr* eh);

// --- Adressraum-Ermittlung ----------------------------------------------------

// Ermittelt den von allen PT_LOAD-Segmenten gemeinsam ueberdeckten
// virtuellen Adressbereich [min_addr, max_addr). Gibt 0 zurueck, wenn kein
// PT_LOAD-Segment existiert.
int elf_loader_calc_address_range(const Elf64_Ehdr* eh, const void* file,
                                   uint64_t* min_addr, uint64_t* max_addr);

// Findet das PT_DYNAMIC-Programmheader-Segment. NULL, falls nicht vorhanden
// (z.B. statisch gelinktes Executable).
Elf64_Phdr* elf_loader_find_dynamic_phdr(const Elf64_Ehdr* eh);

// Findet das PT_INTERP-Programmheader-Segment. NULL, falls nicht vorhanden.
// Wird vom Kernel-ELF-Loader benutzt, nicht von ld-vespera.so selbst.
Elf64_Phdr* elf_loader_find_interp_phdr(const Elf64_Ehdr* eh);

// --- Mapping -------------------------------------------------------------

// Mapped alle PT_LOAD-Segmente von `file` relativ zu load_bias in den
// zuvor reservierten Bereich [map_base, map_base+map_size). Kopiert
// Dateiinhalt hinein und setzt anschliessend per elf_loader_sys_mprotect
// die tatsaechlichen Segment-Schutzrechte (R/W/X gemaess p_flags). Gibt 1
// bei Erfolg zurueck, 0 wenn das initiale Reservierungs-Mapping fehlschlaegt.
int elf_loader_map_segments(const Elf64_Ehdr* eh, const void* file,
                             uint64_t load_bias, uint64_t map_base,
                             uint64_t map_size);

// --- PT_DYNAMIC-Scan -------------------------------------------------------

// Durchlaeuft PT_DYNAMIC und befuellt sowohl obj (dynsym/dynstr/hash/
// init_fn/init_array/...) als auch out (rela/jmprel-Lage, DT_NEEDED-Offsets).
// dyn zeigt bereits auf die relozierte (load_bias-addierte) Adresse des
// PT_DYNAMIC-Segments.
void elf_loader_scan_dynamic(elf_loaded_object_t* obj, const Elf64_Dyn* dyn,
                              uint64_t load_bias, elf_dyn_scan_result_t* out);

// --- Symbolsuche -------------------------------------------------------------

// Sucht `name` ausschliesslich innerhalb von obj (kein Abstieg in
// obj->needed). NULL, falls nicht gefunden oder kein dynsym/dynstr
// vorhanden.
Elf64_Sym* elf_loader_find_symbol_in_object(elf_loaded_object_t* obj, const char* name);

// Sucht `name` in obj und rekursiv in obj->needed[]. *found_in wird auf das
// Objekt gesetzt, in dem das Symbol tatsaechlich definiert ist (fuer die
// load_bias-Berechnung der Aufrufer). depth ist der Rekursionsstart (0 beim
// ersten Aufruf) und wird intern gegen eine feste Tiefe (16) begrenzt, um
// zyklische DT_NEEDED-Graphen abzufangen.
Elf64_Sym* elf_loader_find_symbol_recursive(elf_loaded_object_t* obj, const char* name,
                                             elf_loaded_object_t** found_in, int depth);

// --- Relokation -------------------------------------------------------------

// Wendet eine RELA-Tabelle (.rela.dyn oder .rela.plt) auf obj an. Behandelt
// R_X86_64_RELATIVE, R_X86_64_GLOB_DAT, R_X86_64_JUMP_SLOT, R_X86_64_64.
// Symbolaufloesung fuer undefinierte Symbole laeuft ueber
// elf_loader_find_symbol_recursive() innerhalb von obj.
void elf_loader_apply_relocations(elf_loaded_object_t* obj, uint64_t rela_addr,
                                   uint64_t rela_size, uint64_t rela_ent);

// --- Init-Funktionen -------------------------------------------------------

// Ruft DT_INIT und anschliessend alle Eintraege von DT_INIT_ARRAY auf.
void elf_loader_run_init_functions(elf_loaded_object_t* obj);

#ifdef __cplusplus
}
#endif

#endif // VESPERA_ELF_LOADER_H