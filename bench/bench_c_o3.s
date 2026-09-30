	.file	"bench.c"
	.text
	.p2align 4
	.globl	estimate_pi
	.type	estimate_pi, @function
estimate_pi:
.LFB23:
	.cfi_startproc
	endbr64
	movsd	.LC1(%rip), %xmm5
	movapd	%xmm0, %xmm1
	movapd	%xmm5, %xmm0
	divsd	%xmm1, %xmm0
	testl	%edi, %edi
	jle	.L4
	movsd	.LC2(%rip), %xmm1
	movsd	.LC3(%rip), %xmm6
	xorl	%eax, %eax
	pxor	%xmm3, %xmm3
	mulsd	%xmm0, %xmm1
	.p2align 6
	.p2align 4
	.p2align 3
.L3:
	movapd	%xmm1, %xmm2
	movapd	%xmm6, %xmm4
	addl	$1, %eax
	mulsd	%xmm1, %xmm2
	addsd	%xmm0, %xmm1
	addsd	%xmm5, %xmm2
	divsd	%xmm2, %xmm4
	addsd	%xmm4, %xmm3
	cmpl	%eax, %edi
	jne	.L3
	mulsd	%xmm3, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L4:
	pxor	%xmm3, %xmm3
	mulsd	%xmm3, %xmm0
	ret
	.cfi_endproc
.LFE23:
	.size	estimate_pi, .-estimate_pi
	.p2align 4
	.globl	fib
	.type	fib, @function
fib:
.LFB24:
	.cfi_startproc
	endbr64
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	movl	%edi, %ebx
	subq	$104, %rsp
	.cfi_def_cfa_offset 160
	cmpl	$1, %edi
	jle	.L43
	leal	-1(%rdi), %edx
	movl	%edi, %r13d
	xorl	%r14d, %r14d
	movl	%edx, %eax
	andl	$-2, %eax
	subl	%eax, %r13d
	movl	%edx, %eax
	cmpl	%r13d, %ebx
	je	.L52
.L9:
	leal	-2(%rbx), %edx
	movl	%eax, %ebp
	movl	%r13d, 44(%rsp)
	xorl	%r12d, %r12d
	andl	$-2, %edx
	leal	-2(%rbx), %r13d
	subl	%edx, %ebp
	movl	%ebp, %ebx
.L15:
	leal	-1(%rax), %edx
	cmpl	%ebx, %eax
	je	.L53
	leal	-2(%rax), %ebp
	movl	%edx, %ecx
	movl	%ebx, 52(%rsp)
	xorl	%r15d, %r15d
	movl	%ebp, %eax
	movl	%r12d, 48(%rsp)
	movl	%r13d, %r12d
	movl	%r14d, %r13d
	andl	$-2, %eax
	subl	%eax, %ecx
	movl	%ecx, 60(%rsp)
.L19:
	leal	-1(%rdx), %r8d
	cmpl	60(%rsp), %edx
	je	.L54
	leal	-2(%rdx), %edi
	movl	%r8d, %eax
	movl	%r15d, 56(%rsp)
	movl	%r12d, %r14d
	andl	$-2, %edi
	xorl	%ebx, %ebx
	leal	-2(%rdx), %esi
	movl	%r13d, %r12d
	subl	%edi, %eax
	movl	%eax, 64(%rsp)
.L23:
	leal	-1(%r8), %r10d
	cmpl	64(%rsp), %r8d
	je	.L55
	leal	-2(%r8), %ecx
	movl	%r10d, %r15d
	movl	%ebp, %edx
	movl	%esi, %r11d
	andl	$-2, %ecx
	movl	%ebx, %esi
	movl	%r12d, %r9d
	movl	%r14d, %ebx
	subl	%ecx, %r15d
	movl	%r10d, %ebp
	leal	-2(%r8), %ecx
	xorl	%r14d, %r14d
.L27:
	leal	-1(%rbp), %eax
	cmpl	%r15d, %ebp
	je	.L56
	subl	$2, %ebp
	movl	%eax, %r8d
	xorl	%r12d, %r12d
	movl	%r9d, %r10d
	movl	%ebp, %edi
	movl	%esi, %r13d
	andl	$-2, %edi
	subl	%edi, %r8d
	movl	%r8d, 24(%rsp)
.L31:
	cmpl	24(%rsp), %eax
	je	.L57
	leal	-2(%rax), %edi
	leal	-3(%rax), %esi
	subl	$5, %eax
	movl	%edx, 12(%rsp)
	movl	%edi, %r8d
	movl	%esi, %r9d
	movl	%ecx, 16(%rsp)
	andl	$-2, %r8d
	subl	%r8d, %r9d
	movl	%esi, %r8d
	andl	$-2, %r8d
	movl	%r9d, 28(%rsp)
	movl	%ebx, %r9d
	xorl	%ebx, %ebx
	subl	%r8d, %eax
	leal	1(%rsi), %r8d
	movl	%eax, 20(%rsp)
	cmpl	%esi, 28(%rsp)
	je	.L58
