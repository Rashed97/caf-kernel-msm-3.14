/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_H
#define __ARCH_ARM_MACH_MSM_CLOCK_H

#include <linux/clkdev.h>

/**
 * struct clock_init_data - SoC specific clock initialization data
 * @table: table of lookups to add
 * @size: size of @table
 * @pre_init: called before initializing the clock driver.
 * @post_init: called after registering @table. clock APIs can be called inside.
 * @late_init: called during late init
 */
struct clock_init_data {
	struct list_head list;
	struct clk_lookup *table;
	size_t size;
	void (*pre_init)(void);
	void (*post_init)(void);
	int (*late_init)(void);
};

extern int __init msm_gcc_8084_init(void);

int msm_clock_init(struct clock_init_data *data);
int find_vdd_level(struct clk *clk, unsigned long rate);

#ifdef CONFIG_DEBUG_FS
int clock_debug_register(struct clk_lookup *t, size_t s);
void clock_debug_print_enabled(void);
#else
static inline int clock_debug_register(struct clk_lookup *t, size_t s)
{
	return 0;
}
static inline void clock_debug_print_enabled(void) { return; }
#endif

#endif
