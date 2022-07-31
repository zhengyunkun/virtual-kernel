/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VKERNEL_HOOK_FTRACE_H
#define _VKERNEL_HOOK_FTRACE_H

#include <linux/ftrace.h>

#define HOOK(_name, _function, _original)	\
	{					\
		.name = (_name),	\
		.new_func = (_function),	\
		.orig_func = (_original),	\
	}

/**
 * struct ftrace_hook - describes a single hook to install
 *
 * @name:     name of the function to hook
 * @new_func: pointer to the function to execute instead
 * @orig_func: pointer to the location where to save a pointer to the original function
 * @address:  kernel address of the function entry
 * @ops:      ftrace_ops state for this function hook
 *
 * The user should fill in only &name, &new_func, &orig_func fields.
 * Other fields are considered implementation details.
 */
struct ftrace_hook {
	const char *name;
	void *new_func;
	void *orig_func;

	unsigned long address;
	struct ftrace_ops ops;
};

int fh_install_hooks(struct ftrace_hook *hooks, size_t count);
void fh_remove_hooks(struct ftrace_hook *hooks, size_t count);

#endif /* _VKERNEL_HOOK_FTRACE_H */
