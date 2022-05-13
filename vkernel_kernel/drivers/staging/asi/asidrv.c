// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, Oracle and/or its affiliates.
 */

#include <linux/fs.h>
#include <linux/kallsyms.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/nmi.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <asm/asi.h>
#include <asm/dpt.h>
#include <asm/tlbflush.h>

#include "asidrv.h"

#define ASIDRV_TEST_BUFFER_SIZE	PAGE_SIZE

/* Number of read for mem/memmap test sequence */
#define ASIDRV_MEM_READ_COUNT		1000

/* Number of loop for the sched test sequence */
#define ASIDRV_SCHED_LOOP_COUNT		20

/* Timeout for target to be ready to receive an interrupt or NMI */
#define ASIDRV_TIMEOUT_TARGET_READY	1

/* Timeout for receiving an interrupt or NMI */
#define ASIDRV_TIMEOUT_INTERRUPT	5

/* Timeout before thread can start its job */
#define ASIDRV_TIMEOUT_THREAD_START	5

/* NMI handler name used for testing */
#define ASIDRV_NMI_HANDLER_NAME		"ASI Test"

enum asidrv_state {
	ASIDRV_STATE_NONE,
	ASIDRV_STATE_START,
	ASIDRV_STATE_INTR_WAITING,
	ASIDRV_STATE_INTR_RECEIVED,
	ASIDRV_STATE_NMI_WAITING,
	ASIDRV_STATE_NMI_RECEIVED,
};

struct asidrv_test {
	struct asi		*asi;	/* ASI for testing */
	struct dpt		*dpt;	/* ASI decorated page-table */
	char			*buffer; /* buffer for testing */

	/* runtime */
	atomic_t		state;	/* runtime state */
	int			cpu;	/* cpu the test is running on */
	struct work_struct	work;	/* work for other cpu */
	bool			work_set;
	enum asidrv_run_error	run_error;
	bool			intrnmi;
	struct task_struct	*kthread; /* work on same cpu */
	int			count;
};

struct asidrv_sequence {
	const char *name;
	enum asidrv_run_error (*setup)(struct asidrv_test *t);
	enum asidrv_run_error (*run)(struct asidrv_test *t);
	void (*cleanup)(struct asidrv_test *t);
};

static struct asidrv_test *asidrv_test;
static struct asidrv_test *asidrv_nmi_target;

static void asidrv_test_destroy(struct asidrv_test *test);
static void asidrv_run_fini(struct asidrv_test *test);
static void asidrv_run_cleanup(struct asidrv_test *test,
			       struct asidrv_sequence *sequence);
static void asidrv_intrnmi_send(struct work_struct *work);

static struct asidrv_test *asidrv_test_create(void)
{
	struct asidrv_test *test;
	int err;

	test = kzalloc(sizeof(*test), GFP_KERNEL);
	if (!test)
		return NULL;

	test->buffer = kzalloc(ASIDRV_TEST_BUFFER_SIZE, GFP_KERNEL);
	if (!test->buffer)
		goto error;

	/*
	 * Create and fill a decorator page-table to be used with the ASI.
	 */
	test->dpt = dpt_create(ASI_PGTABLE_MASK);
	if (!test->dpt)
		goto error;

	err = asi_init_dpt(test->dpt);
	if (err)
		goto error;

	err = DPT_MAP_THIS_MODULE(test->dpt);
	if (err)
		goto error;

	/* map the asidrv_test as we will access it during the test */
	err = dpt_map(test->dpt, test, sizeof(*test));
	if (err)
		goto error;

	test->asi = asi_create_test();
	if (!test->asi)
		goto error;

	/*
	 * By default, the ASI structure is not mapped into the ASI. We
	 * map it so that we can access it and verify the consistency
	 * of some values (for example the CR3 value).
	 */
	err = dpt_map(test->dpt, test->asi, sizeof(*test->asi));
	if (err)
		goto error;

	asi_set_pagetable(test->asi, test->dpt->pagetable);

	return test;

error:
	pr_debug("Failed to create ASI Test\n");
	asidrv_test_destroy(test);
	return NULL;
}

