// SPDX-License-Identifier: GPL-2.0

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/kmemleak.h>
#include <asm/bitsperlong.h>
#include <asm/page_types.h>

#include "kallsyms.h"

/* from mm/page_alloc.c */

// 334
static unsigned long nr_kernel_pages __initdata;
static unsigned long nr_all_pages __initdata;

// 7728
#ifdef CONFIG_NUMA
int hashdist = HASHDIST_DEFAULT;

static int __init set_hashdist(char *str)
{
	if (!str)
		return 0;
	hashdist = simple_strtoul(str, &str, 0);
	return 1;
}
__setup("hashdist=", set_hashdist);
#endif

// 8138
#ifndef __HAVE_ARCH_RESERVED_KERNEL_PAGES
/*
 * Returns the number of pages that arch has reserved but
 * is not known to alloc_large_system_hash().
 */
static unsigned long __init arch_reserved_kernel_pages(void)
{
	return 0;
}
#endif

// 8158
/*
 * Adaptive scale is meant to reduce sizes of hash tables on large memory
 * machines. As memory size is increased the scale is also increased but at
 * slower pace.  Starting from ADAPT_SCALE_BASE (64G), every time memory
 * quadruples the scale is increased by one, which means the size of hash table
 * only doubles, instead of quadrupling as well.
 * Because 32-bit systems cannot have large physical memory, where this scaling
 * makes sense, it is disabled on such platforms.
 */
#if __BITS_PER_LONG > 32
#define ADAPT_SCALE_BASE	(64ul << 30)
#define ADAPT_SCALE_SHIFT	2
#define ADAPT_SCALE_NPAGES	(ADAPT_SCALE_BASE >> PAGE_SHIFT)
#endif

// 8170
/*
 * allocate a large system hash table from bootmem
 * - it is assumed that the hash table must contain an exact power-of-2
 *   quantity of entries
 * - limit is the number of hash buckets, not the total allocation size
 */
void *__init alloc_large_system_hash(const char *tablename,
				     unsigned long bucketsize,
				     unsigned long numentries,
				     int scale,
				     int flags,
				     unsigned int *_hash_shift,
				     unsigned int *_hash_mask,
				     unsigned long low_limit,
				     unsigned long high_limit)
{
	unsigned long long max = high_limit;
	unsigned long log2qty, size;
	void *table = NULL;
	gfp_t gfp_flags;
	bool virt;

	/* allow the kernel cmdline to have a say */
	if (!numentries) {
		/* round applicable memory size up to nearest megabyte */
		numentries = nr_kernel_pages;
		numentries -= arch_reserved_kernel_pages();

		/* It isn't necessary when PAGE_SIZE >= 1MB */
		if (PAGE_SHIFT < 20)
			numentries = round_up(numentries, (1<<20)/PAGE_SIZE);

#if __BITS_PER_LONG > 32
		if (!high_limit) {
			unsigned long adapt;

			for (adapt = ADAPT_SCALE_NPAGES; adapt < numentries;
			     adapt <<= ADAPT_SCALE_SHIFT)
				scale++;
		}
#endif

		/* limit to 1 bucket per 2^scale bytes of low memory */
		if (scale > PAGE_SHIFT)
			numentries >>= (scale - PAGE_SHIFT);
		else
			numentries <<= (PAGE_SHIFT - scale);

		/* Make sure we've got at least a 0-order allocation.. */
		if (unlikely(flags & HASH_SMALL)) {
			/* Makes no sense without HASH_EARLY */
			WARN_ON(!(flags & HASH_EARLY));
			if (!(numentries >> *_hash_shift)) {
				numentries = 1UL << *_hash_shift;
				BUG_ON(!numentries);
			}
		} else if (unlikely((numentries * bucketsize) < PAGE_SIZE))
			numentries = PAGE_SIZE / bucketsize;
	}
	numentries = roundup_pow_of_two(numentries);

	/* limit allocation size to 1/16 total memory by default */
	if (max == 0) {
		max = ((unsigned long long)nr_all_pages << PAGE_SHIFT) >> 4;
		do_div(max, bucketsize);
	}
	max = min(max, 0x80000000ULL);

	if (numentries < low_limit)
		numentries = low_limit;
	if (numentries > max)
		numentries = max;

	log2qty = ilog2(numentries);

	gfp_flags = (flags & HASH_ZERO) ? GFP_ATOMIC | __GFP_ZERO : GFP_ATOMIC;
	do {
		virt = false;
		size = bucketsize << log2qty;
        printk(KERN_INFO "alloc_large_system_hash virt: %d, get_order(size): %d, MAX_ORDER: %d, hashdist: %d",
            virt, get_order(size), MAX_ORDER, hashdist);
		if (flags & HASH_EARLY) {
			if (flags & HASH_ZERO)
				table = memblock_alloc(size, SMP_CACHE_BYTES);
			else
				table = memblock_alloc_raw(size,
							   SMP_CACHE_BYTES);
		} else if (get_order(size) >= MAX_ORDER || hashdist) {
			table = __vmalloc(size, gfp_flags, PAGE_KERNEL);
			virt = true;
		} else {
			/*
			 * If bucketsize is not a power-of-two, we may free
			 * some pages at the end of hash table which
			 * alloc_pages_exact() automatically does
			 */
			table = alloc_pages_exact(size, gfp_flags);
			kmemleak_alloc(table, size, 1, gfp_flags);
		}
	} while (!table && size > PAGE_SIZE && --log2qty);

	if (!table)
		panic("Failed to allocate %s hash table\n", tablename);

	pr_info("%s hash table entries: %ld (order: %d, %lu bytes, %s)\n",
		tablename, 1UL << log2qty, ilog2(size) - PAGE_SHIFT, size,
		virt ? "vmalloc" : "linear");

	if (_hash_shift)
		*_hash_shift = log2qty;
	if (_hash_mask)
		*_hash_mask = (1 << log2qty) - 1;

	return table;
}