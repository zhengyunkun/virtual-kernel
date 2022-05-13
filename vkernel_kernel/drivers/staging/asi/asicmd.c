// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, Oracle and/or its affiliates.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "asidrv.h"

static const char * const page_table_level[] = {
	"PTE", "PMD", "PUD", "P4D", "PGD"
};

struct asidrv_test {
	char		*name;		/* test name */
	enum asidrv_seqnum seqnum;	/* sequence */
	bool		asi_active;	/* ASI active at the end of test? */
	char		*desc;		/* test description */
};

struct asidrv_test test_list[] = {
	{ "nop", ASIDRV_SEQ_NOP, true,
	  "enter/exit ASI and nothing else" },
	{ "mem", ASIDRV_SEQ_MEM, false,
	  "enter ASI and accessed an unmapped buffer" },
	{ "memmap", ASIDRV_SEQ_MEMMAP, true,
	  "enter ASI and accessed a mapped buffer" },
	{ "intr", ASIDRV_SEQ_INTERRUPT, true,
	  "receive an interruption while running with ASI" },
	{ "nmi", ASIDRV_SEQ_NMI, true,
	  "receive a NMI while running with ASI" },
	{ "intrnmi", ASIDRV_SEQ_INTRNMI, true,
	  "receive a NMI in an interrupt received while running with ASI" },
	{ "sched", ASIDRV_SEQ_SCHED, true,
	  "call schedule() while running with ASI" },
	{ "printk", ASIDRV_SEQ_PRINTK, true,
	  "call printk() while running with ASI" },
};

#define	TEST_LIST_SIZE	(sizeof(test_list) / sizeof(test_list[0]))

static void usage(void)
{
	int i;

	printf("Usage: asicmd (<cmd>|<test>...)\n");
	printf("\n");
	printf("Commands:\n");
	printf("  all      - run all tests\n");
	printf("  fault    - list ASI faults\n");
	printf("  fltclr   - clear ASI faults\n");
	printf("  stkon    - show stack on ASI fault\n");
	printf("  stkoff   - do not show stack on ASI fault\n");
	printf("  map      - list ASI mappings\n");
	printf("  mapadd   - add ASI mappings\n");
	printf("  mapclr   - clear ASI mapping\n");
	printf("\n");
	printf("Tests:\n");
	for (i = 0; i < TEST_LIST_SIZE; i++)
		printf("  %-10s - %s\n", test_list[i].name, test_list[i].desc);
}

static void usage_mapadd(void)
{
	printf("usage: asicmd mapadd [percpu:]<addr>:<size>[:<level>]\n");
}

static void usage_mapclr(void)
{
	printf("usage: asicmd mapclr [percpu:]<addr>\n");
}

static void asidrv_run_test(int fd, struct asidrv_test *test)
{
	struct asidrv_run_param rparam;
	int err;

	printf("Test %s (sequence %d)\n", test->name, test->seqnum);

	rparam.sequence = test->seqnum;

	err = ioctl(fd, ASIDRV_IOCTL_RUN_SEQUENCE, &rparam);

	printf("  - rv = %d ; ", err);
	if (err < 0) {
		printf("error %d\n", errno);
	} else {
		printf("result = %d ; ", rparam.run_error);
		printf("%s\n",
		       rparam.asi_active ? "asi active" : "asi inactive");
	}

	printf("  - expect = %s\n",
	       test->asi_active ? "asi active" : "asi inactive");

	if (err < 0)
		printf("ERROR - error %d\n", errno);
	else if (rparam.run_error != ASIDRV_RUN_ERR_NONE)
		printf("TEST ERROR - error %d\n", rparam.run_error);
	else if (test->asi_active != rparam.asi_active)
		printf("TEST FAILED - unexpected ASI state\n");
	else
		printf("TEST OK\n");
}