static void asidrv_test_destroy(struct asidrv_test *test)
{
	if (!test)
		return;

	if (test->dpt)
		dpt_destroy(test->dpt);

	if (test->asi)
		asi_destroy(test->asi);

	kfree(test->buffer);
	kfree(test);
}

static int asidrv_asi_is_active(struct asi *asi)
{
	struct asi *current_asi;
	unsigned long cr3;
	bool is_active;
	int idepth;

	if (!asi)
		return false;

	current_asi = this_cpu_read(cpu_asi_session.asi);
	if (current_asi == asi) {
		idepth = this_cpu_read(cpu_asi_session.idepth);
		is_active = (idepth == 0);
	} else {
		is_active = false;
		if (current_asi) {
			/* weird... another ASI is active! */
			pr_debug("ASI %px is active (testing ASI = %px)\n",
				 current_asi, asi);
		}
	}

	/*
	 * If the ASI is active check that the CR3 value is consistent with
	 * this ASI being active. Otherwise, check that CR3 value doesn't
	 * reference an ASI.
	 */
	cr3 = __native_read_cr3();
	if (is_active) {
		if ((cr3 ^ asi->base_cr3) >> ASI_PCID_PREFIX_SHIFT == 0)
			return true;

		pr_warn("ASI %px: active ASI has inconsistent CR3 value (cr3=%lx, ASI base=%lx)\n",
			asi, cr3, asi->base_cr3);

	} else if (cr3 & ASI_PCID_PREFIX_MASK) {
		pr_warn("ASI %px: inactive ASI has inconsistent CR3 value (cr3=%lx, ASI base=%lx)\n",
			asi, cr3, asi->base_cr3);
	}

	return false;
}

/*
 * Wait for an atomic value to be set or the timeout to expire.
 * Return 0 if the value is set, or -1 if the timeout expires.
 */
static enum asidrv_run_error asidrv_wait(struct asidrv_test *test,
					 int value, unsigned int timeout)
{
	cycles_t start = get_cycles();
	cycles_t stop = start + timeout * tsc_khz * 1000;

	while (get_cycles() < stop) {
		if (atomic_read(&test->state) == value ||
		    test->run_error != ASIDRV_RUN_ERR_NONE)
			return test->run_error;
		cpu_relax();
	}

	/* timeout reached */
	return ASIDRV_RUN_ERR_TIMEOUT;
}

/*
 * Wait for an atomic value to transition from the initial value (set
 * on entry) to the final value, or to timeout. Return 0 if the transition
 * was done, or -1 if the timeout expires.
 */
static enum asidrv_run_error asidrv_wait_transition(struct asidrv_test *test,
						    int initial, int final,
						    unsigned int timeout)
{
	/* set the initial state value */
	atomic_set(&test->state, initial);

	/* do an active wait for the state changes */
	return asidrv_wait(test, final, timeout);
}

/*
 * NMI Test Sequence
 */

static int asidrv_nmi_handler(unsigned int val, struct pt_regs *regs)
{
	struct asidrv_test *test = asidrv_nmi_target;

	if (!test)
		return NMI_DONE;

	/* ASI should be interrupted by the NMI */
	if (asidrv_asi_is_active(test->asi)) {
		test->run_error = ASIDRV_RUN_ERR_NMI_ASI_ACTIVE;
		atomic_set(&test->state, ASIDRV_STATE_NMI_RECEIVED);
		return NMI_HANDLED;
	}

	pr_debug("Received NMI\n");
	atomic_set(&test->state, ASIDRV_STATE_NMI_RECEIVED);
	asidrv_nmi_target = NULL;

	return NMI_HANDLED;
}

