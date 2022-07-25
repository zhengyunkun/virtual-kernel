// SPDX-License-Identifier: GPL-2.0

#ifndef _LOCKING_RTMUTEX_H
#define _LOCKING_RTMUTEX_H

/* kernel/locking/rtmutex_common.h */
/*
 * This is the control structure for tasks blocked on a rt_mutex,
 * which is allocated on the kernel stack on of the blocked task.
 *
 * @tree_entry:		pi node to enqueue into the mutex waiters tree
 * @pi_tree_entry:	pi node to enqueue into the mutex owner waiters tree
 * @task:		task reference to the blocked task
 */
struct rt_mutex_waiter
{
	struct rb_node tree_entry;
	struct rb_node pi_tree_entry;
	struct task_struct *task;
	struct rt_mutex *lock;
#ifdef CONFIG_DEBUG_RT_MUTEXES
	unsigned long ip;
	struct pid *deadlock_task_pid;
	struct rt_mutex *deadlock_lock;
#endif
	int prio;
	u64 deadline;
};

/* kernel/locking/rtmutex_common.h */
#ifdef CONFIG_DEBUG_RT_MUTEXES
/* kernel/locking/rtmutex-debug.c */
void debug_rt_mutex_free_waiter(struct rt_mutex_waiter *waiter)
{
	put_pid(waiter->deadlock_task_pid);
	memset(waiter, 0x22, sizeof(*waiter));
}
#else
/* kernel/locking/rtmutex.h */
#define debug_rt_mutex_free_waiter(w) \
	do                                \
	{                                 \
	} while (0)
#endif

/*
 * lock->owner state tracking:
 */
#define RT_MUTEX_HAS_WAITERS 1UL

static inline struct task_struct *rt_mutex_owner(struct rt_mutex *lock)
{
	unsigned long owner = (unsigned long)READ_ONCE(lock->owner);

	return (struct task_struct *)(owner & ~RT_MUTEX_HAS_WAITERS);
}

#endif /* _LOCKING_RTMUTEX_H */