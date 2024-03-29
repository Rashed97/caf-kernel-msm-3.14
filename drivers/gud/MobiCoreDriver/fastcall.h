/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _TBASE_FASTCALL_H_
#define _TBASE_FASTCALL_H_

#include "mci/mcimcp.h" /* struct mcp_buffer */
#include "platform.h"

/* Use the arch_extension sec pseudo op before switching to secure world */
#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#ifndef CONFIG_ARM64
#define MC_ARCH_EXTENSION_SEC
#endif
#endif

bool mc_fastcall(void *data);
int mc_fc_init(struct mcp_buffer *mcp_buffer, phys_addr_t mci_base_pa,
	       void *mci_base, size_t queue_sz);
int mc_fc_info(uint32_t ext_info_id, uint32_t *state, uint32_t *ext_info);
int mc_fc_mem_trace(phys_addr_t buffer, uint32_t size);
int mc_fc_nsiq(void);
int mc_fc_yield(void);

#ifdef MC_FASTCALL_WORKER_THREAD
int mc_fastcall_init(void);
void mc_fastcall_cleanup(void);
#else
static inline int mc_fastcall_init(void)
{
	return 0;
}

static inline void mc_fastcall_cleanup(void)
{
}
#endif

#endif /* _TBASE_FASTCALL_H_ */
