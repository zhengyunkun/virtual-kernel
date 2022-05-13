/* SPDX-License-Identifier: GPL-2.0 */
#ifndef ARCH_X86_MM_ASI_H
#define ARCH_X86_MM_ASI_H

#ifdef CONFIG_ADDRESS_SPACE_ISOLATION

/*
 * An Address Space Isolation (ASI) is defined with a struct asi and
 * associated with an ASI type (struct asi_type). All ASIs of the same
 * type reference the same ASI type.
 *
 * An ASI type has a unique PCID prefix (a value in the range [1, 255])
 * which is used to define the PCID used for the ASI CR3 value. The
 * first four bits of the ASI PCID come from the kernel PCID (a value
 * between 1 and 6, see TLB_NR_DYN_ASIDS). The remaining 8 bits are
 * filled with the ASI PCID prefix.
 *
 *   ASI PCID = (ASI Type PCID Prefix << 4) | Kernel PCID
 *
 * The ASI PCID is used to optimize TLB flushing when switching between
 * the kernel and ASI pagetables. The optimization is valid only when
 * a task switches between ASI of different types. If a task switches
 * between different ASIs with the same type then the ASI TLB the task
 * is switching to will always be flushed.
 */

#define ASI_PCID_PREFIX_SHIFT	4
#define ASI_PCID_PREFIX_MASK	0xff0
#define ASI_KERNEL_PCID_MASK	0x00f

/*
 * We use bit 12 of a pagetable pointer (and so of the CR3 value) as
 * a way to know if a pointer/CR3 is referencing a full kernel page
 * table or an ASI page table.
 *
 * A full kernel pagetable is always located on the first half of an
 * 8K buffer, while an ASI pagetable is always located on the second
 * half of an 8K buffer.
 */
#define ASI_PGTABLE_BIT		PAGE_SHIFT
#define ASI_PGTABLE_MASK	(1 << ASI_PGTABLE_BIT)

#ifndef __ASSEMBLY__

#include <linux/export.h>

#include <asm/asi_session.h>
#include <asm/dpt.h>

/*
 * ASI_NR_DYN_ASIDS is the same as TLB_NR_DYN_ASIDS. We can't directly
 * use TLB_NR_DYN_ASIDS because asi.h and tlbflush.h can't both include
 * each other.
 */
#define ASI_TLB_NR_DYN_ASIDS	6

struct asi_tlb_pgtable {
	u64 id;
	u64 gen;
};

struct asi_tlb_state {
	struct asi_tlb_pgtable	tlb_pgtables[ASI_TLB_NR_DYN_ASIDS];
};

#ifdef CONFIG_PAGE_TABLE_ISOLATION
#define ASI_PCID_PREFIX_USER		0x80	/* user ASI */
#endif
#define ASI_PCID_PREFIX_TEST		0xff	/* test ASI */
#define ASI_PCID_PREFIX_VKERNEL	0xf0	/*vkernel ASI */

struct asi_type {
	int			pcid_prefix;	/* PCID prefix */
	struct asi_tlb_state	*tlb_state;	/* percpu ASI TLB state */
	atomic64_t		last_pgtable_id; /* last id for this type */
	bool			fault_abort;	/* abort ASI on fault? */
};

/*
 * Macro to define and declare an ASI type.
 *
 * Declaring an ASI type will also define an inline function
 * (asi_create_<typename>()) to easily create an ASI of the
 * specified type.
 */
