/* SPDX-License-Identifier: GPL-2.0 */
#ifndef ARCH_X86_MM_ASI_SESSION_H
#define ARCH_X86_MM_ASI_SESSION_H

#ifdef CONFIG_ADDRESS_SPACE_ISOLATION

struct asi;

struct asi_session {
	struct asi		*asi;		/* ASI for this session */
	unsigned long		isolation_cr3;	/* cr3 when ASI is active */
	unsigned long		original_cr3;	/* cr3 before entering ASI */
	/*
	 * The interrupt depth (idepth) tracks interrupt (actually
	 * interrupt/exception/NMI) nesting. ASI is interrupted on
	 * the first interrupt, and it is resumed when that interrupt
	 * handler returns.
	 */
	unsigned int		idepth;		/* interrupt depth */
};

#endif	/* CONFIG_ADDRESS_SPACE_ISOLATION */

#endif
