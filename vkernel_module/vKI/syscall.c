#include"custom.h" 
#include <linux/syscalls.h>
#include <linux/pid_namespace.h>

#include "vkernel.h"
#include "syscalls/syscalls.h"

/**
 * vkn_syscall_init - vkernel syscall data initialization
 * @vkn: vkernel structure pointer
 *
 * This function initializes the vkernel's system call subsystem by calling the syscall's
 * initialization function.
 */
void __init vkn_syscall_init(struct vkernel *vkn)
{
    vkn_futex_init(vkn);
    // TODO: Initialize other syscall data, e.g. vkn_read_init(), vkn_clone_init() ...
}

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
    vkn_syscall_init(vkn);

    sys_call_table[__NR_futex] = (sys_call_ptr_t)&vkn_sys_futex;

    orig_sys_clone = (void *)sys_call_table[__NR_clone];
    sys_call_table[__NR_clone] = (sys_call_ptr_t)&vkn_sys_clone;

    orig_sys_personality = (void *)sys_call_table[__NR_personality];
    sys_call_table[__NR_personality] = (sys_call_ptr_t)&vkn_sys_personality;

    orig_sys_pkey_free = (void *)sys_call_table[__NR_pkey_free];
    sys_call_table[__NR_pkey_free] = (sys_call_ptr_t)&vkn_sys_pkey_free;

    orig_sys_fsopen = (void *)sys_call_table[__NR_fsopen];
    sys_call_table[__NR_fsopen] = (sys_call_ptr_t)&vkn_sys_fsopen;

    orig_sys_add_key = (void *)sys_call_table[__NR_add_key];
    sys_call_table[__NR_add_key] = (sys_call_ptr_t)&vkn_sys_add_key;

    orig_sys_swapon = (void *)sys_call_table[__NR_swapon];
    sys_call_table[__NR_swapon] = (sys_call_ptr_t)&vkn_sys_swapon;

    orig_sys_ustat = (void *)sys_call_table[__NR_ustat];
    sys_call_table[__NR_ustat] = (sys_call_ptr_t)&vkn_sys_ustat;

    orig_sys_kexec_file_load = (void *)sys_call_table[__NR_kexec_file_load];
    sys_call_table[__NR_kexec_file_load] = (sys_call_ptr_t)&vkn_sys_kexec_file_load;

    orig_sys_move_mount = (void *)sys_call_table[__NR_move_mount];
    sys_call_table[__NR_move_mount] = (sys_call_ptr_t)&vkn_sys_move_mount;

    orig_sys_kexec_load = (void *)sys_call_table[__NR_kexec_load];
    sys_call_table[__NR_kexec_load] = (sys_call_ptr_t)&vkn_sys_kexec_load;

    orig_sys_fsmount = (void *)sys_call_table[__NR_fsmount];
    sys_call_table[__NR_fsmount] = (sys_call_ptr_t)&vkn_sys_fsmount;

    orig_sys_pkey_alloc = (void *)sys_call_table[__NR_pkey_alloc];
    sys_call_table[__NR_pkey_alloc] = (sys_call_ptr_t)&vkn_sys_pkey_alloc;

    orig_sys_keyctl = (void *)sys_call_table[__NR_keyctl];
    sys_call_table[__NR_keyctl] = (sys_call_ptr_t)&vkn_sys_keyctl;

    orig_sys_swapoff = (void *)sys_call_table[__NR_swapoff];
    sys_call_table[__NR_swapoff] = (sys_call_ptr_t)&vkn_sys_swapoff;

    orig_sys_pkey_mprotect = (void *)sys_call_table[__NR_pkey_mprotect];
    sys_call_table[__NR_pkey_mprotect] = (sys_call_ptr_t)&vkn_sys_pkey_mprotect;

    orig_sys_sysfs = (void *)sys_call_table[__NR_sysfs];
    sys_call_table[__NR_sysfs] = (sys_call_ptr_t)&vkn_sys_sysfs;

    orig_sys_ni_syscall = (void *)sys_call_table[__NR__sysctl];
    sys_call_table[__NR__sysctl] = (sys_call_ptr_t)&vkn_sys_ni_syscall;

    orig_sys_request_key = (void *)sys_call_table[__NR_request_key];
    sys_call_table[__NR_request_key] = (sys_call_ptr_t)&vkn_sys_request_key;

    orig_sys_migrate_pages = (void *)sys_call_table[__NR_migrate_pages];
    sys_call_table[__NR_migrate_pages] = (sys_call_ptr_t)&vkn_sys_migrate_pages;

    orig_sys_open_tree = (void *)sys_call_table[__NR_open_tree];
    sys_call_table[__NR_open_tree] = (sys_call_ptr_t)&vkn_sys_open_tree;

    orig_sys_fspick = (void *)sys_call_table[__NR_fspick];
    sys_call_table[__NR_fspick] = (sys_call_ptr_t)&vkn_sys_fspick;

    orig_sys_userfaultfd = (void *)sys_call_table[__NR_userfaultfd];
    sys_call_table[__NR_userfaultfd] = (sys_call_ptr_t)&vkn_sys_userfaultfd;

    orig_sys_pivot_root = (void *)sys_call_table[__NR_pivot_root];
    sys_call_table[__NR_pivot_root] = (sys_call_ptr_t)&vkn_sys_pivot_root;

    orig_sys_fsconfig = (void *)sys_call_table[__NR_fsconfig];
    sys_call_table[__NR_fsconfig] = (sys_call_ptr_t)&vkn_sys_fsconfig;

    orig_sys_clone3 = (void *)sys_call_table[__NR_clone3];
    sys_call_table[__NR_clone3] = (sys_call_ptr_t)&vkn_sys_clone3;

    orig_sys_move_pages = (void *)sys_call_table[__NR_move_pages];
    sys_call_table[__NR_move_pages] = (sys_call_ptr_t)&vkn_sys_move_pages;

}
