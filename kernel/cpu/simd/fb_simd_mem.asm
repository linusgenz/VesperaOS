; fb_simd_mem.asm
; VesperaOS - Framebuffer-optimized SIMD memory routines
;
; All routines use a 3-phase approach:
;   Phase 1 - Prefix:  byte-copy until dst is aligned
;   Phase 2 - SIMD:    aligned SIMD loop (main body)
;   Phase 3 - Tail:    remaining bytes after the loop
;
; Optimizations:
;   - Loop unrolling:  4xYMM = 128 bytes/iter (AVX2), 4xXMM = 64 bytes/iter (SSE2)
;   - Non-temporal stores (vmovntdq / movntdq): bypass cache, write directly to RAM
;     -> avoids polluting L1/L2/L3 with framebuffer data that won't be read back
;   - Software prefetch (prefetchnta): prefetch src 256 bytes ahead, non-temporal
;     -> brings data into L1 without caching in L2/L3
;   - sfence after NT-stores: ensures write ordering
;
; Why 4x unrolling?
;   The Write-Combining Buffer (WCB) typically has 4 entries x 64 bytes = 256 bytes.
;   4xYMM (128 bytes) fills 2 WCB entries per iteration — good utilization without overflow.
;   8xYMM offers no measurable benefit.

bits 64
section .text

; ============================================================
; fb_memcpy_avx2(void* dst, const void* src, usize len)
;
; 4xYMM unrolled, 128 bytes/iter
; prefetchnta 256 bytes ahead
; vmovdqu src (unaligned ok), vmovntdq dst (must be aligned)
; ============================================================
global fb_memcpy_avx2
fb_memcpy_avx2:
    test    rdx, rdx
    jz      .done

    ; Phase 1: copy bytes until dst is 32-byte aligned
    mov     rcx, rdi
    and     rcx, 31                     ; rcx = dst % 32
    jz      .main                       ; already aligned

    neg     rcx
    and     rcx, 31                     ; bytes until next 32-byte boundary

    cmp     rcx, rdx
    cmova   rcx, rdx                    ; clamp to remaining length
    sub     rdx, rcx

.prefix_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rcx
    jnz     .prefix_loop

    test    rdx, rdx
    jz      .flush

    ; Phase 2a: 4xYMM loop (128 bytes/iter)
.main:
    cmp     rdx, 128
    jb      .tail_32

.loop128:
    prefetchnta [rsi + 256]

    vmovdqu     ymm0, [rsi +  0]
    vmovdqu     ymm1, [rsi + 32]
    vmovdqu     ymm2, [rsi + 64]
    vmovdqu     ymm3, [rsi + 96]

    vmovntdq    [rdi +  0], ymm0
    vmovntdq    [rdi + 32], ymm1
    vmovntdq    [rdi + 64], ymm2
    vmovntdq    [rdi + 96], ymm3

    add         rsi, 128
    add         rdi, 128
    sub         rdx, 128
    cmp         rdx, 128
    jae         .loop128

    ; Phase 2b: 1xYMM loop for 32..127 remaining bytes
.tail_32:
    cmp     rdx, 32
    jb      .tail_byte

.loop32:
    vmovdqu     ymm0, [rsi]
    vmovntdq    [rdi], ymm0
    add         rsi, 32
    add         rdi, 32
    sub         rdx, 32
    cmp         rdx, 32
    jae         .loop32

    ; Phase 3: byte tail
.tail_byte:
    test    rdx, rdx
    jz      .flush

.tail_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rdx
    jnz     .tail_loop

.flush:
    sfence
    vzeroupper
.done:
    ret


; ============================================================
; fb_memcpy_sse2(void* dst, const void* src, usize len)
;
; 4xXMM unrolled, 64 bytes/iter
; prefetchnta 128 bytes ahead
; ============================================================
global fb_memcpy_sse2
fb_memcpy_sse2:
    test    rdx, rdx
    jz      .done

    ; Phase 1: align dst to 16 bytes
    mov     rcx, rdi
    and     rcx, 15
    jz      .main

    neg     rcx
    and     rcx, 15

    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx

