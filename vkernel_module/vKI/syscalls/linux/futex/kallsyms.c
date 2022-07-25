// SPDX-License-Identifier: GPL-2.0

#include "kallsyms.h"

/* lib/plist.c */
void (*_vkn_plist_del)(struct plist_node *, struct plist_head *) = NULL;
void (*_vkn_plist_add)(struct plist_node *, struct plist_head *) = NULL;

/* kernel/signal.c */
long (*_vkn_do_no_restart_syscall)(struct restart_block *) = NULL;

/* kernel/sched/core.c */
void (*_vkn_wake_up_q)(struct wake_q_head *) = NULL;
void (*_vkn_wake_q_add_safe)(struct wake_q_head *, struct task_struct *) = NULL;
int (*_vkn_wake_up_state)(struct task_struct *, unsigned int) = NULL;

/* kernel/locking/rtmutex.c */
void (*_vkn_rt_mutex_init_proxy_locked)(struct rt_mutex *, struct task_struct *) = NULL;
int (*_vkn_rt_mutex_start_proxy_lock)(struct rt_mutex *, struct rt_mutex_waiter *,
									  struct task_struct *) = NULL;
void (*_vkn_rt_mutex_proxy_unlock)(struct rt_mutex *, struct task_struct *) = NULL;
int (*_vkn_rt_mutex_futex_trylock)(struct rt_mutex *) = NULL;
void (*_vkn_rt_mutex_init_waiter)(struct rt_mutex_waiter *) = NULL;
int (*_vkn___rt_mutex_start_proxy_lock)(struct rt_mutex *, struct rt_mutex_waiter *,
										struct task_struct *) = NULL;
int (*_vkn_rt_mutex_wait_proxy_lock)(struct rt_mutex *, struct hrtimer_sleeper *,
									 struct rt_mutex_waiter *) = NULL;
bool (*_vkn_rt_mutex_cleanup_proxy_lock)(struct rt_mutex *,
										 struct rt_mutex_waiter *) = NULL;
int (*_vkn___rt_mutex_futex_trylock)(struct rt_mutex *) = NULL;
void (*_vkn_rt_mutex_futex_unlock)(struct rt_mutex *) = NULL;
struct task_struct *(*_vkn_rt_mutex_next_owner)(struct rt_mutex *) = NULL;
bool (*_vkn___rt_mutex_futex_unlock)(struct rt_mutex *, struct wake_q_head *) = NULL;
void (*_vkn_rt_mutex_postunlock)(struct wake_q_head *) = NULL;

/* kernel/pid.c */
struct task_struct *(*_vkn_find_get_task_by_vpid)(pid_t) = NULL;

/* mm/hugetlb.c */
pgoff_t (*_vkn___basepage_index)(struct page *) = NULL;

/* mm/memblock.h */
void *(*_vkn_memblock_alloc_try_nid)(phys_addr_t, phys_addr_t,
			phys_addr_t, phys_addr_t, int) = NULL;
void *(*_vkn_memblock_alloc_try_nid_raw)(
			phys_addr_t, phys_addr_t, phys_addr_t, phys_addr_t, int) = NULL;

/* lib/plist.c */
void plist_del(struct plist_node *node, struct plist_head *head)
{
	if (!_vkn_plist_del)
		_vkn_plist_del = (void *)kallsyms_lookup_name("plist_del");
	_vkn_plist_del(node, head);
}
void plist_add(struct plist_node *node, struct plist_head *head)
{
	if (!_vkn_plist_add)
		_vkn_plist_add = (void *)kallsyms_lookup_name("plist_add");
	_vkn_plist_add(node, head);
}

/* kernel/signal.c */
long do_no_restart_syscall(struct restart_block *param)
{
	if (!_vkn_do_no_restart_syscall)
		_vkn_do_no_restart_syscall = (void *)kallsyms_lookup_name("do_no_restart_syscall");
	return _vkn_do_no_restart_syscall(param);
}

/* kernel/sched/core.c */
void wake_up_q(struct wake_q_head *head)
{
	if (!_vkn_wake_up_q)
		_vkn_wake_up_q = (void *)kallsyms_lookup_name("wake_up_q");
	_vkn_wake_up_q(head);
}
void wake_q_add_safe(struct wake_q_head *head, struct task_struct *task)
{
	if (!_vkn_wake_q_add_safe)
		_vkn_wake_q_add_safe = (void *)kallsyms_lookup_name("wake_q_add_safe");
	_vkn_wake_q_add_safe(head, task);
}
int wake_up_state(struct task_struct *p, unsigned int state)
{
	if (!_vkn_wake_up_state)
		_vkn_wake_up_state = (void *)kallsyms_lookup_name("wake_up_state");
	return _vkn_wake_up_state(p, state);
}

