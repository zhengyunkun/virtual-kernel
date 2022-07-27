// SPDX-License-Identifier: GPL-2.0

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <asm/set_memory.h>

#include "vkernel.h"
#include "capability.h"
#include "apparmor.h"
#include "syscall.h"

/* init process of container, used to find the pid namespace of the container */
static struct task_struct *init_process;
/* the pid namespace to which vkernel attaches */
static struct pid_namespace *pid_ns;
struct vkernel *vkn;

static char *name;  // module name
static unsigned long caps_data[6] = {0};    // effective, permitted, inheritable
static unsigned long caps_bounding[2] = {0};  // bounding
static unsigned long caps_ambient[2] = {0}; // ambient
static int caps_data_num = 6;
static int caps_bounding_num = 2;
static int caps_ambient_num = 2;
module_param(name, charp, 0400);
module_param_array(caps_data, ulong, &caps_data_num, 0400);
module_param_array(caps_bounding, ulong, &caps_bounding_num, 0400);
module_param_array(caps_ambient, ulong, &caps_ambient_num, 0400);

typedef void (*vkn_log_flush_ptr_t)(void);

/**
 * pid_ns_enable_vkernel - enable vkernel through the pid namespace
 */
static void pid_ns_enable_vkernel(void)
{
	pid_ns->vkernel = (void *)vkn;
	// TODO: remove the following structure members from pid_namespace structure in kernel
	pid_ns->sys_call_table = vkn->sys_call_table;
	pid_ns->vknhash_dir = vkn->vknhash_dir;
	pid_ns->vknhash_reg = vkn->vknhash_reg;
	pid_ns->asi_enabled = false;
	pid_ns->asi = NULL;
	pid_ns->dpt = NULL;
}

/**
 * pid_ns_disable_vkernel - disable vkernel through the pid namespace
 */
static void pid_ns_disable_vkernel(void)
{
	// Vkernel log flush means when vkernel exit, make the vkernel log visible only in host namespace
	vkn_log_flush_ptr_t vkn_log_flush = NULL;
	vkn_log_flush = (vkn_log_flush_ptr_t)kallsyms_lookup_name("vkn_log_flush");
	vkn_log_flush();

	pid_ns->vkernel = NULL;
	// TODO: remove the following structure members from pid_namespace structure in kernel
	pid_ns->sys_call_table = NULL;
	pid_ns->vknhash_reg = NULL;
	pid_ns->vknhash_dir = NULL;
	pid_ns->asi_enabled = false;
	pid_ns->asi = NULL;
	pid_ns->dpt = NULL;
}

/**
 * sub_vkernel_struct_init - initialize vkernel hashmap.
 */
static bool vknhash_struct_init(void)
{
	vkn->vknhash_reg = CreateVkernel_hashmap(VKNHASH_BITS);
	if (!vkn->vknhash_reg)
		goto err;
	vkn->vknhash_dir = CreateVkernel_hashmap(VKNHASH_BITS);
	if (!vkn->vknhash_dir)
		goto err;
	return true;
err:
	if (vkn->vknhash_reg)
		DeleteVkernel_hashmap(vkn->vknhash_reg);
	if (vkn->vknhash_dir)
		DeleteVkernel_hashmap(vkn->vknhash_dir);
	return false;
}

/**
 * vkernel_struct_init - initialize a vkernel struct
 */
static bool vkernel_struct_init(void)
{
	sys_call_ptr_t *sys_call_table;
	int i;

	/**
	 * Step1: Allocate memory for the vkernel structure.
	 */
	vkn = kzalloc(sizeof(struct vkernel), GFP_KERNEL);
	if (!vkn)
		goto err;

	/**
	 * Step2: Initialize the name of the vkernel module.
	 */
	strcpy(vkn->name, name);

	/**
	 * Step3: Initialize the system call table of the vkernel container.
	 */
	// TODO: kallsyms_lookup_name() alternative?
	// see: https://github.com/torvalds/linux/commit/0bd476e6c67190b5eb7b6e105c8db8ff61103281
	sys_call_table = (sys_call_ptr_t *)kallsyms_lookup_name("sys_call_table");
	if (!sys_call_table) {
		VKERNEL_LOG(ERR, "can't find sys_call_table\n");
		goto err;
	} else {
		VKERNEL_LOG(INFO, "find address of sys_call_table: 0x%lx\n", (unsigned long)sys_call_table);
	}
	for (i = 0; i < __NR_syscall_max; i++)
		vkn->sys_call_table[i] = *(sys_call_table + i);
	VKERNEL_LOG(INFO, "success to create syscall table: 0x%lx\n", (unsigned long)vkn->sys_call_table);
	
	/*record task's cap_effective*/
	vkn->cap_effective=init_process->cred->cap_effective;
	init_process->vkn_cap_effective=vkn->cap_effective;

	// TODO: other vkernel structure members initialization

	return true;

err:
	return false;
}

static int __init vkernel_init(void)
{
	int ret = 0;

	/**
	 * The vkernel module must be loaded during the container init process, in order for the init_process
	 * struct to correspond to the container's init process.
	 */
	init_process = current;

	/**
	 * Bind the vkernel module to the pid namespace of the container.
	 */
	pid_ns = task_active_pid_ns(init_process);
	if (!pid_ns)
		goto err;

	/**
	 * Vkernel structure initialization.
	 */
	vkernel_struct_init();
	if (!vkn)
		goto err;

	if (!vknhash_struct_init())
		goto err;
	if (!vkernel_hash_init())
		goto err;

	/**
	 * Syscall virtualization initialization.
	 */
	syscall_install(vkn);

	if (task_set_capability(init_process, caps_data, caps_bounding, caps_ambient) != 0)
		goto err;

	/* pid_ns_enable_vkernel is the last step to complete module initialization */
	pid_ns_enable_vkernel();

	VKERNEL_LOG(INFO, "success to init.\n");
	return ret;

err:
	VKERNEL_LOG(ERR, "fail to init.\n");
	kfree(vkn);
	return -1;
}

static void vkernel_exit(void)
{
	pid_ns_disable_vkernel();
	kfree(vkn);
	VKERNEL_LOG(INFO, "exit.\n");
}

module_init(vkernel_init);
module_exit(vkernel_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("vkernel module");