.prefix_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rcx
    jnz     .prefix_loop

    test    rdx, rdx
    jz      .flush

    ; Phase 2a: 4xXMM loop (64 bytes/iter)
.main:
    cmp     rdx, 64
    jb      .tail_16

.loop64:
    prefetchnta [rsi + 128]

    movdqu      xmm0, [rsi +  0]
    movdqu      xmm1, [rsi + 16]
    movdqu      xmm2, [rsi + 32]
    movdqu      xmm3, [rsi + 48]

    movntdq     [rdi +  0], xmm0
    movntdq     [rdi + 16], xmm1
    movntdq     [rdi + 32], xmm2
    movntdq     [rdi + 48], xmm3

    add         rsi, 64
    add         rdi, 64
    sub         rdx, 64
    cmp         rdx, 64
    jae         .loop64

    ; Phase 2b: 1xXMM loop for 16..63 remaining bytes
.tail_16:
    cmp     rdx, 16
    jb      .tail_byte

.loop16:
    movdqu      xmm0, [rsi]
    movntdq     [rdi], xmm0
    add         rsi, 16
    add         rdi, 16
    sub         rdx, 16
    cmp         rdx, 16
    jae         .loop16

    ; Phase 3: byte tail
.tail_byte:
    test    rdx, rdx
    jz      .flush

.tail_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rdx
    jnz     .tail_loop

.flush:
    sfence
.done:
    ret


; ============================================================
; fb_fill_rect_avx2(
;     void*  fb_base,          rdi
;     u32    stride_pixels,    rsi
;     u32    px,               rdx
;     u32    py,               rcx
;     u32    w,                r8
;     u32    h,                r9
;     u32    colour            [rsp+8] nach call  → r10
; )
; ============================================================
global fb_fill_rect_avx2
fb_fill_rect_avx2:
    ; colour vom Stack holen (7. Argument)
    mov     r10d, [rsp + 8]

    ; row_start = fb_base + (py * stride + px) * 4
    mov     rax, rcx                ; rax = py
    imul    rax, rsi                ; rax = py * stride_pixels
    add     rax, rdx                ; rax = py * stride + px
    shl     rax, 2                  ; * sizeof(u32)
    add     rdi, rax                ; rdi = Zeiger auf erste Zeile

    ; stride_bytes = stride_pixels * 4
    shl     rsi, 2                  ; rsi = stride in bytes

    ; w_bytes = w * 4
    mov     r11, r8
    shl     r11, 2                  ; r11 = Breite in bytes

    ; colour in ymm0 broadcasten
    vmovd           xmm0, r10d
    vpbroadcastd    ymm0, xmm0
    vmovdqa         ymm1, ymm0
    vmovdqa         ymm2, ymm0
    vmovdqa         ymm3, ymm0

    ; h == 0 check
    test    r9, r9
    jz      .done

.row_loop:
    push    rdi                     ; Zeilenbeginn sichern
    mov     rdx, r11                ; rdx = w_bytes für diese Zeile

    ; Phase 1: Prefix bis 32-Byte-Alignment
    mov     rcx, rdi
    and     rcx, 31
    jz      .line_main

    neg     rcx
    and     rcx, 31
    and     rcx, ~3

    cmp     rcx, rdx
    cmova   rcx, rdx
    and     rcx, ~3

    sub     rdx, rcx
    shr     rcx, 2
    jz      .line_main

.prefix_loop:
    mov     [rdi], r10d
    add     rdi, 4
    dec     rcx
    jnz     .prefix_loop

    test    rdx, rdx
    jz      .line_flush

    ; Phase 2: 4×YMM NT-Loop (128 Bytes/iter)
.line_main:
    cmp     rdx, 128
    jb      .line_tail32