.L40:
	movl	%esi, 36(%rsp)
	movl	%esi, %edx
	xorl	%ecx, %ecx
	movl	%r10d, 32(%rsp)
	movl	%edi, %r10d
	movl	%ebp, 40(%rsp)
	movl	%ebx, %ebp
.L32:
	movl	%edx, %esi
	cmpl	$1, %edx
	je	.L50
	xorl	%ebx, %ebx
.L36:
	leal	-1(%rsi), %edi
	movl	%r9d, 92(%rsp)
	movl	%r10d, 88(%rsp)
	movl	%r11d, 84(%rsp)
	movl	%edx, 80(%rsp)
	movl	%ecx, 76(%rsp)
	movl	%r8d, 72(%rsp)
	movl	%esi, 68(%rsp)
	call	fib
	movl	68(%rsp), %esi
	movl	72(%rsp), %r8d
	addl	%eax, %ebx
	movl	76(%rsp), %ecx
	movl	80(%rsp), %edx
	subl	$2, %esi
	movl	84(%rsp), %r11d
	movl	88(%rsp), %r10d
	cmpl	$1, %esi
	movl	92(%rsp), %r9d
	jg	.L36
	leal	-3(%r8), %edi
	subl	$2, %edx
	subl	$2, %r8d
	andl	$-2, %edi
	movl	%edx, %eax
	subl	%edi, %eax
	addl	%ebx, %eax
	addl	%eax, %ecx
	cmpl	$1, %r8d
	jne	.L32
	.p2align 4
	.p2align 3
.L50:
	movl	36(%rsp), %esi
	movl	%ebp, %ebx
	leal	1(%rcx), %edx
	movl	%r10d, %edi
	addl	%edx, %ebx
	movl	40(%rsp), %ebp
	movl	32(%rsp), %r10d
	leal	-2(%rsi), %edx
	cmpl	%edx, 20(%rsp)
	je	.L34
	movl	%edx, %esi
	leal	1(%rsi), %r8d
	cmpl	%esi, 28(%rsp)
	jne	.L40
.L58:
	movl	12(%rsp), %edx
	movl	16(%rsp), %ecx
	leal	(%r8,%rbx), %esi
	movl	%r9d, %ebx
.L33:
	movl	%edi, %eax
	addl	%esi, %r12d
	cmpl	$1, %edi
	jne	.L31
	movl	%r10d, %r9d
	movl	%r13d, %esi
	addl	$1, %r12d
	jmp	.L29
	.p2align 4,,10
	.p2align 3
.L57:
	movl	%r10d, %r9d
	movl	%r13d, %esi
	leal	-1(%rax,%r12), %r12d
.L29:
	addl	%r12d, %r14d
	cmpl	$1, %ebp
	jne	.L27
	leal	1(%r14), %r13d
	movl	%r9d, %r12d
	movl	%ebx, %r14d
	movl	%edx, %ebp
	movl	%esi, %ebx
	movl	%ecx, %r8d
	movl	%r11d, %esi
.L25:
	addl	%r13d, %ebx
	cmpl	$1, %r8d
	jne	.L23
	movl	%r12d, %r13d
	movl	56(%rsp), %r15d
	movl	%esi, %edx
	movl	%r14d, %r12d
	leal	1(%rbx), %edi
	jmp	.L21
	.p2align 4,,10
	.p2align 3
.L56:
	leal	-1(%rbp,%r14), %r13d
	movl	%r9d, %r12d
	movl	%ebx, %r14d
	movl	%edx, %ebp
	movl	%esi, %ebx
	movl	%ecx, %r8d
	movl	%r11d, %esi
	jmp	.L25
.L55:
	movl	56(%rsp), %r15d
	movl	%r12d, %r13d
	movl	%esi, %edx
	movl	%r14d, %r12d
	leal	-1(%r8,%rbx), %edi
.L21:
	addl	%edi, %r15d
	cmpl	$1, %edx
	jne	.L19
	movl	%r13d, %r14d
	movl	52(%rsp), %ebx
	movl	%r12d, %r13d
	leal	1(%r15), %edx
	movl	48(%rsp), %r12d
	jmp	.L17
	.p2align 4,,10
	.p2align 3
.L54:
	movl	%r13d, %r14d
	movl	52(%rsp), %ebx
	movl	%r12d, %r13d
	movl	48(%rsp), %r12d
	leal	-1(%rdx,%r15), %edx
.L17:
	movl	%ebp, %eax
	addl	%edx, %r12d
	cmpl	$1, %ebp
	jne	.L15
	movl	%r13d, %ebx
	leal	1(%r12), %eax
	movl	44(%rsp), %r13d
	addl	%eax, %r14d
	cmpl	$1, %ebx
	jne	.L59
