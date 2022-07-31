// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "vkernel_hook: " fmt

#include <linux/futex.h>

#include "hook.h"
#include "../vKI/vkernel.h"

#define GET_CURRENT_VKERNEL ((struct vkernel *)get_task_vkernel(current))

/**
 * Vkernel container is bound to the pid namespace, to prevent escape, the creation of a new pid namespace
 * by a vkernel container is prohibited.
 */
static struct nsproxy *(*real_create_new_namespaces)(unsigned long flags,
	struct task_struct *tsk, struct user_namespace *user_ns,
	struct fs_struct *new_fs);

static struct nsproxy *vkn_create_new_namespaces(unsigned long flags,
	struct task_struct *tsk, struct user_namespace *user_ns,
	struct fs_struct *new_fs)
{
	struct nsproxy *ns;

	// disallow the flag CLONE_NEWPID
	if (flags & CLONE_NEWPID) {
		pr_info("the parameter CLONE_NEWPID for create_new_namespace() is not allowed!\n");
		ns = ERR_PTR(-EINVAL);
	} else {
		pr_debug("create_new_namespaces() before\n");
		ns = real_create_new_namespaces(flags, tsk, user_ns, new_fs);
		pr_debug("create_new_namespaces() after\n");
	}

	return ns;
}

/**
 * When other kernel functions which don't exist in syscall futex() call chain call do_futex(), hook it.
 */
static long (*real_do_futex)(u32 __user *uaddr, int op, u32 val, ktime_t *timeout,
		u32 __user *uaddr2, u32 val2, u32 val3);

static long vkn_do_futex(u32 __user *uaddr, int op, u32 val, ktime_t *timeout,
		u32 __user *uaddr2, u32 val2, u32 val3)
{
	long ret;

	pr_debug("do_futex() before.\n");
	ret = GET_CURRENT_VKERNEL->do_futex(current->clear_child_tid, FUTEX_WAKE, 1, NULL, NULL, 0, 0);
	pr_debug("do_futex() after.\n");

	return ret;
}

static struct ftrace_hook hooks[] = {
	//HOOK("create_new_namespaces", vkn_create_new_namespaces, &real_create_new_namespaces),
	HOOK("do_futex",  vkn_do_futex,  &real_do_futex),
};

static int vkernel_hook_init(void)
{
	int err;

	err = fh_install_hooks(hooks, ARRAY_SIZE(hooks));
	if (err)
		return err;
	
	pr_info("vkernel_hook module loaded\n");

	return 0;
}
module_init(vkernel_hook_init);

static void vkernel_hook_exit(void)
{
	fh_remove_hooks(hooks, ARRAY_SIZE(hooks));

	pr_info("vkernel_hook module unloaded\n");
}
module_exit(vkernel_hook_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("vkernel module for hooking kernel function");