.line_loop128:
    vmovntdq    [rdi +   0], ymm0
    vmovntdq    [rdi +  32], ymm1
    vmovntdq    [rdi +  64], ymm2
    vmovntdq    [rdi +  96], ymm3
    add         rdi, 128
    sub         rdx, 128
    cmp         rdx, 128
    jae         .line_loop128

.line_tail32:
    cmp     rdx, 32
    jb      .line_tail_u32

.line_loop32:
    vmovntdq    [rdi], ymm0
    add         rdi, 32
    sub         rdx, 32
    cmp         rdx, 32
    jae         .line_loop32

    ; Phase 3: u32-Tail
.line_tail_u32:
    cmp     rdx, 4
    jb      .line_flush

.line_tail_loop:
    mov     [rdi], r10d
    add     rdi, 4
    sub     rdx, 4
    cmp     rdx, 4
    jae     .line_tail_loop

.line_flush:
    pop     rdi                     ; Zeilenbeginn wiederherstellen
    add     rdi, rsi                ; nächste Zeile: + stride_bytes

    dec     r9
    jnz     .row_loop

  ;  sfence
  ;  vzeroupper
.done:
    ret


; ============================================================
; fb_fill_rect_sse2(
;     void*  fb_base,          rdi
;     u32    stride_pixels,    rsi
;     u32    px,               rdx
;     u32    py,               rcx
;     u32    w,                r8
;     u32    h,                r9
;     u32    colour            [rsp+8]  → r10
; )
; ============================================================
global fb_fill_rect_sse2
fb_fill_rect_sse2:
    mov     r10d, [rsp + 8]

    mov     rax, rcx
    imul    rax, rsi
    add     rax, rdx
    shl     rax, 2
    add     rdi, rax

    shl     rsi, 2                  ; stride → bytes
    mov     r11, r8
    shl     r11, 2                  ; w → bytes

    ; colour in xmm0–xmm3 broadcasten
    movd        xmm0, r10d
    pshufd      xmm0, xmm0, 0x00
    movdqa      xmm1, xmm0
    movdqa      xmm2, xmm0
    movdqa      xmm3, xmm0

    test    r9, r9
    jz      .done

.row_loop:
    push    rdi
    mov     rdx, r11

    ; Phase 1: Prefix bis 16-Byte-Alignment
    mov     rcx, rdi
    and     rcx, 15
    jz      .line_main

    neg     rcx
    and     rcx, 15
    and     rcx, ~3

    cmp     rcx, rdx
    cmova   rcx, rdx
    and     rcx, ~3

    sub     rdx, rcx
    shr     rcx, 2
    jz      .line_main

.prefix_loop:
    mov     [rdi], r10d
    add     rdi, 4
    dec     rcx
    jnz     .prefix_loop

    test    rdx, rdx
    jz      .line_flush

    ; Phase 2: 4×XMM NT-Loop (64 Bytes/iter)
.line_main:
    cmp     rdx, 64
    jb      .line_tail16

.line_loop64:
    movntdq     [rdi +  0], xmm0
    movntdq     [rdi + 16], xmm1
    movntdq     [rdi + 32], xmm2
    movntdq     [rdi + 48], xmm3
    add         rdi, 64
    sub         rdx, 64
    cmp         rdx, 64
    jae         .line_loop64

.line_tail16:
    cmp     rdx, 16
    jb      .line_tail_u32

.line_loop16:
    movntdq     [rdi], xmm0
    add         rdi, 16
    sub         rdx, 16
    cmp         rdx, 16
    jae         .line_loop16

    ; Phase 3: u32-Tail
.line_tail_u32:
    cmp     rdx, 4
    jb      .line_flush

.line_tail_loop:
    mov     [rdi], r10d
    add     rdi, 4
    sub     rdx, 4
    cmp     rdx, 4
    jae     .line_tail_loop

.line_flush:
    pop     rdi
    add     rdi, rsi
    dec     r9
    jnz     .row_loop

    sfence
.done:
    ret


