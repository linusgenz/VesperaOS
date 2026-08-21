
main:	file format elf64-x86-64

Disassembly of section .plt:

0000000000001000 <.plt>:
    1000: ff 35 ea 2f 00 00            	pushq	0x2fea(%rip)            # 0x3ff0 <_GLOBAL_OFFSET_TABLE_+0x8>
    1006: ff 25 ec 2f 00 00            	jmpq	*0x2fec(%rip)           # 0x3ff8 <_GLOBAL_OFFSET_TABLE_+0x10>
    100c: 0f 1f 40 00                  	nopl	(%rax)

0000000000001010 <vmath_add@plt>:
    1010: ff 25 ea 2f 00 00            	jmpq	*0x2fea(%rip)           # 0x4000 <_GLOBAL_OFFSET_TABLE_+0x18>
    1016: 68 00 00 00 00               	pushq	$0x0
    101b: e9 e0 ff ff ff               	jmp	0x1000 <.plt>

0000000000001020 <vmath_mul@plt>:
    1020: ff 25 e2 2f 00 00            	jmpq	*0x2fe2(%rip)           # 0x4008 <_GLOBAL_OFFSET_TABLE_+0x20>
    1026: 68 01 00 00 00               	pushq	$0x1
    102b: e9 d0 ff ff ff               	jmp	0x1000 <.plt>

Disassembly of section .text:

0000000000001030 <syscall>:
    1030: 55                           	pushq	%rbp
    1031: 48 89 e5                     	movq	%rsp, %rbp
    1034: 48 8b 45 10                  	movq	0x10(%rbp), %rax
    1038: 48 89 7d f8                  	movq	%rdi, -0x8(%rbp)
    103c: 48 89 75 f0                  	movq	%rsi, -0x10(%rbp)
    1040: 48 89 55 e8                  	movq	%rdx, -0x18(%rbp)
    1044: 48 89 4d e0                  	movq	%rcx, -0x20(%rbp)
    1048: 4c 89 45 d8                  	movq	%r8, -0x28(%rbp)
    104c: 4c 89 4d d0                  	movq	%r9, -0x30(%rbp)
    1050: 48 c7 45 c8 ff ff ff ff      	movq	$-0x1, -0x38(%rbp)
    1058: 48 8b 45 d8                  	movq	-0x28(%rbp), %rax
    105c: 48 89 45 c0                  	movq	%rax, -0x40(%rbp)
    1060: 48 8b 45 d0                  	movq	-0x30(%rbp), %rax
    1064: 48 89 45 b8                  	movq	%rax, -0x48(%rbp)
    1068: 48 8b 45 10                  	movq	0x10(%rbp), %rax
    106c: 48 89 45 b0                  	movq	%rax, -0x50(%rbp)
    1070: 48 8b 45 f8                  	movq	-0x8(%rbp), %rax
    1074: 48 8b 7d f0                  	movq	-0x10(%rbp), %rdi
    1078: 48 8b 75 e8                  	movq	-0x18(%rbp), %rsi
    107c: 48 8b 55 e0                  	movq	-0x20(%rbp), %rdx
    1080: 4c 8b 55 c0                  	movq	-0x40(%rbp), %r10
    1084: 4c 8b 45 b8                  	movq	-0x48(%rbp), %r8
    1088: 4c 8b 4d b0                  	movq	-0x50(%rbp), %r9
    108c: 0f 05                        	syscall
    108e: 48 89 45 c8                  	movq	%rax, -0x38(%rbp)
    1092: 48 8b 45 c8                  	movq	-0x38(%rbp), %rax
    1096: 5d                           	popq	%rbp
    1097: c3                           	retq
    1098: 0f 1f 84 00 00 00 00 00      	nopl	(%rax,%rax)

00000000000010a0 <exit>:
    10a0: 55                           	pushq	%rbp
    10a1: 48 89 e5                     	movq	%rsp, %rbp
    10a4: 48 83 ec 10                  	subq	$0x10, %rsp
    10a8: 89 7d fc                     	movl	%edi, -0x4(%rbp)
    10ab: 48 63 75 fc                  	movslq	-0x4(%rbp), %rsi
    10af: bf 3c 00 00 00               	movl	$0x3c, %edi
    10b4: 31 c0                        	xorl	%eax, %eax
    10b6: 41 89 c1                     	movl	%eax, %r9d
    10b9: 4c 89 ca                     	movq	%r9, %rdx
    10bc: 4c 89 c9                     	movq	%r9, %rcx
    10bf: 4d 89 c8                     	movq	%r9, %r8
    10c2: 48 c7 04 24 00 00 00 00      	movq	$0x0, (%rsp)
    10ca: e8 61 ff ff ff               	callq	0x1030 <syscall>
    10cf: 90                           	nop

