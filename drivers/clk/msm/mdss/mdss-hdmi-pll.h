/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_HDMI_PLL_H
#define __MDSS_HDMI_PLL_H

struct hdmi_pll_cfg {
	unsigned long vco_rate;
	u32 reg;
};

struct hdmi_pll_vco_clk {
	unsigned long	rate;	/* current vco rate */
	unsigned long	min_rate;	/* min vco rate */
	unsigned long	max_rate;	/* max vco rate */
	bool		rate_set;
	struct hdmi_pll_cfg *ip_seti;
	struct hdmi_pll_cfg *cp_seti;
	struct hdmi_pll_cfg *ip_setp;
	struct hdmi_pll_cfg *cp_setp;
	struct hdmi_pll_cfg *crctrl;
	void		*priv;

	struct clk	c;
};

int hdmi_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int hdmi_20nm_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int hdmi_8996_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res);
#endif
