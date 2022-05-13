// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates.
 *
 * Kernel Address Space Isolation (ASI)
 */

#include <linux/mm.h>
#include <linux/sched/debug.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>

#include <asm/asi.h>
#include <asm/bug.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>
#include <asm/traps.h>
#include <asm/dpt.h>

#include <asm/cpufeature.h>
#include <asm/hypervisor.h>
#include <asm/cmdline.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/desc.h>
#include <asm/sections.h>
#include <asm/traps.h>

#ifdef CONFIG_PAGE_TABLE_ISOLATION
DEFINE_ASI_TYPE(user, ASI_PCID_PREFIX_USER, false);
#endif
DEFINE_ASI_TYPE(test, ASI_PCID_PREFIX_TEST, true);
DEFINE_ASI_TYPE(vkernel, ASI_PCID_PREFIX_VKERNEL, true);

void asi_run_fini(struct dpt *dpt)
{
	dpt_unmap(dpt, current);
	dpt_unmap(dpt, current->stack);
}
static void asi_log_fault(struct asi *asi, struct pt_regs *regs,
			   unsigned long error_code, unsigned long address,
			   enum asi_fault_origin fault_origin)
{
	int i;

	/*
	 * Log information about the fault only if this is a fault
	 * we don't know about yet (and the fault log is not full).
	 */
	spin_lock(&asi->fault_lock);
	if (!(asi->fault_log_policy & fault_origin)) {
		spin_unlock(&asi->fault_lock);
		return;
	}
	for (i = 0; i < ASI_FAULT_LOG_SIZE; i++) {
		if (asi->fault_log[i].address == regs->ip) {
			asi->fault_log[i].count++;
			spin_unlock(&asi->fault_lock);
			if (likely(vkernel_asi_enabled(current))) {
				dpt_verify_and_map(asi, regs, address, error_code);
			}
			return;
		}
		if (!asi->fault_log[i].address) {
			asi->fault_log[i].address = regs->ip;
			asi->fault_log[i].count = 1;
			if (likely(vkernel_asi_enabled(current)))
				dpt_verify_and_map(asi, regs, address, error_code);
			break;
		}
	}

	if (i >= ASI_FAULT_LOG_SIZE) {
		pr_warn("ASI %p: fault log buffer is full [%d]\n",
			asi, i);
	}

	pr_info("ASI %p: PF#%d (%ld) at %pS on %px\n", asi, i,
		error_code, (void *)regs->ip, (void *)address);

	if (asi->fault_log_policy & ASI_FAULT_LOG_STACK)
		show_stack(NULL, (unsigned long *)regs->sp);

	spin_unlock(&asi->fault_lock);
}

bool asi_fault(struct pt_regs *regs, unsigned long error_code,
	       unsigned long address, enum asi_fault_origin fault_origin)
{
	struct asi_session *asi_session;
	struct dpt *dpt;

	dpt = current->nsproxy->pid_ns_for_children->dpt;
	/*
	 * If address space isolation was active when the fault occurred
	 * then the page fault handler has interrupted the isolation
	 * (exception handlers interrupt isolation very early) and switched
	 * CR3 back to its original kernel value. So we can safely retrieved
	 * the CPU ASI session.
	 */
	asi_session = &get_cpu_var(cpu_asi_session);

	/*
	 * If address space isolation is not active, or we have a fault
	 * after isolation was aborted then this was not a fault while
	 * using ASI and we don't handle it.
	 */
	if (!asi_session->asi || asi_session->idepth > 1)
		return false;

	/*
	 * We have a fault while the CPU is using address space isolation.
	 * Depending on the ASI fault policy, either:
	 *
	 * - Abort the isolation. The ASI used when the fault occurred is
	 *   aborted, and the faulty instruction is immediately retried.
	 *   The fault is not processed by the system fault handler. The
	 *   fault handler will return immediately, the system will not
	 *   restore the ASI pagetable and will continue to run with the
	 *   full kernel pagetable.
	 *
	 * - Or preserve the isolation. The system fault handler will
	 *   process the fault like any regular fault. The ASI pagetable
	 *   be restored after the fault has been handled and the system
	 *   fault handler returns.
	 */
	if (asi_session->asi->type->fault_abort) {
		asi_log_fault(asi_session->asi, regs, error_code,
			      address, fault_origin);
//		asi_exit(asi_session->asi);
//		asi_run_fini(dpt);
		if (likely(vkernel_asi_enabled(current))) {
			return true;
		}
		asi_session->asi = NULL;
		asi_session->idepth = 0;
		return true;
	}

	return false;
}

