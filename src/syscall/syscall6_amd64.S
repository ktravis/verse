.text
   .globl __syscall5, __syscall6
    __syscall5:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -24(%rbp)
        movq	%rsi, -32(%rbp)
        movq	%rdx, -40(%rbp)
        movq	%rcx, -48(%rbp)
        movq	%r8, -56(%rbp)
        movq	%r9, -64(%rbp)
        movq	-56(%rbp), %r10
        movq	-64(%rbp), %r8
        movq	-24(%rbp), %rax
        movq	-32(%rbp), %rdi
        movq	-40(%rbp), %rsi
        movq	-48(%rbp), %rdx
        syscall
        movq	%rax, -8(%rbp)
        movq	-8(%rbp), %rax
        popq	%rbp
        ret
    __syscall6:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -24(%rbp)
        movq	%rsi, -32(%rbp)
        movq	%rdx, -40(%rbp)
        movq	%rcx, -48(%rbp)
        movq	%r8, -56(%rbp)
        movq	%r9, -64(%rbp)
        movq	-56(%rbp), %r10
        movq	-64(%rbp), %r8
        movq	16(%rbp), %r9
        movq	-24(%rbp), %rax
        movq	-32(%rbp), %rdi
        movq	-40(%rbp), %rsi
        movq	-48(%rbp), %rdx
        syscall
        movq	%rax, -8(%rbp)
        movq	-8(%rbp), %rax
        popq	%rbp
        ret