static void asidrv_nmi_send(struct work_struct *work)
{
	struct asidrv_test *test = container_of(work, struct asidrv_test, work);
	cpumask_t mask = CPU_MASK_NONE;
	enum asidrv_run_error err;

	cpumask_set_cpu(test->cpu, &mask);

	/* wait for cpu target to be ready, then send an NMI */
	err = asidrv_wait(test,
			  ASIDRV_STATE_NMI_WAITING,
			  ASIDRV_TIMEOUT_TARGET_READY);
	if (err) {
		pr_debug("Target cpu %d not ready, NMI not sent: error %d\n",
			 test->cpu, err);
		return;
	}

	pr_debug("Sending NMI to cpu %d\n", test->cpu);
	asidrv_nmi_target = test;
	/*
	 * The value of asidrv_nmi_target should be set and propagated
	 * before sending the IPI.
	 */
	wmb();
	apic->send_IPI_mask(&mask, NMI_VECTOR);
}

static enum asidrv_run_error asidrv_nmi_setup(struct asidrv_test *test)
{
	int err;

	err = register_nmi_handler(NMI_LOCAL, asidrv_nmi_handler,
				   NMI_FLAG_FIRST,
				   ASIDRV_NMI_HANDLER_NAME);
	if (err) {
		pr_debug("Failed to register NMI handler\n");
		return ASIDRV_RUN_ERR_NMI_REG;
	}

	/* set work to have another cpu to send us an NMI */
	if (test->intrnmi)
		INIT_WORK(&test->work, asidrv_intrnmi_send);
	else
		INIT_WORK(&test->work, asidrv_nmi_send);

	test->work_set = true;

	return ASIDRV_RUN_ERR_NONE;
}

static void asidrv_nmi_cleanup(struct asidrv_test *test)
{
	unregister_nmi_handler(NMI_LOCAL, ASIDRV_NMI_HANDLER_NAME);
}

static enum asidrv_run_error asidrv_nmi_run(struct asidrv_test *test)
{
	enum asidrv_run_error err;
	unsigned long flags;

	/* disable interrupts as we want to be interrupted by an NMI */
	local_irq_save(flags);

	/* wait for state changes indicating that an NMI was received */
	err = asidrv_wait_transition(test,
				     ASIDRV_STATE_NMI_WAITING,
				     ASIDRV_STATE_NMI_RECEIVED,
				     ASIDRV_TIMEOUT_INTERRUPT);
	if (err == ASIDRV_RUN_ERR_TIMEOUT) {
		pr_debug("NMI wait timeout\n");
		err = ASIDRV_RUN_ERR_NMI;
	}

	local_irq_restore(flags);

	return err;
}

/*
 * Interrupt Test Sequence
 */

static void asidrv_intr_handler(void *info)
{
	struct asidrv_test *test = info;

	/* ASI should be interrupted by the interrupt */
	if (asidrv_asi_is_active(test->asi)) {
		test->run_error = ASIDRV_RUN_ERR_INTR_ASI_ACTIVE;
		atomic_set(&test->state, ASIDRV_STATE_INTR_RECEIVED);
		return;
	}

	pr_debug("Received interrupt\n");
	atomic_set(&test->state, ASIDRV_STATE_INTR_RECEIVED);

	if (!test->intrnmi)
		return;

	pr_debug("Waiting for NMI in interrupt\n");
	asidrv_nmi_run(test);
}

static void asidrv_intr_send(struct work_struct *work)
{
	struct asidrv_test *test = container_of(work, struct asidrv_test, work);
	enum asidrv_run_error err;

	/* wait for cpu target to be ready, then send an interrupt */
	err = asidrv_wait(test,
			  ASIDRV_STATE_INTR_WAITING,
			  ASIDRV_TIMEOUT_TARGET_READY);
	if (err) {
		pr_debug("Target cpu %d not ready, interrupt not sent: error %d\n",
			 test->cpu, err);
		return;
	}

	pr_debug("Sending interrupt to cpu %d\n", test->cpu);
	smp_call_function_single(test->cpu, asidrv_intr_handler,
				 test, false);
}

static enum asidrv_run_error asidrv_intr_setup(struct asidrv_test *test)
{
	/* set work to have another cpu to send us an interrupt */
	INIT_WORK(&test->work, asidrv_intr_send);
	test->work_set = true;
	return ASIDRV_RUN_ERR_NONE;
}

