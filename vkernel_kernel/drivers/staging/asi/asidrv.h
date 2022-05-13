/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */

#ifndef __ASIDRV_H__
#define __ASIDRV_H__

#include <linux/types.h>

enum asidrv_seqnum {
	ASIDRV_SEQ_NOP,		/* empty sequence */
	ASIDRV_SEQ_PRINTK,	/* printk sequence */
	ASIDRV_SEQ_MEM,		/* access unmapped memory */
	ASIDRV_SEQ_MEMMAP,	/* access mapped memory */
	ASIDRV_SEQ_INTERRUPT,	/* interrupt sequence */
	ASIDRV_SEQ_NMI,		/* NMI interrupt sequence */
	ASIDRV_SEQ_INTRNMI,	/* NMI in interrupt sequence */
	ASIDRV_SEQ_SCHED,	/* schedule() sequence */
};

enum asidrv_run_error {
	ASIDRV_RUN_ERR_NONE,	/* no error */
	ASIDRV_RUN_ERR_SEQUENCE, /* unknown sequence */
	ASIDRV_RUN_ERR_MAP_STACK, /* failed to map current stack */
	ASIDRV_RUN_ERR_MAP_TASK, /* failed to map current task */
	ASIDRV_RUN_ERR_ENTER,	/* failed to enter ASI */
	ASIDRV_RUN_ERR_ACTIVE,	/* ASI is not active after entering ASI */
	ASIDRV_RUN_ERR_MAP_BUFFER, /* failed to map buffer */
	ASIDRV_RUN_ERR_NCPUS,	/* not enough active cpus */
	ASIDRV_RUN_ERR_INTR,	/* no interrupt received */
	ASIDRV_RUN_ERR_INTR_ASI_ACTIVE, /* ASI active in interrupt handler */
	ASIDRV_RUN_ERR_TIMEOUT,
	ASIDRV_RUN_ERR_NMI,	/* no NMI received */
	ASIDRV_RUN_ERR_NMI_REG,	/* failed to register NMI handler */
	ASIDRV_RUN_ERR_NMI_ASI_ACTIVE, /* ASI active in NMI handler */
	ASIDRV_RUN_ERR_KTHREAD,	/* failed to create kernel thread */
};

#define ASIDRV_IOCTL_RUN_SEQUENCE	_IOWR('a', 1, struct asidrv_run_param)

/*
 * ASIDRV_IOCTL_LIST_FAULT: return the list of ASI faults.
 *
 * User should set 'length' with the number of entries available in the
 * 'fault' array. On return, 'length' is set to the number of ASI faults
 * (which can be larger than the original 'length' value), and the 'fault'
 * array is filled with the ASI faults.
 */
#define ASIDRV_IOCTL_LIST_FAULT		_IOWR('a', 2, struct asidrv_fault_list)
#define ASIDRV_IOCTL_CLEAR_FAULT	_IO('a', 3)
#define ASIDRV_IOCTL_LOG_FAULT_STACK	_IO('a', 4)

/*
 * ASIDRV_IOCTL_ADD_MAPPING: add mapping to the ASI.
 *
 * User should set 'length' with the number of mapping described in the
 * 'mapping' array.
 * Return value:
 *   -1   - error no mapping was added
 *    0   - no error, all mappings were added
 *   N>0  - error but the first N mappings were added
 */
#define ASIDRV_IOCTL_ADD_MAPPING	_IOWR('a', 5, struct asidrv_mapping_list)
#define ASIDRV_IOCTL_CLEAR_MAPPING	_IOW('a', 6, struct asidrv_mapping_list)
#define ASIDRV_IOCTL_LIST_MAPPING	_IOWR('a', 7, struct asidrv_mapping_list)

#define ASIDRV_KSYM_NAME_LEN	128
/*
 * We need KSYM_SYMBOL_LEN to lookup symbol. However it's not part of
 * userland include. So we use a reasonably large value (KSYM_SYMBOL_LEN
 * is around 310).
 */
#define ASIDRV_KSYM_SYMBOL_LEN	512

struct asidrv_run_param {
	__u32 sequence;		/* sequence to run */
	__u32 run_error;	/* result error after run */
	__u32 asi_active;	/* ASI is active after run? */
};

struct asidrv_fault {
	__u64 addr;
	char  symbol[ASIDRV_KSYM_SYMBOL_LEN];
	__u32 count;
};

struct asidrv_fault_list {
	__u32 length;
	struct asidrv_fault fault[0];
};

struct asidrv_mapping {
	__u64 addr;
	__u64 size;
	__u32 level;
	__u32 percpu;
};

struct asidrv_mapping_list {
	__u32 length;
	struct asidrv_mapping mapping[0];
};

#endif