; ============================================================
; fb_memmove_avx2(void* dst, const void* src, usize len)
;
; Overlap-safe memmove, optimized for scroll_pixels (dst < src).
;
; Forward path (dst < src):
;   Same as fb_memcpy_avx2 — 4xYMM unrolled, NT-stores, prefetch.
;
; Backward path (dst > src):
;   Simple 1xYMM loop without NT-stores (rare path, no optimization needed).
;   NT-stores require aligned dst which can't be guaranteed going backwards.
; ============================================================
global fb_memmove_avx2
fb_memmove_avx2:
    test    rdx, rdx
    jz      .done

    cmp     rdi, rsi
    je      .done
    jb      .forward                    ; dst < src -> forward path

    ; Backward path: dst > src (overlapping, copy from end to start)
    add     rdi, rdx
    add     rsi, rdx

    cmp     rdx, 32
    jb      .back_byte

.back_loop:
    sub     rdi, 32
    sub     rsi, 32
    vmovdqu     ymm0, [rsi]
    vmovdqu     [rdi], ymm0             ; no NT-store: dst alignment unknown
    sub         rdx, 32
    cmp         rdx, 32
    jae         .back_loop

.back_byte:
    test    rdx, rdx
    jz      .back_done

.back_byte_loop:
    dec     rdi
    dec     rsi
    mov     al, [rsi]
    mov     [rdi], al
    dec     rdx
    jnz     .back_byte_loop

.back_done:
    vzeroupper
.done:
    ret

    ; Forward path: 3-phase, 4xYMM unrolled, NT-stores
.forward:
    ; Phase 1: align dst to 32 bytes
    mov     rcx, rdi
    and     rcx, 31
    jz      .fwd_main

    neg     rcx
    and     rcx, 31

    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx

.fwd_prefix:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rcx
    jnz     .fwd_prefix

    test    rdx, rdx
    jz      .fwd_flush

    ; Phase 2: 4xYMM NT loop (128 bytes/iter)
.fwd_main:
    cmp     rdx, 128
    jb      .fwd_tail_32

.fwd_loop128:
    prefetchnta [rsi + 256]

    vmovdqu     ymm0, [rsi +  0]
    vmovdqu     ymm1, [rsi + 32]
    vmovdqu     ymm2, [rsi + 64]
    vmovdqu     ymm3, [rsi + 96]

    vmovntdq    [rdi +  0], ymm0
    vmovntdq    [rdi + 32], ymm1
    vmovntdq    [rdi + 64], ymm2
    vmovntdq    [rdi + 96], ymm3

    add         rsi, 128
    add         rdi, 128
    sub         rdx, 128
    cmp         rdx, 128
    jae         .fwd_loop128

.fwd_tail_32:
    cmp     rdx, 32
    jb      .fwd_tail_byte

.fwd_loop32:
    vmovdqu     ymm0, [rsi]
    vmovntdq    [rdi], ymm0
    add         rsi, 32
    add         rdi, 32
    sub         rdx, 32
    cmp         rdx, 32
    jae         .fwd_loop32

    ; Phase 3: byte tail
.fwd_tail_byte:
    test    rdx, rdx
    jz      .fwd_flush

.fwd_tail_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rdx
    jnz     .fwd_tail_loop

.fwd_flush:
    sfence
    vzeroupper
    ret


; ============================================================
; fb_memcpy_avx512(void* dst, const void* src, usize len)
;
; 4xZMM unrolled, 256 bytes/iter
; prefetchnta 512 bytes ahead
; vmovdqu64 src, vmovntdq64 dst (64-byte aligned stores)
; ============================================================
global fb_memcpy_avx512
fb_memcpy_avx512:
    test    rdx, rdx
    jz      .done

    ; Phase 1: align dst to 64 bytes
    mov     rcx, rdi
    and     rcx, 63
    jz      .main

    neg     rcx
    and     rcx, 63

    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx

.prefix_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rcx
    jnz     .prefix_loop

    test    rdx, rdx
    jz      .flush

.main:
    cmp     rdx, 256
    jb      .tail_64