static enum asidrv_run_error asidrv_intr_run(struct asidrv_test *test)
{
	enum asidrv_run_error err;

	/* wait for state changes indicating that an interrupt was received */
	err = asidrv_wait_transition(test,
				     ASIDRV_STATE_INTR_WAITING,
				     ASIDRV_STATE_INTR_RECEIVED,
				     ASIDRV_TIMEOUT_INTERRUPT);
	if (err == ASIDRV_RUN_ERR_TIMEOUT) {
		pr_debug("Interrupt wait timeout\n");
		err = ASIDRV_RUN_ERR_INTR;
	}

	return err;
}

/*
 * Interrupt+NMI Test Sequence
 */

static void asidrv_intrnmi_send(struct work_struct *work)
{
	/* send and interrupt and then send an NMI */
	asidrv_intr_send(work);
	asidrv_nmi_send(work);
}

static enum asidrv_run_error asidrv_intrnmi_setup(struct asidrv_test *test)
{
	test->intrnmi = true;
	return asidrv_nmi_setup(test);
}

static enum asidrv_run_error asidrv_intrnmi_run(struct asidrv_test *test)
{
	enum asidrv_run_error err;
	enum asidrv_state state;

	/*
	 * Wait for state changes indicating that an interrupt and
	 * then an NMI were received.
	 */
	err = asidrv_wait_transition(test,
				     ASIDRV_STATE_INTR_WAITING,
				     ASIDRV_STATE_NMI_RECEIVED,
				     ASIDRV_TIMEOUT_INTERRUPT);
	if (err == ASIDRV_RUN_ERR_TIMEOUT) {
		state = atomic_read(&test->state);
		if (state == ASIDRV_STATE_INTR_WAITING) {
			pr_debug("Interrupt wait timeout\n");
			err = ASIDRV_RUN_ERR_INTR;
		} else {
			pr_debug("NMI wait timeout\n");
			err = ASIDRV_RUN_ERR_NMI;
		}
	}

	return err;
}

/*
 * Sched Test Sequence
 */

static void asidrv_sched_loop(struct asidrv_test *test)
{
	int last_count = 0;

	while (test->count < ASIDRV_SCHED_LOOP_COUNT) {
		test->count++;
		last_count = test->count;
		/*
		 * Call into the scheduler, until it runs the other
		 * asidrv_sched_loop() task. We know it has run when
		 * test->count changes.
		 */
		while (last_count == test->count)
			schedule();
	}
	test->count++;
}

static int asidrv_sched_kthread(void *data)
{
	struct asidrv_test *test = data;
	enum asidrv_run_error err;

	err = asidrv_wait(test, ASIDRV_STATE_START,
			  ASIDRV_TIMEOUT_THREAD_START);
	if (err) {
		pr_debug("Error waiting for start state: error %d\n", err);
		return err;
	}
	asidrv_sched_loop(test);

	return 0;
}

static enum asidrv_run_error asidrv_sched_init(struct asidrv_test *test)
{
	test->kthread = kthread_create_on_node(asidrv_sched_kthread, test,
					       cpu_to_node(test->cpu),
					       "sched test");
	if (!test->kthread)
		return ASIDRV_RUN_ERR_KTHREAD;

	kthread_bind(test->kthread, test->cpu);
	test->count = 0;

	return ASIDRV_RUN_ERR_NONE;
}

static enum asidrv_run_error asidrv_sched_run(struct asidrv_test *test)
{
	atomic_set(&test->state, ASIDRV_STATE_START);
	asidrv_sched_loop(test);

	return ASIDRV_RUN_ERR_NONE;
}

/*
 * Memory Buffer Access Test Sequences
 */

#define OPTNONE __attribute__((optimize(0)))

static enum asidrv_run_error OPTNONE asidrv_mem_run(struct asidrv_test *test)
{
	char c;
	int i, index;

