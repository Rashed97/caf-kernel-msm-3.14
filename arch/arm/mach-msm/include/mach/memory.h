/* arch/arm/mach-msm/include/mach/memory.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
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
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H
#include <linux/types.h>

/* physical offset of RAM */
#define PLAT_PHYS_OFFSET UL(CONFIG_PHYS_OFFSET)

#ifndef __ASSEMBLY__

#ifdef CONFIG_CACHE_L2X0
extern void l2x0_cache_sync(void);
#define finish_arch_switch(prev)     do { l2x0_cache_sync(); } while (0)
#endif

#define MAX_HOLE_ADDRESS    (PHYS_OFFSET + 0x10000000)

#endif

#if defined CONFIG_ARCH_MSM_SCORPION || defined CONFIG_ARCH_MSM_KRAIT
#define arch_has_speculative_dfetch()	1
#endif

#endif

#define CONSISTENT_DMA_SIZE	(SZ_1M * 14)
