/* SPDX-License-Identifier: GPL-2.0 */

/*
 * VKernel framework generic data struct, function defination.
 */

#ifndef _VKERNEL_H
#define _VKERNEL_H

#include <linux/types.h>
#include <linux/pid_namespace.h>
#include <linux/vknhash.h>
#include <asm/syscall.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

/* level: INFO, DEBUG, ERR, etc. */
#define VKERNEL_LOG(level, fmt, args...) \
	printk(KERN_##level "%s: " fmt, vkn->name, ##args)

#define VKNHASH_BITS 8

struct vkernel {

	char name[50];

	/* syscall virtualization */
	sys_call_ptr_t sys_call_table[__NR_syscall_max+1];
	
	/*task_struct->cred->cap_effective*/
	kernel_cap_t	cap_effective;
	
	/* hashmaps which contain access rights */
	struct Vkernel_hashmap *vknhash_reg;
	struct Vkernel_hashmap *vknhash_dir;
};

#endif /* _VKERNEL_H */