struct asi *asi_create(struct asi_type *type)
{
	struct asi *asi;

	if (!type)
		return NULL;

	asi = kzalloc(sizeof(*asi), GFP_KERNEL);
	if (!asi)
		return NULL;

	asi->type = type;
	asi->pgtable_id = atomic64_inc_return(&type->last_pgtable_id);
	atomic64_set(&asi->pgtable_gen, 0);
	spin_lock_init(&asi->fault_lock);
	/* by default, log ASI kernel faults */
	asi->fault_log_policy = ASI_FAULT_LOG_KERNEL;

	return asi;
}
EXPORT_SYMBOL(asi_create);

void asi_destroy(struct asi *asi)
{
	kfree(asi);
}
EXPORT_SYMBOL(asi_destroy);

void asi_set_pagetable(struct asi *asi, pgd_t *pagetable)
{
	/*
	 * Check that the specified pagetable is properly aligned to be
	 * used as an ASI pagetable. If not, the pagetable is ignored
	 * and entering/exiting ASI will do nothing.
	 */
	if (!(((unsigned long)pagetable) & ASI_PGTABLE_MASK)) {
		WARN(1, "ASI %p: invalid ASI pagetable", asi);
		asi->pagetable = NULL;
		return;
	}
	asi->pagetable = pagetable;

	/*
	 * Initialize the invariant part of the ASI CR3 value. We will
	 * just have to complete the PCID with the kernel PCID before
	 * using it.
	 */
	asi->base_cr3 = __sme_pa(asi->pagetable) |
		(asi->type->pcid_prefix << ASI_PCID_PREFIX_SHIFT);

}
EXPORT_SYMBOL(asi_set_pagetable);

/*
 * asi_init_dpt - Initialize a decorated page-table with the minimum
 * mappings for using an ASI. Note that this function doesn't map any
 * stack. If the stack of the task entering an ASI is not mapped then
 * this will trigger a double-fault as soon as the task tries to access
 * its stack.
 */
int asi_init_dpt(struct dpt *dpt)
{
	int err;

	/*
	 * Map the kernel.
	 *
	 * XXX We should check if we can map only kernel text, i.e. map with
	 * size = _etext - _text
	 */
	err = dpt_map(dpt, (void *)__START_KERNEL_map, KERNEL_IMAGE_SIZE);
	if (err)
		return err;

	/*
	 * Map the cpu_entry_area because we need the GDT to be mapped.
	 * Not sure we need anything else from cpu_entry_area.
	 */
	err = dpt_map_range(dpt, (void *)CPU_ENTRY_AREA_PER_CPU, P4D_SIZE,
			    PGT_LEVEL_P4D);
	if (err)
		return err;

	/*
	 * Map fixed_percpu_data to get the stack canary.
	 */
	if (IS_ENABLED(CONFIG_STACKPROTECTOR)) {
		err = DPT_MAP_CPUVAR(dpt, fixed_percpu_data);
		if (err)
			return err;
	}

	/* Map current_task, we need it for __schedule() */
	err = DPT_MAP_CPUVAR(dpt, current_task);
	if (err)
		return err;

	/*
	 * Map the percpu ASI tlbstate. This also maps the asi_session
	 * which is used by interrupt handlers to figure out if we have
	 * entered isolation and switch back to the kernel address space.
	 */
	err = DPT_MAP_CPUVAR(dpt, cpu_tlbstate);
	if (err)
		return err;

	return 0;
}
EXPORT_SYMBOL(asi_init_dpt);

