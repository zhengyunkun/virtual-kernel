#ifndef _VKERNEL_SYSCALLS_H
#define _VKERNEL_SYSCALLS_H

/* APIs for syscall @futex required by syscall virtualization */
long vkn_sys_futex(const struct pt_regs *regs);
void __init vkn_futex_init(void *vkn);
void vkn_futex_cleanup(void *vkn);

#endif /* _VKERNEL_SYSCALLS_H */