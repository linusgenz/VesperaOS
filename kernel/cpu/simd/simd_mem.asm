; simd_mem.asm
; VesperaOS - SIMD-optimized memory routines
;
; General-purpose SIMD memory routines (no NT-stores, no prefetch).
; For framebuffer-specific optimizations see fb_simd_mem.asm.

bits 64
section .text

; ============================================================
; simd_memcpy_avx2(void* dst, const void* src, usize len)
;
; 1xYMM loop (32 bytes/iter), byte tail for remainder
; ============================================================
global simd_memcpy_avx2
simd_memcpy_avx2:
    cmp     rdx, 32
    jb      .byte_loop

.avx2_loop:
    vmovdqu ymm0, [rsi]
    vmovdqu [rdi], ymm0
    add     rsi, 32
    add     rdi, 32
    sub     rdx, 32
    cmp     rdx, 32
    jae     .avx2_loop

    test    rdx, rdx
    jz      .done

.byte_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rdx
    jnz     .byte_loop

.done:
    vzeroupper          ; avoid AVX->SSE transition penalty
    ret

; ============================================================
; simd_memcpy_sse2(void* dst, const void* src, usize len)
;
; SSE2 fallback for simd_memcpy_avx2.
; 1xXMM loop (16 bytes/iter), byte tail for remainder
; ============================================================
global simd_memcpy_sse2
simd_memcpy_sse2:
    cmp     rdx, 16
    jb      .byte_loop

.sse2_loop:
    movdqu  xmm0, [rsi]
    movdqu  [rdi], xmm0
    add     rsi, 16
    add     rdi, 16
    sub     rdx, 16
    cmp     rdx, 16
    jae     .sse2_loop

    test    rdx, rdx
    jz      .done

.byte_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rdx
    jnz     .byte_loop

.done:
    ret

; ============================================================
; simd_memset_avx2(void* dst, u32 value, usize len)
;
; Broadcasts value to all 8 YMM lanes, 32 bytes/iter.
; u32 tail for remaining bytes (pixel granularity).
; ============================================================
global simd_memset_avx2
simd_memset_avx2:
    ; broadcast u32 value across all 8 lanes of ymm0
    vmovd        xmm0, esi
    vpbroadcastd ymm0, xmm0

    cmp     rdx, 32
    jb      .tail

.avx2_loop:
    vmovdqu [rdi], ymm0
    add     rdi, 32
    sub     rdx, 32
    cmp     rdx, 32
    jae     .avx2_loop

    test    rdx, rdx
    jz      .done

    ; u32 tail (pixel-wise)
.tail:
    cmp     rdx, 4
    jb      .done

.tail_loop:
    mov     [rdi], esi
    add     rdi, 4
    sub     rdx, 4
    cmp     rdx, 4
    jae     .tail_loop

.done:
    vzeroupper
    ret

; ============================================================
; simd_memset_sse2(void* dst, u32 value, usize len)
;
; SSE2 fallback for simd_memset_avx2.
; Broadcasts value to all 4 XMM lanes, 16 bytes/iter.
; ============================================================
global simd_memset_sse2
simd_memset_sse2:
    ; broadcast u32 value across all 4 lanes of xmm0
    movd    xmm0, esi
    pshufd  xmm0, xmm0, 0x00

    cmp     rdx, 16
    jb      .tail

.sse2_loop:
    movdqu  [rdi], xmm0
    add     rdi, 16
    sub     rdx, 16
    cmp     rdx, 16
    jae     .sse2_loop

    test    rdx, rdx
    jz      .done

    ; u32 tail
.tail:
    cmp     rdx, 4
    jb      .done

.tail_loop:
    mov     [rdi], esi
    add     rdi, 4
    sub     rdx, 4
    cmp     rdx, 4
    jae     .tail_loop

.done:
    ret

; ============================================================
; simd_memmove_avx2(void* dst, const void* src, usize len)
;
; Overlap-safe memmove.
; Forward path  (dst <= src): jumps directly to simd_memcpy_avx2.
; Backward path (dst >  src): copies from end to start to avoid overlap.
; ============================================================
global simd_memmove_avx2
simd_memmove_avx2:
    cmp     rdi, rsi
    jbe     simd_memcpy_avx2    ; no overlap or dst < src -> forward copy

    ; Backward path: dst > src, copy from end to start
    add     rdi, rdx
    add     rsi, rdx

    cmp     rdx, 32
    jb      .byte_loop

.avx2_loop:
    sub     rdi, 32
    sub     rsi, 32
    vmovdqu ymm0, [rsi]
    vmovdqu [rdi], ymm0
    sub     rdx, 32
    cmp     rdx, 32
    jae     .avx2_loop

    test    rdx, rdx
    jz      .done

.byte_loop:
    dec     rdi
    dec     rsi
    mov     al, [rsi]
    mov     [rdi], al
    dec     rdx
    jnz     .byte_loop

.done:
    vzeroupper
    ret