	/*
	 * Do random reads in the test buffer, and return if the ASI
	 * becomes inactive.
	 */
	for (i = 0; i < ASIDRV_MEM_READ_COUNT; i++) {
		index = get_cycles() % ASIDRV_TEST_BUFFER_SIZE;
		c = test->buffer[index];
		if (!asidrv_asi_is_active(test->asi)) {
			pr_warn("ASI inactive after reading byte %d at %d\n",
				i + 1, index);
			break;
		}
	}

	return ASIDRV_RUN_ERR_NONE;
}

static enum asidrv_run_error asidrv_memmap_setup(struct asidrv_test *test)
{
	int err;

	pr_debug("mapping test buffer %px\n", test->buffer);
	err = dpt_map(test->dpt, test->buffer, ASIDRV_TEST_BUFFER_SIZE);
	if (err)
		return ASIDRV_RUN_ERR_MAP_BUFFER;

	return ASIDRV_RUN_ERR_NONE;
}

static void asidrv_memmap_cleanup(struct asidrv_test *test)
{
	dpt_unmap(test->dpt, test->buffer);
}

/*
 * Printk Test Sequence
 */
static enum asidrv_run_error asidrv_printk_run(struct asidrv_test *test)
{
	pr_notice("asidrv printk test...\n");
	return ASIDRV_RUN_ERR_NONE;
}

struct asidrv_sequence asidrv_sequences[] = {
	[ASIDRV_SEQ_NOP] = {
		"nop",
		NULL, NULL, NULL,
	},
	[ASIDRV_SEQ_PRINTK] = {
		"printk",
		NULL, asidrv_printk_run, NULL,
	},
	[ASIDRV_SEQ_MEM] = {
		"mem",
		NULL, asidrv_mem_run, NULL,
	},
	[ASIDRV_SEQ_MEMMAP] = {
		"memmap",
		asidrv_memmap_setup, asidrv_mem_run, asidrv_memmap_cleanup,
	},
	[ASIDRV_SEQ_INTERRUPT] = {
		"interrupt",
		asidrv_intr_setup, asidrv_intr_run, NULL,
	},
	[ASIDRV_SEQ_NMI] = {
		"nmi",
		asidrv_nmi_setup, asidrv_nmi_run, asidrv_nmi_cleanup,
	},
	[ASIDRV_SEQ_INTRNMI] = {
		"intr+nmi",
		asidrv_intrnmi_setup, asidrv_intrnmi_run, asidrv_nmi_cleanup,
	},
	[ASIDRV_SEQ_SCHED] = {
		"sched",
		asidrv_sched_init, asidrv_sched_run, NULL,
	},
};

static enum asidrv_run_error asidrv_run_init(struct asidrv_test *test)
{
	cpumask_t mask = CPU_MASK_NONE;
	int err;

	test->run_error = ASIDRV_RUN_ERR_NONE;

	/*
	 * Binding ourself to the current cpu but keep preemption
	 * enabled.
	 */
	test->cpu = get_cpu();
	cpumask_set_cpu(test->cpu, &mask);
	set_cpus_allowed_ptr(current, &mask);
	put_cpu();

	/*
	 * Map the current stack, we need it to enter ASI.
	 */
	err = dpt_map(test->dpt, current->stack,
		      PAGE_SIZE << THREAD_SIZE_ORDER);
	if (err) {
		asidrv_run_fini(test);
		return ASIDRV_RUN_ERR_MAP_STACK;
	}

	/*
	 * Map the current task, schedule() needs it.
	 */
	err = dpt_map(test->dpt, current, sizeof(struct task_struct));
	if (err)
		return ASIDRV_RUN_ERR_MAP_TASK;

	/*
	 * The ASI page-table has been updated so bump the generation
	 * number to have the ASI TLB flushed.
	 */
	atomic64_inc(&test->asi->pgtable_gen);

	return ASIDRV_RUN_ERR_NONE;
}

static void asidrv_run_fini(struct asidrv_test *test)
{
	dpt_unmap(test->dpt, current);
	dpt_unmap(test->dpt, current->stack);
}

static enum asidrv_run_error asidrv_run_setup(struct asidrv_test *test,
					      struct asidrv_sequence *sequence)
{
	unsigned int other_cpu;
	int run_err;

