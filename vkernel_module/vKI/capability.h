// SPDX-License-Identifier: GPL-2.0

#ifndef _VKERNEL_CAPABILITY_H
#define _VKERNEL_CAPABILITY_H

int task_set_capability(struct task_struct *target, unsigned long *caps_data,
        unsigned long *caps_bounding, unsigned long *caps_ambient);

#endif /* _VKERNEL_CAPABILITY_H */