00000000000010d0 <write>:
    10d0: 55                           	pushq	%rbp
    10d1: 48 89 e5                     	movq	%rsp, %rbp
    10d4: 48 83 ec 20                  	subq	$0x20, %rsp
    10d8: 48 89 7d f8                  	movq	%rdi, -0x8(%rbp)
    10dc: 48 89 75 f0                  	movq	%rsi, -0x10(%rbp)
    10e0: 48 89 55 e8                  	movq	%rdx, -0x18(%rbp)
    10e4: 48 8b 75 f8                  	movq	-0x8(%rbp), %rsi
    10e8: 48 8b 55 f0                  	movq	-0x10(%rbp), %rdx
    10ec: 48 8b 4d e8                  	movq	-0x18(%rbp), %rcx
    10f0: bf 01 00 00 00               	movl	$0x1, %edi
    10f5: 31 c0                        	xorl	%eax, %eax
    10f7: 41 89 c1                     	movl	%eax, %r9d
    10fa: 4d 89 c8                     	movq	%r9, %r8
    10fd: 48 c7 04 24 00 00 00 00      	movq	$0x0, (%rsp)
    1105: e8 26 ff ff ff               	callq	0x1030 <syscall>
    110a: 48 83 c4 20                  	addq	$0x20, %rsp
    110e: 5d                           	popq	%rbp
    110f: c3                           	retq

0000000000001110 <_start>:
    1110: 55                           	pushq	%rbp
    1111: 48 89 e5                     	movq	%rsp, %rbp
    1114: 48 83 ec 20                  	subq	$0x20, %rsp
    1118: 89 7d fc                     	movl	%edi, -0x4(%rbp)
    111b: 48 89 75 f0                  	movq	%rsi, -0x10(%rbp)
    111f: 48 89 55 e8                  	movq	%rdx, -0x18(%rbp)
    1123: 48 8d 3d d6 0e 00 00         	leaq	0xed6(%rip), %rdi       # 0x2000 <vmath_mul+0x2000>
    112a: e8 81 00 00 00               	callq	0x11b0 <write_str>
    112f: be 15 00 00 00               	movl	$0x15, %esi
    1134: 89 f7                        	movl	%esi, %edi
    1136: e8 d5 fe ff ff               	callq	0x1010 <vmath_add@plt>
    113b: 89 45 e4                     	movl	%eax, -0x1c(%rbp)
    113e: bf 06 00 00 00               	movl	$0x6, %edi
    1143: be 07 00 00 00               	movl	$0x7, %esi
    1148: e8 d3 fe ff ff               	callq	0x1020 <vmath_mul@plt>
    114d: 89 45 e0                     	movl	%eax, -0x20(%rbp)
    1150: 48 63 75 e4                  	movslq	-0x1c(%rbp), %rsi
    1154: 48 8d 3d d0 0e 00 00         	leaq	0xed0(%rip), %rdi       # 0x202b <vmath_mul+0x202b>
    115b: e8 90 00 00 00               	callq	0x11f0 <write_hex>
    1160: 48 63 75 e0                  	movslq	-0x20(%rbp), %rsi
    1164: 48 8d 3d d4 0e 00 00         	leaq	0xed4(%rip), %rdi       # 0x203f <vmath_mul+0x203f>
    116b: e8 80 00 00 00               	callq	0x11f0 <write_hex>
    1170: 83 7d e4 2a                  	cmpl	$0x2a, -0x1c(%rbp)
    1174: 75 19                        	jne	0x118f <_start+0x7f>
    1176: 83 7d e0 2a                  	cmpl	$0x2a, -0x20(%rbp)
    117a: 75 13                        	jne	0x118f <_start+0x7f>
    117c: 48 8d 3d d0 0e 00 00         	leaq	0xed0(%rip), %rdi       # 0x2053 <vmath_mul+0x2053>
    1183: e8 28 00 00 00               	callq	0x11b0 <write_str>
    1188: 31 ff                        	xorl	%edi, %edi
    118a: e8 11 ff ff ff               	callq	0x10a0 <exit>
    118f: 48 8d 3d f5 0e 00 00         	leaq	0xef5(%rip), %rdi       # 0x208b <vmath_mul+0x208b>
    1196: e8 15 00 00 00               	callq	0x11b0 <write_str>
    119b: bf 01 00 00 00               	movl	$0x1, %edi
    11a0: e8 fb fe ff ff               	callq	0x10a0 <exit>
    11a5: 66 66 2e 0f 1f 84 00 00 00 00 00     	nopw	%cs:(%rax,%rax)

00000000000011b0 <write_str>:
    11b0: 55                           	pushq	%rbp
    11b1: 48 89 e5                     	movq	%rsp, %rbp
    11b4: 48 83 ec 10                  	subq	$0x10, %rsp
    11b8: 48 89 7d f8                  	movq	%rdi, -0x8(%rbp)
    11bc: 48 8b 45 f8                  	movq	-0x8(%rbp), %rax
    11c0: 48 89 45 f0                  	movq	%rax, -0x10(%rbp)
    11c4: 48 8b 7d f8                  	movq	-0x8(%rbp), %rdi
    11c8: e8 c3 00 00 00               	callq	0x1290 <my_strlen>
    11cd: 48 8b 75 f0                  	movq	-0x10(%rbp), %rsi
    11d1: 48 89 c2                     	movq	%rax, %rdx
    11d4: 48 bf 01 00 00 00 00 00 00 70	movabsq	$0x7000000000000001, %rdi # imm = 0x7000000000000001
    11de: e8 ed fe ff ff               	callq	0x10d0 <write>
    11e3: 48 83 c4 10                  	addq	$0x10, %rsp
    11e7: 5d                           	popq	%rbp
    11e8: c3                           	retq
    11e9: 0f 1f 80 00 00 00 00         	nopl	(%rax)