/* kernel/locking/rtmutex.c */
void rt_mutex_init_proxy_locked(struct rt_mutex *lock,
								struct task_struct *proxy_owner)
{
	if (!_vkn_rt_mutex_init_proxy_locked)
		_vkn_rt_mutex_init_proxy_locked = (void *)kallsyms_lookup_name("rt_mutex_init_proxy_locked");
	_vkn_rt_mutex_init_proxy_locked(lock, proxy_owner);
}
int rt_mutex_start_proxy_lock(struct rt_mutex *lock,
							  struct rt_mutex_waiter *waiter,
							  struct task_struct *task)
{
	if (!_vkn_rt_mutex_start_proxy_lock)
		_vkn_rt_mutex_start_proxy_lock = (void *)kallsyms_lookup_name("rt_mutex_start_proxy_lock");
	return _vkn_rt_mutex_start_proxy_lock(lock, waiter, task);
}
void rt_mutex_proxy_unlock(struct rt_mutex *lock,
						   struct task_struct *proxy_owner)
{
	if (!_vkn_rt_mutex_proxy_unlock)
		_vkn_rt_mutex_proxy_unlock = (void *)kallsyms_lookup_name("rt_mutex_proxy_unlock");
	_vkn_rt_mutex_proxy_unlock(lock, proxy_owner);
}
int rt_mutex_futex_trylock(struct rt_mutex *lock)
{
	if (!_vkn_rt_mutex_futex_trylock)
		_vkn_rt_mutex_futex_trylock = (void *)kallsyms_lookup_name("rt_mutex_futex_trylock");
	return _vkn_rt_mutex_futex_trylock(lock);
}
void rt_mutex_init_waiter(struct rt_mutex_waiter *waiter)
{
	if (!_vkn_rt_mutex_init_waiter)
		_vkn_rt_mutex_init_waiter = (void *)kallsyms_lookup_name("rt_mutex_init_waiter");
	_vkn_rt_mutex_init_waiter(waiter);
}
int __rt_mutex_start_proxy_lock(struct rt_mutex *lock,
								struct rt_mutex_waiter *waiter,
								struct task_struct *task)
{
	if (!_vkn___rt_mutex_start_proxy_lock)
		_vkn___rt_mutex_start_proxy_lock = (void *)kallsyms_lookup_name("__rt_mutex_start_proxy_lock");
	return _vkn___rt_mutex_start_proxy_lock(lock, waiter, task);
}
int rt_mutex_wait_proxy_lock(struct rt_mutex *lock,
							 struct hrtimer_sleeper *to,
							 struct rt_mutex_waiter *waiter)
{
	if (!_vkn_rt_mutex_wait_proxy_lock)
		_vkn_rt_mutex_wait_proxy_lock = (void *)kallsyms_lookup_name("rt_mutex_wait_proxy_lock");
	return _vkn_rt_mutex_wait_proxy_lock(lock, to, waiter);
}
bool rt_mutex_cleanup_proxy_lock(struct rt_mutex *lock,
								 struct rt_mutex_waiter *waiter)
{
	if (!_vkn_rt_mutex_cleanup_proxy_lock)
		_vkn_rt_mutex_cleanup_proxy_lock = (void *)kallsyms_lookup_name("rt_mutex_cleanup_proxy_lock");
	return _vkn_rt_mutex_cleanup_proxy_lock(lock, waiter);
}
int __rt_mutex_futex_trylock(struct rt_mutex *lock)
{
	if (!_vkn___rt_mutex_futex_trylock)
		_vkn___rt_mutex_futex_trylock = (void *)kallsyms_lookup_name("__rt_mutex_futex_trylock");
	return _vkn___rt_mutex_futex_trylock(lock);
}
void rt_mutex_futex_unlock(struct rt_mutex *lock)
{
	if (!_vkn_rt_mutex_futex_unlock)
		_vkn_rt_mutex_futex_unlock = (void *)kallsyms_lookup_name("rt_mutex_futex_unlock");
	_vkn_rt_mutex_futex_unlock(lock);
}
struct task_struct *rt_mutex_next_owner(struct rt_mutex *lock)
{
	if (!_vkn_rt_mutex_next_owner)
		_vkn_rt_mutex_next_owner = (void *)kallsyms_lookup_name("rt_mutex_next_owner");
	return _vkn_rt_mutex_next_owner(lock);
}
bool __rt_mutex_futex_unlock(struct rt_mutex *lock,
							 struct wake_q_head *wake_q)
{
	if (!_vkn___rt_mutex_futex_unlock)
		_vkn___rt_mutex_futex_unlock = (void *)kallsyms_lookup_name("__rt_mutex_futex_unlock");
	return _vkn___rt_mutex_futex_unlock(lock, wake_q);
}
void rt_mutex_postunlock(struct wake_q_head *wake_q)
{
	if (!_vkn_rt_mutex_postunlock)
		_vkn_rt_mutex_postunlock = (void *)kallsyms_lookup_name("rt_mutex_postunlock");
	_vkn_rt_mutex_postunlock(wake_q);
}

/* kernel/pid.c */
struct task_struct *find_get_task_by_vpid(pid_t nr)
{
	if (!_vkn_find_get_task_by_vpid)
		_vkn_find_get_task_by_vpid = (void *)kallsyms_lookup_name("find_get_task_by_vpid");
	return _vkn_find_get_task_by_vpid(nr);
}

/* mm/hugetlb.c */
pgoff_t __basepage_index(struct page *page)
{
	if (!_vkn___basepage_index)
		_vkn___basepage_index = (void *)kallsyms_lookup_name("__basepage_index");
	return _vkn___basepage_index(page);
}

/* mm/memblock.h */
void *memblock_alloc_try_nid(
			phys_addr_t size, phys_addr_t align,
			phys_addr_t min_addr, phys_addr_t max_addr,
			int nid)
{
	if (!_vkn_memblock_alloc_try_nid)
		_vkn_memblock_alloc_try_nid = (void *)kallsyms_lookup_name("memblock_alloc_try_nid");
	return _vkn_memblock_alloc_try_nid(size, align, min_addr, max_addr, nid);
}

void *memblock_alloc_try_nid_raw(
			phys_addr_t size, phys_addr_t align,
			phys_addr_t min_addr, phys_addr_t max_addr,
			int nid)
{
	if (!_vkn_memblock_alloc_try_nid_raw)
		_vkn_memblock_alloc_try_nid_raw = (void *)kallsyms_lookup_name("memblock_alloc_try_nid_raw");
	return _vkn_memblock_alloc_try_nid_raw(size, align, min_addr, max_addr, nid);
}