	test->work_set = false;
	test->intrnmi = false;

	if (sequence->setup) {
		run_err = sequence->setup(test);
		if (run_err)
			return run_err;
	}

	if (test->work_set) {
		other_cpu = cpumask_any_but(cpu_online_mask, test->cpu);
		if (other_cpu == test->cpu) {
			pr_debug("Sequence %s requires an extra online cpu\n",
				 sequence->name);
			asidrv_run_cleanup(test, sequence);
			return ASIDRV_RUN_ERR_NCPUS;
		}

		atomic_set(&test->state, ASIDRV_STATE_NONE);
		schedule_work_on(other_cpu, &test->work);
	}

	if (test->kthread)
		wake_up_process(test->kthread);

	return ASIDRV_RUN_ERR_NONE;
}

static void asidrv_run_cleanup(struct asidrv_test *test,
			       struct asidrv_sequence *sequence)
{
	if (test->kthread) {
		kthread_stop(test->kthread);
		test->kthread = NULL;
	}

	if (test->work_set) {
		/* ensure work has completed */
		flush_work(&test->work);
		test->work_set = false;
	}

	if (sequence->cleanup)
		sequence->cleanup(test);
}

/*
 * Run the specified sequence with ASI. Report result back.
 */
static enum asidrv_run_error asidrv_run(struct asidrv_test *test,
					enum asidrv_seqnum seqnum,
					bool *asi_active)
{
	struct asidrv_sequence *sequence = &asidrv_sequences[seqnum];
	int run_err = ASIDRV_RUN_ERR_NONE;
	int err = 0;

	if (seqnum >= ARRAY_SIZE(asidrv_sequences)) {
		pr_debug("Undefined sequence %d\n", seqnum);
		return ASIDRV_RUN_ERR_SEQUENCE;
	}

	pr_debug("ASI running sequence %s\n", sequence->name);

	run_err = asidrv_run_setup(test, sequence);
	if (run_err)
		return run_err;

	err = asi_enter(test->asi);
	if (err) {
		run_err = ASIDRV_RUN_ERR_ENTER;
		goto failed_noexit;
	}

	if (!asidrv_asi_is_active(test->asi)) {
		run_err = ASIDRV_RUN_ERR_ACTIVE;
		goto failed;
	}

	if (sequence->run) {
		run_err = sequence->run(test);
		if (run_err != ASIDRV_RUN_ERR_NONE)
			goto failed;
	}

	*asi_active = asidrv_asi_is_active(test->asi);

failed:
	asi_exit(test->asi);

failed_noexit:
	asidrv_run_cleanup(test, sequence);

	return run_err;
}

static int asidrv_ioctl_run_sequence(struct asidrv_test *test,
				     unsigned long arg)
{
	struct asidrv_run_param __user *urparam;
	struct asidrv_run_param rparam;
	enum asidrv_run_error run_err;
	enum asidrv_seqnum seqnum;
	bool asi_active = false;

	urparam = (struct asidrv_run_param *)arg;
	if (copy_from_user(&rparam, urparam, sizeof(rparam)))
		return -EFAULT;

	seqnum = rparam.sequence;

	pr_debug("ASI sequence %d\n", seqnum);

	run_err = asidrv_run_init(test);
	if (run_err) {
		pr_debug("ASI run init error %d\n", run_err);
		goto failed_nofini;
	}

	run_err = asidrv_run(test, seqnum, &asi_active);
	if (run_err) {
		pr_debug("ASI run error %d\n", run_err);
	} else {
		pr_debug("ASI run okay, ASI is %s\n",
			 asi_active ? "active" : "inactive");
	}

	asidrv_run_fini(test);

failed_nofini:
	rparam.run_error = run_err;
	rparam.asi_active = asi_active;

	if (copy_to_user(urparam, &rparam, sizeof(rparam)))
		return -EFAULT;

	return 0;
}

/*
 * ASI fault ioctls
 */

