// main.c
// Minimal-Testprogramm: nutzt vmath_add()/vmath_mul() aus libvmath.so ueber
// ganz normales dynamisches Linken (DT_NEEDED), NICHT ueber dlopen()/dlsym().
// Der Kernel laedt dieses Executable, sieht PT_INTERP=/lib/ld-vespera.so,
// startet den Interpreter, der wiederum via DT_NEEDED libvmath.so findet,
// laedt, reloziert, und erst DANN zu diesem main()-Entry springt.
//
// Bewusst freistehend gehalten (kein <stdio.h> etc.), damit dieses Demo
// unabhaengig vom Stand deiner vesplib-libc-Portierung testbar ist. Die
// "Ausgabe" laeuft ueber write() direkt auf HANDLE_STDOUT, analog zu den
// ld_dbg_write()-Helfern, die wir beim Debuggen des Loaders gebaut haben.

#include <stdint.h>

#include "../libmath/vmath.h"

#define HANDLE_TYPE_DEVICE 0x7000000000000000ULL
#define HANDLE_STDOUT       (HANDLE_TYPE_DEVICE | 1)

int64_t syscall(
    uint64_t num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5
) {
    int64_t ret = -1;

    register uint64_t r10_ asm("r10") = arg3;
    register uint64_t r8_ asm("r8") = arg4;
    register uint64_t r9_ asm("r9") = arg5;

    asm volatile("syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg0), "S"(arg1), "d"(arg2), "r"(r10_), "r"(r8_), "r"(r9_)
        : "rcx", "r11", "memory");

    return ret;
}

void exit(int code) {
    syscall(60, code, 0, 0, 0, 0, 0);
}

int64_t write(int64_t fd, const void* buf, uint64_t count) {
    return syscall(1, fd, (uint64_t)buf, count, 0, 0, 0);
}

static uint64_t my_strlen(const char* s) {
    uint64_t n = 0;
    while (s[n]) n++;
    return n;
}

static void write_str(const char* s) {
    write(HANDLE_STDOUT, s, my_strlen(s));
}

// Minimaler Hex-Ausgabehelfer, gleiches Muster wie ld_dbg_hex() im Loader -
// bewusst dupliziert statt geteilt, damit dieses Testprogramm keine
// Abhaengigkeit zum Loader-internen Code hat.
static void write_hex(const char* label, uint64_t val) {
    static const char hexdigits[] = "0123456789abcdef";
    char buf[16];
    write_str(label);
    write_str("0x");
    for (int i = 15; i >= 0; i--) {
        buf[15 - i] = hexdigits[(val >> (i * 4)) & 0xf];
    }
    write(HANDLE_STDOUT, buf, 16);
    write_str("\n");
}

int _start(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    write_str("=== VesperaOS dynlink demo (no dlfcn) ===\n");

    // Kernpruefung: Wenn ld-vespera.so das DT_NEEDED korrekt aufgeloest UND
    // reloziert hat, muss vmath_add()/vmath_mul() ueber die normale
    // R_X86_64_JUMP_SLOT/GLOB_DAT-Relokation auf den korrekten Code in
    // libvmath.so zeigen - kein manuelles dlopen()/dlsym() noetig.
    int sum = vmath_add(21, 21);
    int prod = vmath_mul(6, 7);

    write_hex("vmath_add(21,21) = ", (uint64_t)(int64_t)sum);
    write_hex("vmath_mul(6,7)   = ", (uint64_t)(int64_t)prod);

    if (sum == 42 && prod == 42) {
        write_str("RESULT: OK - dynamic linking against libvmath.so works\n");
        exit(0);
    } else {
        write_str("RESULT: FAIL - unexpected values, check relocations\n");
        exit(1);
    }

    exit(0);
}
