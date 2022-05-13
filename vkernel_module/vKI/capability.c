// SPDX-License-Identifier: GPL-2.0

#include <linux/security.h>
#include <linux/prctl.h>

/**
 * task_set_capability - set container init processs's capability
 * @caps_data: effective, permitted, inheritable
 * @caps_bounding: bounding, ambient
 * @caps_ambient: ambient
 */
int task_set_capability(struct task_struct *target, unsigned long *caps_data,
		unsigned long *caps_bounding, unsigned long *caps_ambient)
{
	kernel_cap_t effective, inheritable, permitted, _effective, _inheritable, _permitted;
	struct cred *new;
	int ret, i, j, action;

	cap_capget(target, &_effective, &_inheritable, &_permitted);

	// bounding
	if ((1 << CAP_SETPCAP) & _effective.cap[0]) {
		for (i = 0; i <= CAP_LAST_CAP; i++) {
			j = i;
			if (j > 31)
				j %= 32;
			if ((1 << (unsigned int)j) & caps_bounding[i / 32])
				continue;
			ret = cap_task_prctl(PR_CAPBSET_DROP, i, 0, 0, 0);
			if (ret != 0)
				return ret;
		}
	}

	// effective, permitted, inheritable
	for (i = 0; i < _KERNEL_CAPABILITY_U32S; i++) {
		effective.cap[i] = caps_data[i];
		permitted.cap[i] = caps_data[2 + i];
		inheritable.cap[i] = caps_data[4 + i];
	}

	new = prepare_creds();
	if (!new)
		return -ENOMEM;

	ret = cap_capset(new, current_cred(), &effective, &inheritable, &permitted);
	if (ret != 0)
		return ret;
	commit_creds(new);

	// ambient
	for (i = 0; i <= CAP_LAST_CAP; i++) {
		action = PR_CAP_AMBIENT_LOWER;
		j = i;
		if (j > 31)
			j %= 32;
		if ((1 << (unsigned int)j) & caps_ambient[i / 32])
			action = PR_CAP_AMBIENT_RAISE;
		ret = cap_task_prctl(PR_CAP_AMBIENT, action, i, 0, 0);
		if (ret != 0)
			return ret;
	}

	return 0;
}
