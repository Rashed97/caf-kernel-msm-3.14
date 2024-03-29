/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __WCD9XXX_COMMON_V2_H__
#define __WCD9XXX_COMMON_V2_H__

#include <linux/mfd/wcd9xxx/core-resource.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>

enum wcd_clock_type {
	WCD_CLK_OFF,
	WCD_CLK_RCO,
	WCD_CLK_MCLK,
};

struct wcd9xxx_resmgr_v2 {
	struct snd_soc_codec *codec;
	struct wcd9xxx_core_resource *core_res;

	int master_bias_users;
	int clk_mclk_users;
	int clk_rco_users;

	struct mutex codec_bg_clk_lock;

	enum wcd_clock_type clk_type;
};

#define WCD9XXX_V2_BG_CLK_LOCK(resmgr)			\
{							\
	struct wcd9xxx_resmgr_v2 *__resmgr = resmgr;	\
	pr_debug("%s: Acquiring BG_CLK\n", __func__);	\
	mutex_lock(&__resmgr->codec_bg_clk_lock);	\
	pr_debug("%s: Acquiring BG_CLK done\n", __func__);	\
}

#define WCD9XXX_V2_BG_CLK_UNLOCK(resmgr)			\
{							\
	struct wcd9xxx_resmgr_v2 *__resmgr = resmgr;	\
	pr_debug("%s: Releasing BG_CLK\n", __func__);	\
	mutex_unlock(&__resmgr->codec_bg_clk_lock);	\
}

#define WCD9XXX_V2_BG_CLK_ASSERT_LOCKED(resmgr)		\
{							\
	WARN_ONCE(!mutex_is_locked(&resmgr->codec_bg_clk_lock), \
		  "%s: BG_CLK lock should have acquired\n", __func__); \
}

int wcd_resmgr_enable_master_bias(struct wcd9xxx_resmgr_v2 *resmgr);
int wcd_resmgr_disable_master_bias(struct wcd9xxx_resmgr_v2 *resmgr);
struct wcd9xxx_resmgr_v2 *wcd_resmgr_init(
		struct wcd9xxx_core_resource *core_res,
		struct snd_soc_codec *codec);
void wcd_resmgr_remove(struct wcd9xxx_resmgr_v2 *resmgr);
int wcd_resmgr_post_init(struct wcd9xxx_resmgr_v2 *resmgr,
			 struct snd_soc_codec *codec);
int wcd_resmgr_enable_clk_block(struct wcd9xxx_resmgr_v2 *resmgr,
				enum wcd_clock_type type);
int wcd_resmgr_disable_clk_block(struct wcd9xxx_resmgr_v2 *resmgr,
				enum wcd_clock_type type);
#endif
