# SPDX-License-Identifier: GPL-3.0-or-later
# Minimal Standalone Linux x86-64 Runtime for Cco Linker
# Provides: _start, exit, printf, malloc, free using pure Linux syscalls.

.global _start
.global exit
.global printf
.global malloc
.global free

.text

# Entry Point: _start
# Linux x86-64 ABI:
#   (%rsp)     = argc
#   8(%rsp)    = argv[0]
#   16(%rsp)   = argv[1] ...
_start:
    xorq %rbp, %rbp          # Mark top of stack frame for debuggers
    movq (%rsp), %rdi        # argc -> %rdi
    leaq 8(%rsp), %rsi       # argv -> %rsi
    call main                # call Cco main(argc, argv)
    movq %rax, %rdi          # main's return code -> exit code
exit:
    movq $60, %rax           # SYS_exit
    syscall
    hlt

# Minimal Memory Allocator: malloc & free
# Uses Linux sys_mmap (syscall 9). free is a safe no-op.
malloc:
    # Round size up to 16 bytes for alignment
    addq $15, %rdi
    andq $-16, %rdi
    movq %rdi, %rsi          # length
    xorq %rdi, %rdi          # addr = NULL (kernel chooses address)
    movq $3, %rdx            # PROT_READ | PROT_WRITE
    movq $34, %r10           # MAP_PRIVATE | MAP_ANONYMOUS
    movq $-1, %r8            # fd = -1
    xorq %r9, %r9            # offset = 0
    movq $9, %rax            # SYS_mmap
    syscall
    ret

free:
    # No-op in minimal runtime
    ret

# Minimal Formatted Printer: printf
# Handles Cco format strings:
#   "%ld\n" / "%d\n" -> 64-bit signed integer in %rsi
#   "%s\n"           -> null-terminated string in %rsi
#   "%g\n"           -> 64-bit IEEE 754 double in %xmm0
# Uses Linux sys_write (syscall 1) directly to stdout (fd 1).
printf:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    subq $88, %rsp

    testq %rdi, %rdi
    jz .Lprintf_done
    cmpb $0x25, (%rdi)       # check '%'
    jne .Lprintf_done
    movzbl 1(%rdi), %eax
    cmpb $0x73, %al          # 's'
    je .Lprint_str
    cmpb $0x67, %al          # 'g'
    je .Lprint_float

.Lprint_int:
    movq %rsi, %rax
    leaq 63(%rsp), %r12
    movb $10, (%r12)         # newline
    movq $1, %rcx           # length
    testq %rax, %rax
    jns .Lpos_int
    negq %rax
    movb $1, %r13b
    jmp .Ldloop
.Lpos_int:
    movb $0, %r13b
.Ldloop:
    xorq %rdx, %rdx
    movq $10, %rbx
    divq %rbx
    addb $48, %dl
    decq %r12
    incq %rcx
    movb %dl, (%r12)
    testq %rax, %rax
    jnz .Ldloop
    testb %r13b, %r13b
    jz .Lwrite_int
    decq %r12
    incq %rcx
    movb $0x2d, (%r12)      # '-'
.Lwrite_int:
    movq $1, %rax           # SYS_write
    movq $1, %rdi           # fd = stdout
    movq %r12, %rsi         # buf
    movq %rcx, %rdx         # count
    syscall
    jmp .Lprintf_done

.Lprint_str:
    testq %rsi, %rsi
    jz .Lprintf_done
    movq %rsi, %r12
    xorq %rdx, %rdx
.Lstrlen:
    cmpb $0, (%r12, %rdx)
    je .Lstr_out
    incq %rdx
    jmp .Lstrlen
.Lstr_out:
    movq $1, %rax           # SYS_write
    movq $1, %rdi           # fd = stdout
    syscall
    # write trailing newline
    movb $10, 64(%rsp)
    movq $1, %rax
    movq $1, %rdi
    leaq 64(%rsp), %rsi
    movq $1, %rdx
    syscall
    jmp .Lprintf_done

.Lprint_float:
    xorpd %xmm1, %xmm1
    ucomisd %xmm1, %xmm0
    jae .Lfloat_pos
    # print negative sign
    movb $0x2d, 64(%rsp)
    movq $1, %rax
    movq $1, %rdi
    leaq 64(%rsp), %rsi
    movq $1, %rdx
    syscall
    # negate float: 0.0 - xmm0
    subsd %xmm0, %xmm1
    movaps %xmm1, %xmm0
.Lfloat_pos:
    cvttsd2si %xmm0, %r12   # integer part -> %r12
    cvtsi2sd %r12, %xmm1
    subsd %xmm1, %xmm0      # fractional part in %xmm0
    movq $0x40c3880000000000, %rax # 10000.0
    movq %rax, %xmm1
    mulsd %xmm1, %xmm0
    cvttsd2si %xmm0, %r13   # frac part (0..9999) -> %r13

    # Format integer part
    movq %r12, %rax
    leaq 31(%rsp), %rsi
    movq $0, %rcx
.Li_dloop:
    xorq %rdx, %rdx
    movq $10, %rbx
    divq %rbx
    addb $48, %dl
    decq %rsi
    incq %rcx
    movb %dl, (%rsi)
    testq %rax, %rax
    jnz .Li_dloop
    movq $1, %rax
    movq $1, %rdi
    movq %rcx, %rdx
    syscall

    # Print decimal point '.'
    movb $0x2e, 64(%rsp)
    movq $1, %rax
    movq $1, %rdi
    leaq 64(%rsp), %rsi
    movq $1, %rdx
    syscall

    # Format 4 fractional digits zero-padded + newline
    movq %r13, %rax
    leaq 40(%rsp), %rsi
    movb $10, (%rsi)        # newline
    decq %rsi
    movq $4, %rcx
.Lf_dloop:
    xorq %rdx, %rdx
    movq $10, %rbx
    divq %rbx
    addb $48, %dl
    movb %dl, (%rsi)
    decq %rsi
    loop .Lf_dloop
    movq $1, %rax
    movq $1, %rdi
    leaq 37(%rsp), %rsi
    movq $5, %rdx
    syscall

.Lprintf_done:
    xorq %rax, %rax
    addq $88, %rsp
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