.L44:
	leal	1(%r14), %ebx
	jmp	.L43
	.p2align 4,,10
	.p2align 3
.L53:
	movl	%r13d, %ebx
	leal	-1(%rax,%r12), %eax
	movl	44(%rsp), %r13d
	addl	%eax, %r14d
	cmpl	$1, %ebx
	je	.L44
.L59:
	leal	-1(%rbx), %edx
	movl	%edx, %eax
	cmpl	%r13d, %ebx
	jne	.L9
.L52:
	leal	(%rdx,%r14), %ebx
.L43:
	addq	$104, %rsp
	.cfi_remember_state
	.cfi_def_cfa_offset 56
	movl	%ebx, %eax
	popq	%rbx
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
.L34:
	.cfi_restore_state
	addl	%ebx, %esi
	movl	12(%rsp), %edx
	movl	16(%rsp), %ecx
	movl	%r9d, %ebx
	jmp	.L33
	.cfi_endproc
.LFE24:
	.size	fib, .-fib
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align 8
.LC5:
	.string	"=================================================="
	.align 8
.LC6:
	.string	"              C Benchmark (GCC)                   "
	.align 8
.LC7:
	.string	"[1] Running 10,000,000 float integration steps to compute Pi..."
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC9:
	.string	"Result (Estimated Pi):\n%g\n"
	.section	.rodata.str1.8
	.align 8
.LC10:
	.string	"[2] Running recursive Fibonacci(35)..."
	.section	.rodata.str1.1
.LC11:
	.string	"Result (Fibonacci 35):\n%d\n"
	.section	.text.startup,"ax",@progbits
	.p2align 4
	.globl	main
	.type	main, @function
main:
.LFB25:
	.cfi_startproc
	endbr64
	pushq	%r13
	.cfi_def_cfa_offset 16
	.cfi_offset 13, -16
	leaq	.LC5(%rip), %rdi
	pushq	%r12
	.cfi_def_cfa_offset 24
	.cfi_offset 12, -24
	pushq	%rbp
	.cfi_def_cfa_offset 32
	.cfi_offset 6, -32
	pushq	%rbx
	.cfi_def_cfa_offset 40
	.cfi_offset 3, -40
	subq	$8, %rsp
	.cfi_def_cfa_offset 48
	call	puts@PLT
	leaq	.LC6(%rip), %rdi
	call	puts@PLT
	leaq	.LC5(%rip), %rdi
	call	puts@PLT
	leaq	.LC7(%rip), %rdi
	call	puts@PLT
	movsd	.LC4(%rip), %xmm1
	movsd	.LC1(%rip), %xmm6
	movl	$10000000, %eax
	movsd	.LC3(%rip), %xmm5
	movsd	.LC8(%rip), %xmm4
	pxor	%xmm0, %xmm0
	.p2align 6
	.p2align 4
	.p2align 3
.L61:
	movapd	%xmm1, %xmm2
	movapd	%xmm5, %xmm3
	mulsd	%xmm1, %xmm2
	addsd	%xmm4, %xmm1
	addsd	%xmm6, %xmm2
	divsd	%xmm2, %xmm3
	addsd	%xmm3, %xmm0
	subl	$1, %eax
	jne	.L61
	mulsd	%xmm4, %xmm0
	leaq	.LC9(%rip), %rsi
	movl	$2, %edi
	movl	$1, %eax
	movl	$34, %r12d
	xorl	%r13d, %r13d
	call	__printf_chk@PLT
	leaq	.LC10(%rip), %rdi
	call	puts@PLT
.L62:
	movl	%r12d, %ebx
	xorl	%ebp, %ebp
.L63:
	leal	-1(%rbx), %edi
	subl	$2, %ebx
	call	fib
	addl	%eax, %ebp
	cmpl	$1, %ebx
	jg	.L63
	addl	%ebp, %r13d
	subl	$2, %r12d
	jne	.L62
	leal	1(%r13), %edx
	leaq	.LC11(%rip), %rsi
	movl	$2, %edi
	xorl	%eax, %eax
	call	__printf_chk@PLT
	leaq	.LC5(%rip), %rdi
	call	puts@PLT
	addq	$8, %rsp
	.cfi_def_cfa_offset 40
	xorl	%eax, %eax
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%rbp
	.cfi_def_cfa_offset 24
	popq	%r12
	.cfi_def_cfa_offset 16
	popq	%r13
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
.LFE25:
	.size	main, .-main
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC1:
	.long	0
	.long	1072693248
	.align 8
.LC2:
	.long	0
	.long	1071644672
	.align 8
.LC3:
	.long	0
	.long	1074790400
	.align 8
.LC4:
	.long	-1698910392
	.long	1047189490
	.align 8
.LC8:
	.long	-1698910392
	.long	1048238066
	.ident	"GCC: (Ubuntu 15.2.0-16ubuntu1) 15.2.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