static int asidrv_ioctl_list_fault(struct asi *asi, unsigned long arg)
{
	struct asidrv_fault_list __user *uflist;
	struct asidrv_fault_list *flist;
	size_t flist_size;
	__u32 uflist_len;
	int i;

	uflist = (struct asidrv_fault_list __user *)arg;
	if (copy_from_user(&uflist_len, &uflist->length, sizeof(uflist_len)))
		return -EFAULT;

	uflist_len = min_t(unsigned int, uflist_len, ASI_FAULT_LOG_SIZE);

	flist_size = sizeof(*flist) + sizeof(struct asidrv_fault) * uflist_len;
	flist = kzalloc(flist_size, GFP_KERNEL);
	if (!flist)
		return -ENOMEM;

	for (i = 0; i < ASI_FAULT_LOG_SIZE; i++) {
		if (!asi->fault_log[i].address)
			break;
		if (i < uflist_len) {
			flist->fault[i].addr = asi->fault_log[i].address;
			flist->fault[i].count = asi->fault_log[i].count;
			sprint_symbol(flist->fault[i].symbol,
				      asi->fault_log[i].address);
		}
	}
	flist->length = i;

	if (copy_to_user(uflist, flist, flist_size)) {
		kfree(flist);
		return -EFAULT;
	}

	if (i >= ASI_FAULT_LOG_SIZE)
		pr_warn("ASI %p: fault log buffer is full [%d]\n", asi, i);

	kfree(flist);

	return 0;
}

static int asidrv_ioctl_clear_fault(struct asi *asi)
{
	int i;

	for (i = 0; i < ASI_FAULT_LOG_SIZE; i++) {
		if (!asi->fault_log[i].address)
			break;
		asi->fault_log[i].address = 0;
	}

	pr_debug("ASI %p: faults cleared\n", asi);
	return 0;
}

static int asidrv_ioctl_log_fault_stack(struct asi *asi, bool log_stack)
{
	if (log_stack) {
		asi->fault_log_policy |= ASI_FAULT_LOG_STACK;
		pr_debug("ASI %p: setting fault stack\n", asi);
	} else {
		asi->fault_log_policy &= ~ASI_FAULT_LOG_STACK;
		pr_debug("ASI %p: clearing fault stack\n", asi);
	}

	return 0;
}

/*
 * ASI decorated pagetable ioctls
 */

static int asidrv_ioctl_add_mapping(struct dpt *dpt, unsigned long arg)
{
	struct asidrv_mapping_list __user *umlist;
	struct asidrv_mapping mapping;
	__u32 umlist_len;
	int i, err;

	umlist = (struct asidrv_mapping_list *)arg;
	if (copy_from_user(&umlist_len, &umlist->length, sizeof(umlist_len)))
		return -EFAULT;

	err = 0;
	for (i = 0; i < umlist_len; i++) {
		if (copy_from_user(&mapping, &umlist->mapping[i],
				   sizeof(mapping))) {
			err = -EFAULT;
			break;
		}

		pr_debug("add mapping %llx/%llx/%u %s\n",
			 mapping.addr, mapping.size, mapping.level,
			 mapping.percpu ? "percpu" : "");

		if (mapping.percpu) {
			if (mapping.level != PGT_LEVEL_PTE) {
				err = -EINVAL;
				break;
			}
			err = dpt_map_percpu(dpt, (void *)mapping.addr,
					     mapping.size);
		} else {
			err = dpt_map_range(dpt, (void *)mapping.addr,
					    mapping.size, mapping.level);
		}
		if (err)
			break;
	}

	if (err)
		return (i == 0) ? err : i;

	return 0;
}

static int asidrv_ioctl_clear_mapping(struct dpt *dpt, unsigned long arg)
{
	struct asidrv_mapping_list __user *umlist;
	struct asidrv_mapping mapping;
	__u32 umlist_len;
	int err, i;

	umlist = (struct asidrv_mapping_list *)arg;
	if (copy_from_user(&umlist_len, &umlist->length, sizeof(umlist_len)))
		return -EFAULT;

	err = 0;
	for (i = 0; i < umlist_len; i++) {
		if (copy_from_user(&mapping, &umlist->mapping[i],
				   sizeof(mapping))) {
			err = -EFAULT;
			break;
		}

		pr_debug("clear mapping %llx %s\n",
			 mapping.addr, mapping.percpu ? "percpu" : "");

		if (mapping.percpu)
			dpt_unmap_percpu(dpt, (void *)mapping.addr);
		else
			dpt_unmap(dpt, (void *)mapping.addr);
	}

	if (err)
		return (i == 0) ? err : i;

	return 0;
}

