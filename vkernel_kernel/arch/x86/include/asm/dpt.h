/* SPDX-License-Identifier: GPL-2.0 */
#ifndef ARCH_X86_MM_DPT_H
#define ARCH_X86_MM_DPT_H

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/xarray.h>
#include <linux/kallsyms.h>

#include <asm/pgtable.h>
#include <asm/asi.h>
#include <asm/sections.h>

enum page_table_level {
	PGT_LEVEL_PTE,
	PGT_LEVEL_PMD,
	PGT_LEVEL_PUD,
	PGT_LEVEL_P4D,
	PGT_LEVEL_PGD
};

/*
 * Structure to keep track of address ranges mapped into a decorated
 * page-table.
 */
struct dpt_range_mapping {
	struct list_head list;
	void *ptr;			/* range start address */
	size_t size;			/* range size */
	enum page_table_level level;	/* mapping level */
	int refcnt;			/* reference count (for overlap) */
};

/*
 * A decorated page-table (dpt) encapsulates a native page-table (e.g.
 * a PGD) and maintain additional attributes related to this page-table.
 */
struct dpt {
	spinlock_t		lock;		/* protect all attributes */
	pgd_t			*pagetable;	/* the actual page-table */
	unsigned int		alignment;	/* page-table alignment */
	struct list_head	mapping_list;	/* list of VA range mapping */

	/*
	 * A page-table can have direct references to another page-table,
	 * at different levels (PGD, P4D, PUD, PMD). When freeing or
	 * modifying a page-table, we should make sure that we free/modify
	 * parts effectively allocated to the actual page-table, and not
	 * parts of another page-table referenced from this page-table.
	 *
	 * To do so, the backend_pages XArray is used to keep track of pages
	 * used for this page-table.
	 */
	struct xarray		backend_pages;		/* page-table pages */
	unsigned long		backend_pages_count;	/* pages count */
};

extern bool dpt_search_range_mapping(struct dpt *dpt,unsigned long addr);
extern bool asi_verify_and_map(struct asi *asi, struct pt_regs *regs, unsigned long addr,
			unsigned long hw_error_code);
extern int dpt_verify_and_map(struct asi *asi, struct pt_regs *regs, unsigned long addr,
			unsigned long hw_error_code);
extern struct dpt *dpt_create(unsigned int pgt_alignment);
extern void dpt_destroy(struct dpt *dpt);
extern int dpt_map_range(struct dpt *dpt, void *ptr, size_t size,
			 enum page_table_level level);
extern int dpt_map(struct dpt *dpt, void *ptr, unsigned long size);
extern void dpt_unmap(struct dpt *dpt, void *ptr);
extern int dpt_remap(struct dpt *dpt, void **mapping, void *ptr, size_t size);

static inline int dpt_map_module(struct dpt *dpt, char *module_name)
{
	struct module *module;

	module = find_module(module_name);
	if (!module)
		return -ESRCH;

	return dpt_map(dpt, module->core_layout.base, module->core_layout.size);
}

/*
 * Copy the memory mapping for the current module. This is defined as a
 * macro to ensure it is expanded in the module making the call so that
 * THIS_MODULE has the correct value.
 */
#define DPT_MAP_THIS_MODULE(dpt)			\
	(dpt_map(dpt, THIS_MODULE->core_layout.base,	\
		 THIS_MODULE->core_layout.size))

extern int dpt_map_percpu(struct dpt *dpt, void *percpu_ptr, size_t size);
extern void dpt_unmap_percpu(struct dpt *dpt, void *percpu_ptr);

#define	DPT_MAP_CPUVAR(dpt, cpuvar)			\
	dpt_map_percpu(dpt, &cpuvar, sizeof(cpuvar))

#endif
