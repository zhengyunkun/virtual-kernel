#include <linux/syscalls.h>
#include <linux/pid_namespace.h>

#include "syscalls/syscalls.h"
#include "vkernel.h"

/**
 * syscall_install - syscall virtualization initialization.
 * @vkn: vkernel structure pointer
 *
 * This function initializes the vkernel container's system call table, and initializes the relevant
 * subsystems in the vkernel module.
 */
void __init syscall_install(struct vkernel *vkn)
{
    sys_call_ptr_t *sys_call_table = vkn->sys_call_table;

    // [202] futex()
    /*futex_init(); // init futex subsystem
    sys_call_table[__NR_futex] = (sys_call_ptr_t)&__x64_sys_futex;
    vkn->do_futex = do_futex;*/
}