static int asidrv_ioctl_list_mapping(struct dpt *dpt, unsigned long arg)
{
	struct asidrv_mapping_list __user *umlist;
	struct asidrv_mapping_list *mlist;
	struct dpt_range_mapping *range;
	unsigned long addr;
	size_t mlist_size;
	__u32 umlist_len;
	int i;

	umlist = (struct asidrv_mapping_list *)arg;
	if (copy_from_user(&umlist_len, &umlist->length, sizeof(umlist_len)))
		return -EFAULT;

	umlist_len = min_t(unsigned int, umlist_len, 512);

	mlist_size = sizeof(*mlist) +
		sizeof(struct asidrv_mapping) * umlist_len;
	mlist = kzalloc(mlist_size, GFP_KERNEL);
	if (!mlist)
		return -ENOMEM;

	i = 0;
	list_for_each_entry(range, &dpt->mapping_list, list) {
		if (i < umlist_len) {
			addr = (__u64)range->ptr;
			mlist->mapping[i].addr = addr;
			mlist->mapping[i].size = range->size;
			mlist->mapping[i].level = range->level;
		}
		i++;
	}
	mlist->length = i;

	if (copy_to_user(umlist, mlist, mlist_size)) {
		kfree(mlist);
		return -EFAULT;
	}

	kfree(mlist);

	return 0;
}

static long asidrv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct asidrv_test *test = asidrv_test;
	struct asi *asi = test->asi;
	struct dpt *dpt = test->dpt;

	switch (cmd) {

	/* ASI fault ioctls */

	case ASIDRV_IOCTL_LIST_FAULT:
		return asidrv_ioctl_list_fault(asi, arg);

	case ASIDRV_IOCTL_CLEAR_FAULT:
		return asidrv_ioctl_clear_fault(asi);

	case ASIDRV_IOCTL_LOG_FAULT_STACK:
		return asidrv_ioctl_log_fault_stack(asi, arg);

	/* ASI decorated pagetable ioctls */

	case ASIDRV_IOCTL_LIST_MAPPING:
		return asidrv_ioctl_list_mapping(dpt, arg);

	case ASIDRV_IOCTL_ADD_MAPPING:
		return asidrv_ioctl_add_mapping(dpt, arg);

	case ASIDRV_IOCTL_CLEAR_MAPPING:
		return asidrv_ioctl_clear_mapping(dpt, arg);

	/* Test ioctls */

	case ASIDRV_IOCTL_RUN_SEQUENCE:
		return asidrv_ioctl_run_sequence(test, arg);

	default:
		return -ENOTTY;
	};
}

static const struct file_operations asidrv_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= asidrv_ioctl,
	.compat_ioctl	= compat_ptr_ioctl,
};

static struct miscdevice asidrv_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KBUILD_MODNAME,
	.fops = &asidrv_fops,
};

static int __init asidrv_init(void)
{
	int err;

	asidrv_test = asidrv_test_create();
	if (!asidrv_test)
		return -ENOMEM;

	err = misc_register(&asidrv_miscdev);
	if (err) {
		asidrv_test_destroy(asidrv_test);
		asidrv_test = NULL;
	}

	return err;
}

static void __exit asidrv_exit(void)
{
	asidrv_test_destroy(asidrv_test);
	asidrv_test = NULL;
	misc_deregister(&asidrv_miscdev);
}

module_init(asidrv_init);
module_exit(asidrv_exit);

MODULE_AUTHOR("Alexandre Chartre <alexandre.chartre@oracle.com>");
MODULE_DESCRIPTION("Privileged interface to ASI");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
