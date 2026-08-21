// vmath.c
// Minimal-Demo-Shared-Library fuer VesperaOS. Absichtlich freistehend
// (keine libc-Abhaengigkeit), damit sie mit denselben restriktiven Flags
// wie ld-vespera.so selbst gebaut werden kann und keine zusaetzlichen
// DT_NEEDED-Eintraege ausser sich selbst benoetigt.

#include "vmath.h"

int vmath_add(int a, int b) {
    return a + b;
}

int vmath_mul(int a, int b) {
    return a * b;
}