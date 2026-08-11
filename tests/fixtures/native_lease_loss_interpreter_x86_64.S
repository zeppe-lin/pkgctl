# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Test-only x86-64 Linux interpreter for the CLI outer-lease-loss vertical.
 *
 * Ordinary build/check programs receive the same deterministic payload as the
 * shared native interpreter.  The dedicated lifecycle recipe begins with '#';
 * that marked program instead performs one post-install target write and a
 * FIFO handshake with native-target-lock-revoker.  The lifecycle process does
 * not return until the revoker has unlinked the anchored POSIX target lock.
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
    cmpb $'#', (%rax)
    je .lifecycle

.build:
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
    jmp .succeeded

.lifecycle:
    mov $257, %rax
    mov $-100, %rdi
    lea post_install_path(%rip), %rsi
    mov $577, %rdx
    mov $0644, %r10
    syscall
    test %rax, %rax
    js .failed
    mov %rax, %r12

    lea post_install_body(%rip), %r13
    mov $post_install_body_size, %r14
.write_post_install:
    mov $1, %rax
    mov %r12, %rdi
    mov %r13, %rsi
    mov %r14, %rdx
    syscall
    test %rax, %rax
    jle .failed_open
    add %rax, %r13
    sub %rax, %r14
    jne .write_post_install

    mov $3, %rax
    mov %r12, %rdi
    syscall

    mov $37, %rax
    mov $10, %rdi
    syscall

    mov $257, %rax
    mov $-100, %rdi
    lea ready_path(%rip), %rsi
    mov $1, %rdx
    xor %r10, %r10
    syscall
    test %rax, %rax
    js .failed
    mov %rax, %r12

    mov $1, %rax
    mov %r12, %rdi
    lea ready_body(%rip), %rsi
    mov $ready_body_size, %rdx
    syscall
    cmp $ready_body_size, %rax
    jne .failed_open

    mov $3, %rax
    mov %r12, %rdi
    syscall

    mov $257, %rax
    mov $-100, %rdi
    lea acknowledge_path(%rip), %rsi
    xor %rdx, %rdx
    xor %r10, %r10
    syscall
    test %rax, %rax
    js .failed
    mov %rax, %r12

    xor %rax, %rax
    mov %r12, %rdi
    lea acknowledge_byte(%rip), %rsi
    mov $1, %rdx
    syscall
    cmp $1, %rax
    jne .failed_open

    mov $37, %rax
    xor %rdi, %rdi
    syscall

    mov $3, %rax
    mov %r12, %rdi
    syscall

.succeeded:
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
post_install_path:
    .asciz "/target/post-install-ran"
post_install_body:
    .ascii "post-install ran\n"
.set post_install_body_size, . - post_install_body
ready_path:
    .asciz "/target/.pkgctl-test-lease-loss-ready"
ready_body:
    .ascii "ready\n"
.set ready_body_size, . - ready_body
acknowledge_path:
    .asciz "/target/.pkgctl-test-lease-loss-ack"
.bss
acknowledge_byte:
    .zero 1
