// SPDX-License-Identifier: GPL-2.0

#ifndef _TRACE_KALLSYMS_H
#define _TRACE_KALLSYMS_H

#include <linux/trace_events.h>

#include <linux/kallsyms.h>

/* kernel/trace/trace_syscalls.c */
int set_syscall_print_fmt(struct trace_event_call *call);
void free_syscall_print_fmt(struct trace_event_call *call);
int syscall_exit_register(struct trace_event_call *event,
				 enum trace_reg type, void *data);
enum print_line_t print_syscall_enter(struct trace_iterator *iter, int flags,
		    	 struct trace_event *event);
int syscall_enter_register(struct trace_event_call *event,
				 enum trace_reg type, void *data);
int syscall_enter_define_fields(struct trace_event_call *);
struct list_head *syscall_get_enter_fields(struct trace_event_call *call);
enum print_line_t print_syscall_exit(struct trace_iterator *iter, int flags,
		   		 struct trace_event *event);

#endif /* _TRACE_KALLSYMS_H */