#define DEFINE_ASI_TYPE(name, pcid_prefix, fault_abort)		\
	DEFINE_PER_CPU(struct asi_tlb_state, asi_tlb_ ## name);	\
	struct asi_type asi_type_ ## name = {			\
		pcid_prefix,					\
		&asi_tlb_ ## name,				\
		ATOMIC64_INIT(1),				\
		fault_abort					\
	};							\
	EXPORT_SYMBOL(asi_type_ ## name)

#define DECLARE_ASI_TYPE(name)				\
	extern struct asi_type asi_type_ ## name;	\
	DECLARE_ASI_CREATE(name)

#define DECLARE_ASI_CREATE(name)			\
static inline struct asi *asi_create_ ## name(void)	\
{							\
	return asi_create(&asi_type_ ## name);		\
}

/* ASI fault log size */
#define ASI_FAULT_LOG_SIZE      128

/*
 * Options to specify the fault log policy when a fault occurs
 * while using ASI.
 *
 * When set, ASI_FAULT_LOG_KERNEL|USER log the address and location
 * of the fault. In addition, if ASI_FAULT_LOG_STACK is set, the stack
 * trace where the fault occurred is also logged.
 *
 * Faults are logged only for ASIs with a type which aborts ASI on an
 * ASI fault (see fault_abort in struct asi_type).
 */
#define ASI_FAULT_LOG_KERNEL	0x01	/* log kernel faults */
#define ASI_FAULT_LOG_USER	0x02	/* log user faults */
#define ASI_FAULT_LOG_STACK	0x04	/* log stack trace */

enum asi_fault_origin {
	ASI_FAULT_KERNEL = ASI_FAULT_LOG_KERNEL,
	ASI_FAULT_USER = ASI_FAULT_LOG_USER,
};

struct asi_fault_log {
	unsigned long		address;	/* fault address */
	unsigned long		count;		/* fault count */
};

struct asi {
	struct asi_type		*type;		/* ASI type */
	pgd_t			*pagetable;	/* ASI pagetable */
	u64			pgtable_id;	/* ASI pagetable ID */
	atomic64_t		pgtable_gen;	/* ASI pagetable generation */
	unsigned long		base_cr3;	/* base ASI CR3 */
	spinlock_t		fault_lock;	/* protect fault_log_* */
	struct asi_fault_log	fault_log[ASI_FAULT_LOG_SIZE];
	int			fault_log_policy; /* fault log policy */
};

void asi_schedule_out(struct task_struct *task);
void asi_schedule_in(struct task_struct *task);
bool asi_fault(struct pt_regs *regs, unsigned long error_code,
	       unsigned long address, enum asi_fault_origin fault_origin);
void asi_deferred_enter(struct asi *asi);

extern struct asi *asi_create(struct asi_type *type);
extern void asi_destroy(struct asi *asi);
extern void asi_set_pagetable(struct asi *asi, pgd_t *pagetable);
extern int asi_enter(struct asi *asi);
extern void asi_exit(struct asi *asi);
extern int asi_init_dpt(struct dpt *dpt);

#ifdef CONFIG_PAGE_TABLE_ISOLATION
DECLARE_ASI_TYPE(user);
#endif
DECLARE_ASI_TYPE(test);
DECLARE_ASI_TYPE(vkernel)

static inline void asi_set_log_policy(struct asi *asi, int policy)
{
	asi->fault_log_policy = policy;
}

#else  /* __ASSEMBLY__ */

#include <asm/alternative-asm.h>
#include <asm/asm-offsets.h>
#include <asm/cpufeatures.h>
#include <asm/percpu.h>
#include <asm/processor-flags.h>

#define THIS_ASI_SESSION_asi		\
	PER_CPU_VAR(cpu_tlbstate + TLB_STATE_asi)
#define THIS_ASI_SESSION_isolation_cr3	\
	PER_CPU_VAR(cpu_tlbstate + TLB_STATE_asi_isolation_cr3)
#define THIS_ASI_SESSION_original_cr3	\
	PER_CPU_VAR(cpu_tlbstate + TLB_STATE_asi_original_cr3)
#define THIS_ASI_SESSION_idepth	\
	PER_CPU_VAR(cpu_tlbstate + TLB_STATE_asi_idepth)

.macro SET_NOFLUSH_BIT	reg:req
	bts	$X86_CR3_PCID_NOFLUSH_BIT, \reg
.endm

/*
 * Switch CR3 to the original kernel CR3 value. This is used when exiting
 * interrupting ASI.
 */
.macro ASI_SWITCH_TO_KERNEL_CR3 scratch_reg:req
	/*
	 * KERNEL pages can always resume with NOFLUSH as we do
	 * explicit flushes.
	 */
	movq	THIS_ASI_SESSION_original_cr3, \scratch_reg
	ALTERNATIVE "", "SET_NOFLUSH_BIT \scratch_reg", X86_FEATURE_PCID
	movq	\scratch_reg, %cr3
.endm

/*
 * Interrupt ASI, when there's an interrupt or exception while we
 * were running with ASI.
 */
.macro ASI_INTERRUPT scratch_reg:req
	movq	THIS_ASI_SESSION_asi, \scratch_reg
	testq	\scratch_reg, \scratch_reg
	jz	.Lasi_interrupt_done_\@
	incl	THIS_ASI_SESSION_idepth
	cmp	$1, THIS_ASI_SESSION_idepth
	jne	.Lasi_interrupt_done_\@
	ASI_SWITCH_TO_KERNEL_CR3 \scratch_reg
.Lasi_interrupt_done_\@:
.endm

.macro ASI_PREPARE_RESUME
	call	asi_prepare_resume
.endm

/*
 * Resume ASI, after it was interrupted by an interrupt or an exception.
 */
.macro ASI_RESUME scratch_reg:req
	movq	THIS_ASI_SESSION_asi, \scratch_reg
	testq	\scratch_reg, \scratch_reg
	jz	.Lasi_resume_done_\@
	decl	THIS_ASI_SESSION_idepth
	jnz	.Lasi_resume_done_\@
	movq	THIS_ASI_SESSION_isolation_cr3, \scratch_reg
	mov	\scratch_reg, %cr3
.Lasi_resume_done_\@:
.endm

/*
 * Interrupt ASI, special processing when ASI is interrupted by a NMI
 * or a paranoid interrupt/exception.
 */
.macro ASI_INTERRUPT_AND_SAVE_CR3 scratch_reg:req save_reg:req
	movq	%cr3, \save_reg
	/*
	 * Test the ASI PCID bits. If set, then an ASI page table
	 * is active. If clear, CR3 already has the kernel page table
	 * active.
	 */
	bt	$ASI_PGTABLE_BIT, \save_reg
	jnc	.Ldone_\@
	incl	THIS_ASI_SESSION_idepth
	ASI_SWITCH_TO_KERNEL_CR3 \scratch_reg
.Ldone_\@:
.endm

/*
 * Resume ASI, special processing when ASI is resumed from a NMI
 * or a paranoid interrupt/exception.
 */
.macro ASI_RESUME_AND_RESTORE_CR3 save_reg:req

	ALTERNATIVE "jmp .Lwrite_cr3_\@", "", X86_FEATURE_PCID

	bt	$ASI_PGTABLE_BIT, \save_reg
	jnc	.Lrestore_kernel_cr3_\@

	/*
	 * Restore ASI CR3. We need to update TLB flushing
	 * information.
	 */
	movq	THIS_ASI_SESSION_asi, %rdi
	movq	\save_reg, %rsi
	call	asi_update_flush
	movq	%rax, THIS_ASI_SESSION_isolation_cr3
	decl	THIS_ASI_SESSION_idepth
	movq	%rax, %cr3
	jmp	.Ldone_\@

.Lrestore_kernel_cr3_\@:
	/*
	 * Restore kernel CR3. KERNEL pages can always resume
	 * with NOFLUSH as we do explicit flushes.
	 */
	SET_NOFLUSH_BIT \save_reg

.Lwrite_cr3_\@:
	movq	\save_reg, %cr3

.Ldone_\@:
.endm

#endif	/* __ASSEMBLY__ */

#endif	/* CONFIG_ADDRESS_SPACE_ISOLATION */

#endif
