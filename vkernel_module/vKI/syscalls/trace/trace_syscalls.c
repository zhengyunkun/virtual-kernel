// SPDX-License-Identifier: GPL-2.0

/* from kernel/trace/trace_syscalls.c */

#include <linux/trace_events.h>
#include <trace/syscall.h>

#include "kallsyms.h"


// 206
#define SYSCALL_FIELD(_type, _name) {					\
	.type = #_type, .name = #_name,					\
	.size = sizeof(_type), .align = __alignof__(_type),		\
	.is_signed = is_signed_type(_type), .filter_type = FILTER_OTHER }

// 465
static int init_syscall_trace(struct trace_event_call *call)
{
	int id;
	int num;

	num = ((struct syscall_metadata *)call->data)->syscall_nr;
	if (num < 0 || num >= NR_syscalls) {
		pr_debug("syscall %s metadata not mapped, disabling ftrace event\n",
				((struct syscall_metadata *)call->data)->name);
		return -ENOSYS;
	}

	if (set_syscall_print_fmt(call) < 0)
		return -ENOMEM;

	id = trace_event_raw_init(call);

	if (id < 0) {
		free_syscall_print_fmt(call);
		return id;
	}

	return id;
}

// 490
static struct trace_event_fields __refdata syscall_enter_fields_array[] = {
	SYSCALL_FIELD(int, __syscall_nr),
	{ .type = TRACE_FUNCTION_TYPE,
	  .define_fields = syscall_enter_define_fields },
	{}
};

// 501
struct trace_event_functions exit_syscall_print_funcs = {
	.trace		= print_syscall_exit,
};

// 505
struct trace_event_class __refdata event_class_syscall_enter = {
	.system		= "syscalls",
	.reg		= syscall_enter_register,
	.fields_array	= syscall_enter_fields_array,
	.get_fields	= syscall_get_enter_fields,
	.raw_init	= init_syscall_trace,
};

// 513
struct trace_event_class event_class_syscall_exit = {
	.system		= "syscalls",
	.reg		= syscall_exit_register,
	.fields_array	= (struct trace_event_fields[]){
		SYSCALL_FIELD(int, __syscall_nr),
		SYSCALL_FIELD(long, ret),
		{}
	},
	.fields		= LIST_HEAD_INIT(event_class_syscall_exit.fields),
	.raw_init	= init_syscall_trace,
};

// 497
struct trace_event_functions enter_syscall_print_funcs = {
	.trace		= print_syscall_enter,
};