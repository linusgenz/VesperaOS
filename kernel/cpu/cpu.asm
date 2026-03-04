[bits 64]

section .bss
    vendor_buffer resb 13
section .data
    cpu_features dq 0
    vendor_string db "GenuineIntel", 0   ; Intel string
    amd_string db "AuthenticAMD", 0      ; AMD string


get_cpu_vendor:
    xor rax, rax;
    cpuid
    mov dword [rdi], ebx
    mov dword [rdi+4], edx
    mov dword [rdi+8], ecx
    ret

GLOBAL get_cpu_vendor

check_cpu_features:
    xor rax, rax
    mov eax, 1
    cpuid

    bt edx, 25
    adc rax, 0x1        ; SSE
    bt edx, 26
    adc rax, 0x2        ; SSE2
    bt edx, 28
    adc rax, 0x80       ; HTT

    bt ecx, 0
    adc rax, 0x8        ; SSE3
    bt ecx, 9
    adc rax, 0x10       ; SSSE3
    bt ecx, 19
    adc rax, 0x20       ; SSE4.1
    bt ecx, 20
    adc rax, 0x40       ; SSE4.2
    bt ecx, 28
    adc rax, 0x4        ; AVX
    ret

GLOBAL check_cpu_features

get_cpu_brand:
    mov eax, 0x80000002
    cpuid
    mov [rdi], eax
    mov [rdi+4], ebx
    mov [rdi+8], ecx
    mov [rdi+12], edx

    mov eax, 0x80000003
    cpuid
    mov [rdi+16], eax
    mov [rdi+20], ebx
    mov [rdi+24], ecx
    mov [rdi+28], edx

    mov eax, 0x80000004
    cpuid
    mov [rdi+32], eax
    mov [rdi+36], ebx
    mov [rdi+40], ecx
    mov [rdi+44], edx

    ret

GLOBAL get_cpu_brand

