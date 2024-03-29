/* Copyright (c) 2002,2007-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define ANY_ID (~0)

static const struct adreno_gpu_core adreno_gpulist[] = {
	{
		.gpurev = ADRENO_REV_A306,
		.core = 3,
		.major = 0,
		.minor = 6,
		.patchid = 0x00,
		.pm4fw_name = "a300_pm4.fw",
		.pfpfw_name = "a300_pfp.fw",
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = SZ_128K,
	},
	{
		.gpurev = ADRENO_REV_A304,
		.core = 3,
		.major = 0,
		.minor = 4,
		.patchid = 0x00,
		.pm4fw_name = "a300_pm4.fw",
		.pfpfw_name = "a300_pfp.fw",
		.gpudev = &adreno_a3xx_gpudev,
		.gmem_size = (SZ_64K + SZ_32K),
	},
	{
		.gpurev = ADRENO_REV_A405,
		.core = 4,
		.major = 0,
		.minor = 5,
		.patchid = ANY_ID,
		.features = 0,
		.pm4fw_name = "a420_pm4.fw",
		.pfpfw_name = "a420_pfp.fw",
		.gpudev = &adreno_a4xx_gpudev,
		.gmem_size = SZ_256K,
	},
	{
		.gpurev = ADRENO_REV_A420,
		.core = 4,
		.major = 2,
		.minor = 0,
		.patchid = ANY_ID,
		.features = ADRENO_USES_OCMEM | ADRENO_WARM_START |
					ADRENO_USE_BOOTSTRAP,
		.pm4fw_name = "a420_pm4.fw",
		.pfpfw_name = "a420_pfp.fw",
		.gpudev = &adreno_a4xx_gpudev,
		.gmem_size = (SZ_1M + SZ_512K),
		.pm4_jt_idx = 0x901,
		.pm4_jt_addr = 0x300,
		.pfp_jt_idx = 0x401,
		.pfp_jt_addr = 0x400,
		.pm4_bstrp_size = 0x06,
		.pfp_bstrp_size = 0x28,
		.pfp_bstrp_ver = 0x4ff083,
	},
	{
		.gpurev = ADRENO_REV_A430,
		.core = 4,
		.major = 3,
		.minor = 0,
		.patchid = ANY_ID,
		.features = ADRENO_USES_OCMEM  | ADRENO_WARM_START |
			ADRENO_USE_BOOTSTRAP | ADRENO_SPTP_PC | ADRENO_PPD |
			ADRENO_CONTENT_PROTECTION | ADRENO_PREEMPTION,
		.pm4fw_name = "a420_pm4.fw",
		.pfpfw_name = "a420_pfp.fw",
		.gpudev = &adreno_a4xx_gpudev,
		.gmem_size = (SZ_1M + SZ_512K),
		.pm4_jt_idx = 0x901,
		.pm4_jt_addr = 0x300,
		.pfp_jt_idx = 0x401,
		.pfp_jt_addr = 0x400,
		.pm4_bstrp_size = 0x06,
		.pfp_bstrp_size = 0x28,
		.pfp_bstrp_ver = 0x4ff083,
		.shader_offset = 0x20000,
		.shader_size = 0x10000,
		.num_protected_regs = 0x18,
	},
	{
		.gpurev = ADRENO_REV_A418,
		.core = 4,
		.major = 1,
		.minor = 8,
		.patchid = ANY_ID,
		.features = ADRENO_USES_OCMEM  | ADRENO_WARM_START |
			ADRENO_USE_BOOTSTRAP | ADRENO_SPTP_PC,
		.pm4fw_name = "a420_pm4.fw",
		.pfpfw_name = "a420_pfp.fw",
		.gpudev = &adreno_a4xx_gpudev,
		.gmem_size = (SZ_512K),
		.pm4_jt_idx = 0x901,
		.pm4_jt_addr = 0x300,
		.pfp_jt_idx = 0x401,
		.pfp_jt_addr = 0x400,
		.pm4_bstrp_size = 0x06,
		.pfp_bstrp_size = 0x28,
		.pfp_bstrp_ver = 0x4ff083,
		.shader_offset = 0x20000, /* SP and TP addresses */
		.shader_size = 0x10000,
		.num_protected_regs = 0x18,
	},
	{
		.gpurev = ADRENO_REV_A530,
		.core = 5,
		.major = 3,
		.minor = 0,
		.patchid = 0,
		.pm4fw_name = "a530v1_pm4.fw",
		.pfpfw_name = "a530v1_pfp.fw",
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_size = SZ_1M,
		.num_protected_regs = 0x20,
	},
	{
		.gpurev = ADRENO_REV_A530,
		.core = 5,
		.major = 3,
		.minor = 0,
		.patchid = ANY_ID,
		.pm4fw_name = "a530_pm4.fw",
		.pfpfw_name = "a530_pfp.fw",
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_size = SZ_1M,
		.num_protected_regs = 0x20,
	},
	{
		.gpurev = ADRENO_REV_A510,
		.core = 5,
		.major = 1,
		.minor = 0,
		.patchid = ANY_ID,
		.pm4fw_name = "a530_pm4.fw",
		.pfpfw_name = "a530_pfp.fw",
		.gpudev = &adreno_a5xx_gpudev,
		.gmem_size = SZ_256K,
		.num_protected_regs = 0x20,
	},
};