/*
 * Update ASI TLB flush information for the specified ASI CR3 value.
 * Return an updated ASI CR3 value which specified if TLB needs to
 * be flushed or not.
 */
unsigned long asi_update_flush(struct asi *asi, unsigned long asi_cr3)
{
	struct asi_tlb_pgtable *tlb_pgtable;
	struct asi_tlb_state *tlb_state;
	s64 pgtable_gen;
	u16 pcid;

	pcid = asi_cr3 & ASI_KERNEL_PCID_MASK;
	tlb_state = get_cpu_ptr(asi->type->tlb_state);
	tlb_pgtable = &tlb_state->tlb_pgtables[pcid - 1];
	pgtable_gen = atomic64_read(&asi->pgtable_gen);
	if (tlb_pgtable->id == asi->pgtable_id &&
	    tlb_pgtable->gen == pgtable_gen) {
		asi_cr3 |= X86_CR3_PCID_NOFLUSH;
	} else {
		tlb_pgtable->id = asi->pgtable_id;
		tlb_pgtable->gen = pgtable_gen;
	}

	return asi_cr3;
}


/*
 * Switch to the ASI pagetable.
 *
 * If schedule is ASI_SWITCH_NOW, then immediately switch to the ASI
 * pagetable by updating the CR3 register with the ASI CR3 value.
 * Otherwise, if schedule is ASI_SWITCH_ON_RESUME, prepare everything
 * for switching to ASI pagetable but do not update the CR3 register
 * yet. This will be done by the next ASI_RESUME call.
 */

enum asi_switch_schedule {
	ASI_SWITCH_NOW,
	ASI_SWITCH_ON_RESUME,
};

static void asi_switch_to_asi_cr3(struct asi *asi,
				  enum asi_switch_schedule schedule)
{
	unsigned long original_cr3, asi_cr3;
	struct asi_session *asi_session;
	u16 pcid;

	WARN_ON(!irqs_disabled());

	original_cr3 = __get_current_cr3_fast();

	/* build the ASI cr3 value */
	if (boot_cpu_has(X86_FEATURE_PCID)) {
		pcid = original_cr3 & ASI_KERNEL_PCID_MASK;
		asi_cr3 = asi_update_flush(asi, asi->base_cr3 | pcid);
	} else {
		asi_cr3 = asi->base_cr3;
	}

	/* get the ASI session ready for entering ASI */
	asi_session = &get_cpu_var(cpu_asi_session);
	asi_session->asi = asi;
	asi_session->original_cr3 = original_cr3;
	asi_session->isolation_cr3 = asi_cr3;

	if (schedule == ASI_SWITCH_ON_RESUME) {
		/*
		 * Defer the CR3 update the next ASI resume by setting
		 * the interrupt depth to 1.
		 */
		asi_session->idepth = 1;
	} else {
		/* Update CR3 to immediately enter ASI */
		native_write_cr3(asi_cr3);
	}
}

static void asi_switch_to_kernel_cr3(struct asi *asi)
{
	struct asi_session *asi_session;
	unsigned long original_cr3;

	WARN_ON(!irqs_disabled());

	original_cr3 = this_cpu_read(cpu_asi_session.original_cr3);
	if (boot_cpu_has(X86_FEATURE_PCID))
		original_cr3 |= X86_CR3_PCID_NOFLUSH;
	native_write_cr3(original_cr3);

	asi_session = &get_cpu_var(cpu_asi_session);
	asi_session->asi = NULL;
	asi_session->idepth = 0;
}

