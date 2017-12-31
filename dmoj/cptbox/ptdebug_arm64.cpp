#define _BSD_SOURCE
#include <sys/ptrace.h>
#include <sys/uio.h>
#include "ptbox.h"

#define ARM_x0 0
#define ARM_x1 1
#define ARM_x2 2
#define ARM_x3 3
#define ARM_x4 4
#define ARM_x5 5
#define ARM_x6 6
#define ARM_x7 7
#define ARM_x8 8

#ifdef PTBOX_NEED_PRE_POST_SYSCALL
#include <elf.h>

void pt_debugger_arm64::pre_syscall() {
    struct iovec iovec;
    iovec.iov_base = arm64_reg;
    iovec.iov_len = sizeof arm64_reg;
    if (ptrace(PTRACE_GETREGSET, tid, NT_PRSTATUS, &iovec))
        perror("ptrace(PTRACE_GETREGSET)");

    arm64_reg_changed = false;
}

void pt_debugger_arm64::post_syscall() {
    if (!arm64_reg_changed)
        return;

    struct iovec iovec;
    iovec.iov_base = arm64_reg;
    iovec.iov_len = sizeof arm64_reg;
    if (ptrace(PTRACE_SETREGSET, tid, NT_PRSTATUS, &iovec))
        perror("ptrace(PTRACE_SETREGSET)");
}
#endif

long pt_debugger_arm64::peek_reg(int reg) {
    return arm64_reg[reg];
}

void pt_debugger_arm64::poke_reg(int reg, long data) {
    arm64_reg[reg] = data;
    arm64_reg_changed = true;
}

int pt_debugger_arm64::syscall() {
    return (int) peek_reg(ARM_x8);
}

// Note that this deliberately doesn't update arm64_reg.
// The kernel only updates x0 on a system call, so x8 must not be changed.
void pt_debugger_arm64::syscall(int id) {
#ifdef PTBOX_NEED_PRE_POST_SYSCALL
#ifndef NT_ARM_SYSTEM_CALL
#define NT_ARM_SYSTEM_CALL 0x404
#endif
    struct iovec iovec;
    iovec.iov_base = &id;
    iovec.iov_len = sizeof id;

    if (ptrace(PTRACE_SETREGSET, tid, NT_ARM_SYSTEM_CALL, &iovec))
        perror("ptrace(PTRACE_SETREGSET, NT_ARM_SYSTEM_CALL)");
#else
    poke_reg(ARM_x8, id);
#endif
}

long pt_debugger_arm64::result() {
    return peek_reg(ARM_x0);
}

void pt_debugger_arm64::result(long value) {
    poke_reg(ARM_x0, value);
}

long pt_debugger_arm64::arg0() {
    return peek_reg(ARM_x0);
}

void pt_debugger_arm64::arg0(long data) {
#if !PTBOX_FREEBSD
    if (is_enter())
        poke_reg(ARM_x0, data);
#endif
}

#define make_arg(id, reg) \
    long pt_debugger_arm64::arg##id() { \
        return peek_reg(reg); \
    } \
    \
    void pt_debugger_arm64::arg##id(long data) {\
        poke_reg(reg, data); \
    }

make_arg(1, ARM_x1);
make_arg(2, ARM_x2);
make_arg(3, ARM_x3);
make_arg(4, ARM_x4);
make_arg(5, ARM_x5);

#undef make_arg

bool pt_debugger_arm64::is_exit(int syscall) {
    return syscall == 93 || syscall == 94;
}

int pt_debugger_arm64::getpid_syscall() {
    return 172;
}

pt_debugger_arm64::pt_debugger_arm64() {
    // execve is actually 221, but...
    // There is no orig_x8 on ARM, and execve clears all registers.
    // Therefore, 0 is the register value when coming out of a system call.
    // We will pretend 0 is execve.
    execve_id = 0;
}