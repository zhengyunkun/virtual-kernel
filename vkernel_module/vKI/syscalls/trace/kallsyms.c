// SPDX-License-Identifier: GPL-2.0

#include "kallsyms.h"


/* kernel/trace/trace_syscalls.c */
int (*_vkn_set_syscall_print_fmt)(struct trace_event_call *) = NULL;
void (*_vkn_free_syscall_print_fmt)(struct trace_event_call *) = NULL;
int (*_vkn_syscall_exit_register)(struct trace_event_call *, enum trace_reg, void *) = NULL;
enum print_line_t (*_vkn_print_syscall_enter)(struct trace_iterator *, int,
				struct trace_event *) = NULL;
int (*_vkn_syscall_enter_register)(struct trace_event_call *, enum trace_reg, void *) = NULL;
int (*_vkn_syscall_enter_define_fields)(struct trace_event_call *) = NULL;
struct list_head *(*_vkn_syscall_get_enter_fields)(struct trace_event_call *) = NULL;
enum print_line_t (*_vkn_print_syscall_exit)(struct trace_iterator *, int, struct trace_event *) = NULL;

/* kernel/trace/trace_syscalls.c */
int set_syscall_print_fmt(struct trace_event_call *call)
{
	if (!_vkn_set_syscall_print_fmt)
		_vkn_set_syscall_print_fmt = (void *)kallsyms_lookup_name("set_syscall_print_fmt");
	return _vkn_set_syscall_print_fmt(call);
}

void free_syscall_print_fmt(struct trace_event_call *call)
{
	if (!_vkn_free_syscall_print_fmt)
		_vkn_free_syscall_print_fmt = (void *)kallsyms_lookup_name("free_syscall_print_fmt");
	_vkn_free_syscall_print_fmt(call);
}

int syscall_exit_register(struct trace_event_call *event,
				 enum trace_reg type, void *data)
{
	if (!_vkn_syscall_exit_register)
		_vkn_syscall_exit_register = (void *)kallsyms_lookup_name("syscall_exit_register");
	return _vkn_syscall_exit_register(event, type, data);
}

enum print_line_t print_syscall_enter(struct trace_iterator *iter, int flags,
					struct trace_event *event)
{
	if (!_vkn_print_syscall_enter)
		_vkn_print_syscall_enter = (void *)kallsyms_lookup_name("print_syscall_enter");
	return _vkn_print_syscall_enter(iter, flags, event);
}

int syscall_enter_register(struct trace_event_call *event,
				 enum trace_reg type, void *data)
{
	if (!_vkn_syscall_enter_register)
		_vkn_syscall_enter_register = (void *)kallsyms_lookup_name("syscall_enter_register");
	return _vkn_syscall_enter_register(event, type, data);
}

int syscall_enter_define_fields(struct trace_event_call *call)
{
	if (!_vkn_syscall_enter_define_fields)
		_vkn_syscall_enter_define_fields = (void *)kallsyms_lookup_name("syscall_enter_define_fields");
	return _vkn_syscall_enter_define_fields(call);
}

struct list_head *syscall_get_enter_fields(struct trace_event_call *call)
{
	if (!_vkn_syscall_get_enter_fields)
		_vkn_syscall_get_enter_fields = (void *)kallsyms_lookup_name("syscall_get_enter_fields");
	return _vkn_syscall_get_enter_fields(call);
}

enum print_line_t print_syscall_exit(struct trace_iterator *iter, int flags, struct trace_event *event)
{
	if (!_vkn_print_syscall_exit)
		_vkn_print_syscall_exit = (void *)kallsyms_lookup_name("print_syscall_exit");
	return _vkn_print_syscall_exit(iter, flags, event);
}