int asi_enter(struct asi *asi)
{
	struct asi *current_asi;
	unsigned long flags;

	/*
	 * We can re-enter isolation, but only with the same ASI (we don't
	 * support nesting isolation).
	 */
	current_asi = this_cpu_read(cpu_asi_session.asi);
	if (current_asi) {
		if (current_asi != asi) {
			WARN_ON(1);
			return -EBUSY;
		}
		return 0;
	}

	local_irq_save(flags);
	asi_switch_to_asi_cr3(asi, ASI_SWITCH_NOW);
	local_irq_restore(flags);

	return 0;
}
EXPORT_SYMBOL(asi_enter);

void asi_exit(struct asi *asi)
{
	struct asi_session *asi_session;
	struct asi *current_asi;
	unsigned long flags;
	int idepth;

	current_asi = this_cpu_read(cpu_asi_session.asi);
	if (!current_asi) {
		/* already exited */
		return;
	}

	WARN_ON(current_asi != asi);

	idepth = this_cpu_read(cpu_asi_session.idepth);
	if (!idepth) {
		local_irq_save(flags);
		asi_switch_to_kernel_cr3(asi);
		local_irq_restore(flags);
	} else {
		/*
		 * ASI was interrupted so we already switched back
		 * to the back to the kernel page table and we just
		 * need to clear the ASI session.
		 */
		asi_session = &get_cpu_var(cpu_asi_session);
		asi_session->asi = NULL;
		asi_session->idepth = 0;
	}
}
EXPORT_SYMBOL(asi_exit);

void asi_deferred_enter(struct asi *asi)
{
	asi_switch_to_asi_cr3(asi, ASI_SWITCH_ON_RESUME);
}

void asi_prepare_resume(void)
{
	struct asi_session *asi_session;

	asi_session = &get_cpu_var(cpu_asi_session);
	if (!asi_session->asi || asi_session->idepth > 1)
		return;

	asi_switch_to_asi_cr3(asi_session->asi, ASI_SWITCH_ON_RESUME);
}

void asi_schedule_out(struct task_struct *task)
{
	struct asi_session *asi_session;
	unsigned long flags;
	struct asi *asi;

	asi = this_cpu_read(cpu_asi_session.asi);
	if (!asi)
		return;

	/*
	 * Save the ASI session.
	 *
	 * Exit the session if it hasn't been interrupted, otherwise
	 * just save the session state.
	 */
	local_irq_save(flags);
	if (!this_cpu_read(cpu_asi_session.idepth)) {
		asi_switch_to_kernel_cr3(asi);
		task->asi_session.asi = asi;
		task->asi_session.idepth = 0;
	} else {
		asi_session = &get_cpu_var(cpu_asi_session);
		task->asi_session = *asi_session;
		asi_session->asi = NULL;
		asi_session->idepth = 0;
	}
	local_irq_restore(flags);
}

void asi_schedule_in(struct task_struct *task)
{
	struct asi_session *asi_session;
	unsigned long flags;
	struct asi *asi;

	asi = task->asi_session.asi;
	if (!asi)
		return;

	/*
	 * At this point, the CPU shouldn't be using ASI because the
	 * ASI session is expected to be cleared in asi_schedule_out().
	 */
	WARN_ON(this_cpu_read(cpu_asi_session.asi));

	/*
	 * Restore ASI.
	 *
	 * If the task was scheduled out while using ASI, then the ASI
	 * is already setup and we can immediately switch to ASI page
	 * table.
	 *
	 * Otherwise, if the task was scheduled out while ASI was
	 * interrupted, just restore the ASI session.
	 */
	local_irq_save(flags);
	if (!task->asi_session.idepth) {
		asi_switch_to_asi_cr3(asi, ASI_SWITCH_NOW);
	} else {
		asi_session = &get_cpu_var(cpu_asi_session);
		*asi_session = task->asi_session;
	}
	task->asi_session.asi = NULL;
	local_irq_restore(flags);
}