00000000000011f0 <write_hex>:
    11f0: 55                           	pushq	%rbp
    11f1: 48 89 e5                     	movq	%rsp, %rbp
    11f4: 48 83 ec 30                  	subq	$0x30, %rsp
    11f8: 48 89 7d f8                  	movq	%rdi, -0x8(%rbp)
    11fc: 48 89 75 f0                  	movq	%rsi, -0x10(%rbp)
    1200: 48 8b 7d f8                  	movq	-0x8(%rbp), %rdi
    1204: e8 a7 ff ff ff               	callq	0x11b0 <write_str>
    1209: 48 8d 3d b0 0e 00 00         	leaq	0xeb0(%rip), %rdi       # 0x20c0 <vmath_mul+0x20c0>
    1210: e8 9b ff ff ff               	callq	0x11b0 <write_str>
    1215: c7 45 dc 0f 00 00 00         	movl	$0xf, -0x24(%rbp)
    121c: 83 7d dc 00                  	cmpl	$0x0, -0x24(%rbp)
    1220: 7c 39                        	jl	0x125b <write_hex+0x6b>
    1222: 48 8b 45 f0                  	movq	-0x10(%rbp), %rax
    1226: 8b 4d dc                     	movl	-0x24(%rbp), %ecx
    1229: c1 e1 02                     	shll	$0x2, %ecx
    122c: 89 c9                        	movl	%ecx, %ecx
    122e: 48 d3 e8                     	shrq	%cl, %rax
    1231: 48 89 c1                     	movq	%rax, %rcx
    1234: 48 83 e1 0f                  	andq	$0xf, %rcx
    1238: 48 8d 05 91 0e 00 00         	leaq	0xe91(%rip), %rax       # 0x20d0 <write_hex.hexdigits>
    123f: 8a 0c 08                     	movb	(%rax,%rcx), %cl
    1242: b8 0f 00 00 00               	movl	$0xf, %eax
    1247: 2b 45 dc                     	subl	-0x24(%rbp), %eax
    124a: 48 98                        	cltq
    124c: 88 4c 05 e0                  	movb	%cl, -0x20(%rbp,%rax)
    1250: 8b 45 dc                     	movl	-0x24(%rbp), %eax
    1253: 83 c0 ff                     	addl	$-0x1, %eax
    1256: 89 45 dc                     	movl	%eax, -0x24(%rbp)
    1259: eb c1                        	jmp	0x121c <write_hex+0x2c>
    125b: 48 8d 75 e0                  	leaq	-0x20(%rbp), %rsi
    125f: 48 bf 01 00 00 00 00 00 00 70	movabsq	$0x7000000000000001, %rdi # imm = 0x7000000000000001
    1269: ba 10 00 00 00               	movl	$0x10, %edx
    126e: e8 5d fe ff ff               	callq	0x10d0 <write>
    1273: 48 8d 3d af 0d 00 00         	leaq	0xdaf(%rip), %rdi       # 0x2029 <vmath_mul+0x2029>
    127a: e8 31 ff ff ff               	callq	0x11b0 <write_str>
    127f: 48 83 c4 30                  	addq	$0x30, %rsp
    1283: 5d                           	popq	%rbp
    1284: c3                           	retq
    1285: 66 66 2e 0f 1f 84 00 00 00 00 00     	nopw	%cs:(%rax,%rax)

0000000000001290 <my_strlen>:
    1290: 55                           	pushq	%rbp
    1291: 48 89 e5                     	movq	%rsp, %rbp
    1294: 48 89 7d f8                  	movq	%rdi, -0x8(%rbp)
    1298: 48 c7 45 f0 00 00 00 00      	movq	$0x0, -0x10(%rbp)
    12a0: 48 8b 45 f8                  	movq	-0x8(%rbp), %rax
    12a4: 48 8b 4d f0                  	movq	-0x10(%rbp), %rcx
    12a8: 80 3c 08 00                  	cmpb	$0x0, (%rax,%rcx)
    12ac: 74 0e                        	je	0x12bc <my_strlen+0x2c>
    12ae: 48 8b 45 f0                  	movq	-0x10(%rbp), %rax
    12b2: 48 83 c0 01                  	addq	$0x1, %rax
    12b6: 48 89 45 f0                  	movq	%rax, -0x10(%rbp)
    12ba: eb e4                        	jmp	0x12a0 <my_strlen+0x10>
    12bc: 48 8b 45 f0                  	movq	-0x10(%rbp), %rax
    12c0: 5d                           	popq	%rbp
    12c1: c3                           	retq