static int asidrv_fault_list(int fd)
{
	struct asidrv_fault_list *flist;
	int i, rv;

	flist = malloc(sizeof(*flist) +
		       sizeof(struct asidrv_fault) * 10);
	if (!flist) {
		perror("malloc flist");
		return -1;
	}

	flist->length = 10;
	rv = ioctl(fd, ASIDRV_IOCTL_LIST_FAULT, flist);
	if (rv < 0) {
		perror("ioctl list fault");
		return -1;
	}

	if (!flist->length) {
		printf("ASI has no fault\n");
		return 0;
	}

	printf("%-18s  %5s  %s\n", "ADDRESS", "COUNT", "SYMBOL");
	for (i = 0; i < flist->length && i < 10; i++) {
		printf("%#18llx  %5u  %s\n",
		       flist->fault[i].addr,
		       flist->fault[i].count,
		       flist->fault[i].symbol);
	}

	return 0;
}

static int asidrv_map_list(int fd)
{
	struct asidrv_mapping_list *mlist;
	int level;
	int i, rv, len = 64;

	mlist = malloc(sizeof(*mlist) +
		       sizeof(struct asidrv_mapping) * len);
	if (!mlist) {
		perror("malloc mlist");
		return -1;
	}

	mlist->length = len;
	rv = ioctl(fd, ASIDRV_IOCTL_LIST_MAPPING, mlist);
	if (rv < 0) {
		perror("ioctl list mapping");
		return -1;
	}

	if (!mlist->length) {
		printf("ASI has no mapping\n");
		return 0;
	}

	printf("%-18s  %18s  %s\n", "ADDRESS", "SIZE", "LEVEL");
	for (i = 0; i < mlist->length && i < len; i++) {
		printf("%#18llx  %#18llx  ",
		       mlist->mapping[i].addr,
		       mlist->mapping[i].size);
		level = mlist->mapping[i].level;
		if (level < 5)
			printf("%5s\n", page_table_level[level]);
		else
			printf("%5d\n", level);
	}
	printf("Mapping List: %d/%d\n", i, mlist->length);

	return 0;
}

static char *asidrv_skip_percpu(char *str, bool *percpup)
{
	int len = sizeof("percpu:") - 1;

	if (!strncmp(str, "percpu:", len)) {
		str += len;
		*percpup = true;
	} else {
		*percpup = false;
	}

	return str;
}

static int asidrv_parse_mapping_clear(char *arg, struct asidrv_mapping *mapping)
{
	char  *s, *end;
	bool percpu;
	__u64 addr;

	s = asidrv_skip_percpu(arg, &percpu);

	addr = strtoull(s, &end, 0);
	if (*end != 0) {
		printf("invalid mapping address '%s'\n", s);
		return -1;
	}

	printf("mapclr %llx%s\n", addr, percpu ? " percpu" : "");

	mapping->addr = addr;
	mapping->size = 0;
	mapping->level = 0;
	mapping->percpu = percpu;

	return 0;
}

static int asidrv_parse_mapping_add(char *arg, struct asidrv_mapping *mapping)
{
	char *s, *end;
	__u64 addr, size;
	__u32 level;
	bool percpu;
	int i;

	s = asidrv_skip_percpu(arg, &percpu);

	s = strtok(s, ":");
	if (!s) {
		printf("mapadd: <addr> not found\n");
		return -1;
	}

	addr = strtoull(s, &end, 0);
	if (*end != 0) {
		printf("invalid mapping address '%s'\n", s);
		return -1;
	}

	s = strtok(NULL, ":");
	if (!s) {
		printf("mapadd: <size> not found\n");
		return -1;
	}
	size = strtoull(s, &end, 0);
	if (*end != 0) {
		printf("mapadd: invalid size %s\n", s);
		return -1;
	}

	s = strtok(NULL, ":");
	if (!s) {
		level = 0;
	} else {
		/* lookup page table level name */
		level = -1;
		for (i = 0; i < 5; i++) {
			if (!strcasecmp(s, page_table_level[i])) {
				level = i;
				break;
			}
		}
		if (level == -1) {
			level = strtoul(s, &end, 0);
			if (*end != 0 || level >= 5) {
				printf("mapadd: invalid level %s\n", s);
				return -1;
			}
		}
	}

	printf("mapadd %llx/%llx/%u%s\n", addr, size, level,
	       percpu ? " percpu" : "");

	mapping->addr = addr;
	mapping->size = size;
	mapping->level = level;
	mapping->percpu = percpu;

	return 0;
}

