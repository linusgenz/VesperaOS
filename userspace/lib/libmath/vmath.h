// vmath.h
// Minimal-Demo: exportierte Funktion einer Shared Library fuer VesperaOS,
// gedacht zum Laden ueber PT_INTERP -> ld-vespera.so -> DT_NEEDED,
// NICHT ueber dlopen()/dlfcn.

#ifndef VMATH_H
#define VMATH_H

#ifdef __cplusplus
extern "C" {
#endif

// Simple, leicht im Debugger verifizierbare Funktion: addiert zwei
// Ganzzahlen. Bewusst trivial gehalten, damit ein falscher Rueckgabewert
// sofort auf einen Loader-/Relokations-Bug hindeutet, nicht auf einen
// Logikfehler in der Funktion selbst.
int vmath_add(int a, int b);

// Zweite Funktion, um mind. 2 Symbole in der .dynsym zu haben - hilfreich,
// um zu verifizieren, dass elf_loader_find_symbol_in_object() nicht nur
// zufaellig beim ersten Symbol funktioniert.
int vmath_mul(int a, int b);

#ifdef __cplusplus
}
#endif

#endif // VMATH_H