.loop256:
    prefetchnta [rsi + 512]

    vmovdqu64   zmm0, [rsi +   0]
    vmovdqu64   zmm1, [rsi +  64]
    vmovdqu64   zmm2, [rsi + 128]
    vmovdqu64   zmm3, [rsi + 192]

    vmovntdq    [rdi +   0], zmm0
    vmovntdq    [rdi +  64], zmm1
    vmovntdq    [rdi + 128], zmm2
    vmovntdq    [rdi + 192], zmm3

    add         rsi, 256
    add         rdi, 256
    sub         rdx, 256
    cmp         rdx, 256
    jae         .loop256

.tail_64:
    cmp     rdx, 64
    jb      .tail_byte

.loop64:
    vmovdqu64   zmm0, [rsi]
    vmovntdq    [rdi], zmm0
    add         rsi, 64
    add         rdi, 64
    sub         rdx, 64
    cmp         rdx, 64
    jae         .loop64

.tail_byte:
    test    rdx, rdx
    jz      .flush

.tail_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rdx
    jnz     .tail_loop

.flush:
    sfence
    vzeroupper
.done:
    ret


; ============================================================
; fb_memmove_avx512(void* dst, const void* src, usize len)
; Forward path identical zu fb_memcpy_avx512.
; Backward path: 1xZMM ohne NT-Stores.
; ============================================================
global fb_memmove_avx512
fb_memmove_avx512:
    test    rdx, rdx
    jz      .done

    cmp     rdi, rsi
    je      .done
    jb      .forward

    ; Backward path
    add     rdi, rdx
    add     rsi, rdx

    cmp     rdx, 64
    jb      .back_byte

.back_loop:
    sub     rdi, 64
    sub     rsi, 64
    vmovdqu64   zmm0, [rsi]
    vmovdqu64   [rdi], zmm0
    sub         rdx, 64
    cmp         rdx, 64
    jae         .back_loop

.back_byte:
    test    rdx, rdx
    jz      .back_done

.back_byte_loop:
    dec     rdi
    dec     rsi
    mov     al, [rsi]
    mov     [rdi], al
    dec     rdx
    jnz     .back_byte_loop

.back_done:
    vzeroupper
.done:
    ret

.forward:
    mov     rcx, rdi
    and     rcx, 63
    jz      .fwd_main

    neg     rcx
    and     rcx, 63

    cmp     rcx, rdx
    cmova   rcx, rdx
    sub     rdx, rcx

.fwd_prefix:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rcx
    jnz     .fwd_prefix

    test    rdx, rdx
    jz      .fwd_flush

.fwd_main:
    cmp     rdx, 256
    jb      .fwd_tail_64

.fwd_loop256:
    prefetchnta [rsi + 512]

    vmovdqu64   zmm0, [rsi +   0]
    vmovdqu64   zmm1, [rsi +  64]
    vmovdqu64   zmm2, [rsi + 128]
    vmovdqu64   zmm3, [rsi + 192]

    vmovntdq    [rdi +   0], zmm0
    vmovntdq    [rdi +  64], zmm1
    vmovntdq    [rdi + 128], zmm2
    vmovntdq    [rdi + 192], zmm3

    add         rsi, 256
    add         rdi, 256
    sub         rdx, 256
    cmp         rdx, 256
    jae         .fwd_loop256

.fwd_tail_64:
    cmp     rdx, 64
    jb      .fwd_tail_byte

.fwd_loop64:
    vmovdqu64   zmm0, [rsi]
    vmovntdq    [rdi], zmm0
    add         rsi, 64
    add         rdi, 64
    sub         rdx, 64
    cmp         rdx, 64
    jae         .fwd_loop64

.fwd_tail_byte:
    test    rdx, rdx
    jz      .fwd_flush

.fwd_tail_loop:
    mov     al, [rsi]
    mov     [rdi], al
    inc     rsi
    inc     rdi
    dec     rdx
    jnz     .fwd_tail_loop

.fwd_flush:
    sfence
    vzeroupper
    ret