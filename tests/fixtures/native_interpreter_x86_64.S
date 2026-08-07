# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Test-only x86-64 Linux POSIX-program fixture.
 *
 * libpkgexec-linux invokes an inspected interpreter as:
 *
 *   INTERPRETER -c PROGRAM pkgexec
 *
 * This intentionally tiny static executable validates that calling convention,
 * then materializes the payload expected by collections/simple-install. It has
 * no libc or dynamic-loader dependency, so the isolated build root need not
 * copy any host runtime into the fixture.
 */

.global _start
.section .text
_start:
    mov (%rsp), %rax
    cmp $4, %rax
    jne .bad_invocation
    mov 16(%rsp), %rax
    test %rax, %rax
    je .bad_invocation
    cmpb $'-', (%rax)
    jne .bad_invocation
    cmpb $'c', 1(%rax)
    jne .bad_invocation
    cmpb $0, 2(%rax)
    jne .bad_invocation
    mov 24(%rsp), %rax
    test %rax, %rax
    je .bad_invocation

    mov $83, %rax
    lea usr_path(%rip), %rdi
    mov $0755, %rsi
    syscall

    mov $83, %rax
    lea bin_path(%rip), %rdi
    mov $0755, %rsi
    syscall

    mov $257, %rax
    mov $-100, %rdi
    lea payload_path(%rip), %rsi
    mov $577, %rdx
    mov $0644, %r10
    syscall
    test %rax, %rax
    js .failed
    mov %rax, %r12

    lea payload_body(%rip), %r13
    mov $payload_body_size, %r14
.write_payload:
    mov $1, %rax
    mov %r12, %rdi
    mov %r13, %rsi
    mov %r14, %rdx
    syscall
    test %rax, %rax
    jle .failed_open
    add %rax, %r13
    sub %rax, %r14
    jne .write_payload

    mov $91, %rax
    mov %r12, %rdi
    mov $0755, %rsi
    syscall
    test %rax, %rax
    js .failed_open

    mov $3, %rax
    mov %r12, %rdi
    syscall

    mov $60, %rax
    xor %rdi, %rdi
    syscall

.failed_open:
    mov $3, %rax
    mov %r12, %rdi
    syscall
.failed:
    mov $60, %rax
    mov $125, %rdi
    syscall
.bad_invocation:
    mov $60, %rax
    mov $124, %rdi
    syscall

.section .rodata
usr_path:
    .asciz "/build/package/usr"
bin_path:
    .asciz "/build/package/usr/bin"
payload_path:
    .asciz "/build/package/usr/bin/pkgctl-fixture"
payload_body:
    .ascii "pkgctl integration fixture\n"
.set payload_body_size, . - payload_body