static int asidrv_map_change(int fd, unsigned long cmd, char *arg)
{
	struct asidrv_mapping_list *mlist;
	int i, count, err;
	char *s;

	count = 0;
	for (s = arg; s; s = strchr(s + 1, ','))
		count++;

	mlist = malloc(sizeof(mlist) + sizeof(struct asidrv_mapping) * count);
	if (!mlist) {
		perror("malloc mapping list");
		return -ENOMEM;
	}

	for (i = 0; i < count; i++) {
		s = strchr(arg, ',');
		if (s)
			s[0] = '\0';

		if (cmd == ASIDRV_IOCTL_ADD_MAPPING) {
			err = asidrv_parse_mapping_add(arg,
						       &mlist->mapping[i]);
		} else {
			err = asidrv_parse_mapping_clear(arg,
							 &mlist->mapping[i]);
		}
		if (err)
			goto done;
		arg = s + 1;
	}

	mlist->length = count;
	err = ioctl(fd, cmd, mlist);
	if (err < 0) {
		perror("ioctl mapping");
		err = errno;
	} else if (err > 0) {
		/* partial error */
		printf("ioctl mapping: partial failure (%d/%d)\n",
		       err, count);
		for (i = 0; i < count; i++) {
			printf("  %#llx: ", mlist->mapping[i].addr);
			if (i < err)
				printf("done\n");
			else if (i == err)
				printf("failed\n");
			else
				printf("not done\n");
		}
		err = -1;
	}

done:
	free(mlist);

	return err;
}

int main(int argc, char *argv[])
{
	bool run_all, run;
	int i, j, fd, err;
	char *test;

	if (argc <= 1) {
		usage();
		return 2;
	}

	fd = open("/dev/asi", O_RDONLY);
	if (fd == -1) {
		perror("open /dev/asi");
		return 1;
	}

	for (i = 1; i < argc; i++) {
		test = argv[i];

		if (!strcmp(test, "fault")) {
			asidrv_fault_list(fd);
			continue;

		} else if (!strcmp(test, "fltclr")) {
			err = ioctl(fd, ASIDRV_IOCTL_CLEAR_FAULT);
			if (err)
				perror("ioctl clear fault");
			continue;

		} else if (!strcmp(test, "stkon")) {
			err = ioctl(fd, ASIDRV_IOCTL_LOG_FAULT_STACK, true);
			if (err)
				perror("ioctl log fault stack");
			continue;

		} else if (!strcmp(test, "stkoff")) {
			err = ioctl(fd, ASIDRV_IOCTL_LOG_FAULT_STACK, false);
			if (err)
				perror("ioctl log fault sstack");
			continue;

		} else if (!strcmp(test, "map")) {
			asidrv_map_list(fd);
			continue;

		} else if (!strcmp(test, "mapadd")) {
			if (++i >= argc) {
				usage_mapadd();
				return 2;
			}
			asidrv_map_change(fd, ASIDRV_IOCTL_ADD_MAPPING,
					  argv[i]);
			continue;

		} else if (!strcmp(test, "mapclr")) {
			if (++i >= argc) {
				usage_mapclr();
				return 2;
			}
			asidrv_map_change(fd, ASIDRV_IOCTL_CLEAR_MAPPING,
					  argv[i]);
			continue;

		} else if (!strcmp(test, "all")) {
			run_all = true;
		} else {
			run_all = false;
		}

		run = false;
		for (j = 0; j < TEST_LIST_SIZE; j++) {
			if (run_all || !strcmp(test, test_list[j].name)) {
				asidrv_run_test(fd, &test_list[j]);
				run = true;
			}
		}

		if (!run)
			printf("Unknown test '%s'\n", test);
	}

	close(fd);

	return 0;
}
