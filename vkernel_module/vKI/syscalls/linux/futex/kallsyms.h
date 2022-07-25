// SPDX-License-Identifier: GPL-2.0

#ifndef _FUTEX_KALLSYMS_H
#define _FUTEX_KALLSYMS_H

#include <linux/kallsyms.h>
#include <linux/sched/wake_q.h>
#include <linux/rtmutex.h>

/* lib/plist.c */
void plist_del(struct plist_node *node, struct plist_head *head);
void plist_add(struct plist_node *node, struct plist_head *head);

/* kernel/signal.c */
long do_no_restart_syscall(struct restart_block *param);

/* kernel/sched/core.c */
void wake_up_q(struct wake_q_head *head);
void wake_q_add_safe(struct wake_q_head *head, struct task_struct *task);
int wake_up_state(struct task_struct *p, unsigned int state);

/* kernel/locking/rtmutex.c */
void rt_mutex_init_proxy_locked(struct rt_mutex *lock,
								struct task_struct *proxy_owner);
int rt_mutex_start_proxy_lock(struct rt_mutex *lock,
							  struct rt_mutex_waiter *waiter,
							  struct task_struct *task);
void rt_mutex_proxy_unlock(struct rt_mutex *lock,
						   struct task_struct *proxy_owner);
int rt_mutex_futex_trylock(struct rt_mutex *lock);
void rt_mutex_init_waiter(struct rt_mutex_waiter *waiter);
int __rt_mutex_start_proxy_lock(struct rt_mutex *lock,
								struct rt_mutex_waiter *waiter,
								struct task_struct *task);
int rt_mutex_wait_proxy_lock(struct rt_mutex *lock,
							 struct hrtimer_sleeper *to,
							 struct rt_mutex_waiter *waiter);
bool rt_mutex_cleanup_proxy_lock(struct rt_mutex *lock,
								 struct rt_mutex_waiter *waiter);
int __rt_mutex_futex_trylock(struct rt_mutex *lock);
void rt_mutex_futex_unlock(struct rt_mutex *lock);
struct task_struct *rt_mutex_next_owner(struct rt_mutex *lock);
bool __rt_mutex_futex_unlock(struct rt_mutex *lock,
							 struct wake_q_head *wake_q);
void rt_mutex_postunlock(struct wake_q_head *wake_q);

/* kernel/pid.c */
struct task_struct *find_get_task_by_vpid(pid_t nr);

/* mm/hugetlb.c */
pgoff_t __basepage_index(struct page *page);

/* mm/memblock.h */
void *memblock_alloc_try_nid(
			phys_addr_t size, phys_addr_t align,
			phys_addr_t min_addr, phys_addr_t max_addr,
			int nid);
void *memblock_alloc_try_nid_raw(
			phys_addr_t size, phys_addr_t align,
			phys_addr_t min_addr, phys_addr_t max_addr,
			int nid);

#endif /* _FUTEX_KALLSYMS_H */