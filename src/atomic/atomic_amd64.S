//.intel_syntax noprefix
.text
   .globl a_cas, a_swap, a_store, a_incr, a_decr, a_spin, a_fetch_add, a_and, a_and64, a_or, a_or64, a_barrier, a_crash, a_bsf

    a_cas:
        pushq   %rbp
        movq    %rsp, %rbp
        movq    %rdi, -8(%rbp)
        movl    %esi, -12(%rbp)
        movl    %edx, -16(%rbp)
        movq    -8(%rbp), %rcx
        movl    -12(%rbp), %eax
        movl    -16(%rbp), %edx
        lock 
        cmpxchg %edx, (%rcx)
        movl    %eax, -12(%rbp)
        movl    -12(%rbp), %eax
        popq    %rbp
        ret
    a_swap:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movl	%esi, -12(%rbp)
        movq	-8(%rbp), %rdx
        movl	-12(%rbp), %eax
        xchg    %eax, (%rdx)
        movl	%eax, -12(%rbp)
        movl	-12(%rbp), %eax
        popq	%rbp
        ret
    a_store:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movl	%esi, -12(%rbp)
        movq	-8(%rbp), %rax
        movl	-12(%rbp), %edx
        mov     %edx, (%rax)
        lock
        orl     $0,(%rsp)
        popq	%rbp
        ret
    a_incr:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movq	-8(%rbp), %rax
        movq	-8(%rbp), %rdx
        lock
        incl    (%rax)
        popq	%rbp
        ret
    a_decr:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movq	-8(%rbp), %rax
        movq	-8(%rbp), %rdx
        lock
        decl    (%rax)
        popq	%rbp
        ret
    a_spin:
        pushq	%rbp
        movq	%rsp, %rbp
        pause
        popq	%rbp
        ret
    a_fetch_add:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movl	%esi, -12(%rbp)
        movq	-8(%rbp), %rdx
        movl	-12(%rbp), %eax
        lock
        xadd    %eax, (%rdx)
        movl	%eax, -12(%rbp)
        movl	-12(%rbp), %eax
        popq	%rbp
        ret
    a_and:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movl	%esi, -12(%rbp)
        movq	-8(%rbp), %rax
        movl	-12(%rbp), %edx
        lock
        and     %edx, (%rax)
        popq	%rbp
        ret
    a_or:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movl	%esi, -12(%rbp)
        movq	-8(%rbp), %rax
        movl	-12(%rbp), %edx
        lock ; or %edx, (%rax)
        popq	%rbp
        ret
    a_and64:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movq	%rsi, -16(%rbp)
        movq	-8(%rbp), %rax
        movq	-16(%rbp), %rdx
        lock
        and     %rdx, (%rax)
        popq	%rbp
        ret
    a_or64:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movq	%rsi, -16(%rbp)
        movq	-8(%rbp), %rax
        movq	-16(%rbp), %rdx
        lock
        or      %rdx, (%rax)
        popq	%rbp
        ret
    a_barrier:
        pushq	%rbp
        movq	%rsp, %rbp
        popq	%rbp
        ret
    a_crash:
        pushq	%rbp
        movq	%rsp, %rbp
        hlt
        popq	%rbp
        ret
    a_bsf:
        pushq	%rbp
        movq	%rsp, %rbp
        movq	%rdi, -8(%rbp)
        movq	-8(%rbp), %rax
        bsf     %rax,%rax
        movq	%rax, -8(%rbp)
        movq	-8(%rbp), %rax
        popq	%rbp
        ret


//    a_cas:
//        mov rax,rsi
//        lock cmpxchg [rdi],rdx
//        ret
//    a_swap:
//        mov rax, [rdi]
//        lock xchg [rdi], rsi 
//        ret
//    a_store:
//        mov [rdi], rsi
//        ret
