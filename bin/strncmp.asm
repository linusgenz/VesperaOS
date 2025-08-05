
global strncmp
strncmp:
    xor   eax, eax        ; Remains zero throughout the loop
.loop:
    sub   rdx, 1
    jb    .equal             ; Count ran out
    movzx ecx, byte [rsi]
    cmp   [rdi], cl
    jb    .less
    ja    .greater
    inc   rdi
    inc   rsi
    test  cl, cl
    jnz   .loop

.equal:
    ret        ; RAX=0
.less:
    dec   rax
    ret        ; RAX=-1
.greater:
    inc   eax
    ret        ; RAX=1
