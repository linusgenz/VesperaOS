// ld_selfreloc.c
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
// Reloziert ld-vespera.so selbst, BEVOR irgendein anderer Teil des
// Interpreters laeuft. Der Kernel laedt ld-vespera.so wie jedes andere
// ET_DYN-Objekt (Segmente 1:1 aus der Datei kopiert), wendet dabei aber
// KEINE Relokationen an - das muss der Interpreter zwingend selbst tun,
// sonst zeigen z.B. seine eigenen GOT-Eintraege noch auf
// Datei-relative (nicht Lade-relative) Adressen.
//
// Regeln fuer diese Datei, striktER als fuer den Rest des Linkers:
//   - Ausschliesslich R_X86_64_RELATIVE wird verarbeitet. Das ist die
//     EINZIGE Relokationsart, die ohne Symbolaufloesung auskommt (reines
//     "load_bias + addend"). GLOB_DAT/JUMP_SLOT/64 brauchen dynsym/dynstr,
//     die selbst erst durch genau diese RELATIVE-Relokationen korrekt
//     adressierbar werden koennten - klassisches Henne-Ei-Problem, das
//     man umgeht, indem man kompilerseitig dafuer sorgt, dass
//     ld-vespera.so selbst KEINE externen Symbolabhaengigkeiten hat
//     (kein dlopen der eigenen Symbole, keine PLT-Sprungziele auf sich
//     selbst) - siehe Build-Hinweis am Dateiende.
//   - Kein Aufruf von memcpy/memset/irgendeiner Funktion, die selbst erst
//     durch eine Relokation aufgeloest werden muesste. Alles hier ist
//     entweder inline oder ruft ausschliesslich andere `static`-Funktionen
//     in DERSELBEN Uebersetzungseinheit auf, die der Compiler direkt
//     (PC-relativ, ohne GOT-Indirektion) adressieren kann.
//   - Keine globalen/statischen Variablen mit Adressnahme (kein
//     "static int x; foo(&x)") - deren Adressen liegen ebenfalls hinter
//     GOT-Eintraegen, solange nicht reloziert wurde. Alles bleibt auf dem
//     Stack (Parameter, lokale Variablen).
//
// Build-Hinweis: ld-vespera.so MUSS mit -fno-plt (oder aequivalent) und
// ohne jegliche externen/undefinierten Symbolreferenzen in diesem
// Uebersetzungsschritt gelinkt werden. `readelf -r ld-vespera.so` sollte
// ausschliesslich R_X86_64_RELATIVE-Eintraege zeigen (ggf. RELATIVE
// Eintraege im .got fuer interne Funktionspointer sind normal - genau die
// werden hier korrigiert).
// ---------------------------------------------------------------------------

#include <stdint.h>

#include "elf.h"

#define SR_R_X86_64_RELATIVE 8

void ld_selfreloc_apply(uint64_t load_bias, const Elf64_Dyn* dyn_runtime) {
    uint64_t rela_addr = 0;
    uint64_t rela_size = 0;
    uint64_t rela_ent = sizeof(Elf64_Rela);

    for (int i = 0; dyn_runtime[i].d_tag != DT_NULL; i++) {
        switch (dyn_runtime[i].d_tag) {
            case DT_RELA:
                rela_addr = dyn_runtime[i].d_un.d_ptr + load_bias;
                break;
            case DT_RELASZ:
                rela_size = dyn_runtime[i].d_un.d_val;
                break;
            case DT_RELAENT:
                rela_ent = dyn_runtime[i].d_un.d_val;
                break;
            default:
                break;
        }
    }

    if (!rela_addr || !rela_size || !rela_ent) {
        return;
    }

    uint64_t count = rela_size / rela_ent;
    Elf64_Rela* relocs = (Elf64_Rela*)rela_addr;

    for (uint64_t i = 0; i < count; i++) {
        Elf64_Rela* r = &relocs[i];
        uint32_t type = (uint32_t)(r->r_info & 0xffffffffu);

        // Strict prohibition: everything except RELATIVE is ignored.
        if (type != SR_R_X86_64_RELATIVE) {
            continue;
        }

        uint64_t* target = (uint64_t*)(r->r_offset + load_bias);
        *target = load_bias + (uint64_t)r->r_addend;
    }
}
