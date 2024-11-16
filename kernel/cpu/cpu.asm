[bits 64]

section .data
    cpu_features dq 0
    vendor_string db "GenuineIntel", 0   ; Intel string
    amd_string db "AuthenticAMD", 0      ; AMD string
    vendor_buffer resb 13


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
    adc qword [cpu_features], 0x1
    bt edx, 26
    adc qword [cpu_features], 0x2
    bt edx, 28
    adc qword [cpu_features], 0x80

    bt ecx, 0
    adc qword [cpu_features], 0x8
    bt ecx, 9
    adc qword [cpu_features], 0x10
    bt ecx, 19
    adc qword [cpu_features], 0x20
    bt ecx, 20
    adc qword [cpu_features], 0x40
    bt ecx, 28
    adc qword [cpu_features], 0x4
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

