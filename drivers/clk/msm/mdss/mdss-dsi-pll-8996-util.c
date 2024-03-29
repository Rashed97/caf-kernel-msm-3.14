/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/clk/msm-clock-generic.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"
#include "mdss-dsi-pll-8996.h"

#define DSI_PLL_POLL_MAX_READS                  15
#define DSI_PLL_POLL_TIMEOUT_US                 1000

int set_mdss_byte_mux_sel_8996(struct mux_clk *clk, int sel)
{
	return 0;
}

int get_mdss_byte_mux_sel_8996(struct mux_clk *clk)
{
	return 0;
}

int set_mdss_pixel_mux_sel_8996(struct mux_clk *clk, int sel)
{
	return 0;
}

int get_mdss_pixel_mux_sel_8996(struct mux_clk *clk)
{
	return 0;
}

int post_n1_div_set_div(struct div_clk *clk, int div)
{
	struct mdss_pll_resources *pll = clk->priv;
	struct dsi_pll_db *pdb;
	struct dsi_pll_output *pout;

	pdb = (struct dsi_pll_db *)pll->priv;
	pout = &pdb->out;

	/*
	 * vco rate = bit_clk * postdiv * n1div
	 * vco range from 1300 to 2600 Mhz
	 * postdiv = 1
	 * n1div = 1 to 15
	 * n1div = roundup(1300Mhz / bit_clk)
	 * support bit_clk above 86.67Mhz
	 */

	/* this is for vco/bit clock */
	pout->pll_postdiv = 1;	/* fixed, divided by 1 */
	pout->pll_n1div  = div;

	pr_debug("div=%d postdiv=%x n1div=%x\n",
			div, pout->pll_postdiv, pout->pll_n1div);

	/* registers commited at pll_db_commit_8996() */

	return 0;
}

int post_n1_div_get_div(struct div_clk *clk)
{
	u32  div;
	struct mdss_pll_resources *pll = clk->priv;
	struct dsi_pll_db *pdb;
	struct dsi_pll_output *pout;

	pdb = (struct dsi_pll_db *)pll->priv;
	pout = &pdb->out;

	/*
	 * postdiv = 1/2/4/8
	 * n1div = 1 - 15
	 * fot the time being, assume postdiv = 1
	 */

	div = pout->pll_postdiv * pout->pll_n1div;

	pr_debug("div=%d postdiv=%x n1div=%x\n",
			div, pout->pll_postdiv, pout->pll_n1div);

	return div;
}

int n2_div_set_div(struct div_clk *clk, int div)
{
	int rc;
	u32 n2div;
	struct mdss_pll_resources *pll = clk->priv;
	struct dsi_pll_db *pdb;
	struct dsi_pll_output *pout;
	struct mdss_pll_resources *slave;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	pdb = (struct dsi_pll_db *)pll->priv;
	pout = &pdb->out;

	/* this is for pixel clock */
	n2div = MDSS_PLL_REG_R(pll->pll_base, DSIPHY_CMN_CLK_CFG0);
	n2div &= ~0xf0;	/* bits 4 to 7 */
	n2div |= (div << 4);
	MDSS_PLL_REG_W(pll->pll_base, DSIPHY_CMN_CLK_CFG0, n2div);

	/* commit slave if split display is enabled */
	slave = pll->slave;
	if (slave)
		MDSS_PLL_REG_W(slave->pll_base, DSIPHY_CMN_CLK_CFG0, n2div);

	pout->pll_n2div = div;

	/* set dsiclk_sel=1 so that n2div *= 2 */
	MDSS_PLL_REG_W(pll->pll_base, DSIPHY_CMN_CLK_CFG1, 1);
	pr_debug("div=%d n2div=%x\n", div, n2div);

	mdss_pll_resource_enable(pll, false);

	return rc;
}

int n2_div_get_div(struct div_clk *clk)
{
	int rc;
	u32 n2div;
	struct mdss_pll_resources *pll = clk->priv;

	if (is_gdsc_disabled(pll))
		return 0;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	n2div = MDSS_PLL_REG_R(pll->pll_base, DSIPHY_CMN_CLK_CFG0);
	n2div >>= 4;
	n2div &= 0x0f;

	mdss_pll_resource_enable(pll, false);

	pr_debug("div=%d\n", n2div);

	return n2div;
}

static bool pll_is_pll_locked_8996(struct mdss_pll_resources *pll)
{
	u32 status;
	bool pll_locked;

	/* poll for PLL ready status */
	if (readl_poll_timeout_atomic((pll->pll_base +
			DSIPHY_PLL_RESET_SM_READY_STATUS),
			status,
			((status & BIT(5)) > 0),
			DSI_PLL_POLL_MAX_READS,
			DSI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("DSI PLL status=%x failed to Lock\n", status);
		pll_locked = false;
	} else if (readl_poll_timeout_atomic((pll->pll_base +
				DSIPHY_PLL_RESET_SM_READY_STATUS),
				status,
				((status & BIT(0)) > 0),
				DSI_PLL_POLL_MAX_READS,
				DSI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("DSI PLL status=%x PLl not ready\n", status);
		pll_locked = false;
	} else {
		pll_locked = true;
	}

	return pll_locked;
}

static void dsi_pll_start_8996(void __iomem *pll_base)
{
	pr_debug("start PLL at base=%p\n", pll_base);

	MDSS_PLL_REG_W(pll_base, DSIPHY_CMN_PLL_CNTRL, 1);
}

static void dsi_pll_stop_8996(void __iomem *pll_base)
{
	pr_debug("stop PLL at base=%p\n", pll_base);

	MDSS_PLL_REG_W(pll_base, DSIPHY_CMN_PLL_CNTRL, 0);
}

int dsi_pll_enable_seq_8996(struct mdss_pll_resources *pll)
{
	int rc = 0;

	if (!pll) {
		pr_err("Invalid PLL resources\n");
		return -EINVAL;
	}

	dsi_pll_start_8996(pll->pll_base);

	/*
	 * both DSIPHY_PLL_CLKBUFLR_EN and DSIPHY_CMN_GLBL_TEST_CTRL
	 * enabled at mdss_dsi_8996_phy_config()
	 */

	if (!pll_is_pll_locked_8996(pll)) {
		pr_err("DSI PLL lock failed\n");
		rc = -EINVAL;
		goto init_lock_err;
	}

	pr_debug("DSI PLL Lock success\n");

init_lock_err:
	return rc;
}

static int dsi_pll_enable(struct clk *c)
{
	int i, rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	/* Try all enable sequences until one succeeds */
	for (i = 0; i < vco->pll_en_seq_cnt; i++) {
		rc = vco->pll_enable_seqs[i](pll);
		pr_debug("DSI PLL %s after sequence #%d\n",
			rc ? "unlocked" : "locked", i + 1);
		if (!rc)
			break;
	}

	if (rc) {
		mdss_pll_resource_enable(pll, false);
		pr_err("DSI PLL failed to lock\n");
	} else {
		pll->pll_on = true;
	}

	return rc;
}

static void dsi_pll_disable(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	if (!pll->pll_on &&
		mdss_pll_resource_enable(pll, true)) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return;
	}

	pll->handoff_resources = false;

	dsi_pll_stop_8996(pll->pll_base);

	/* stop pll output */
	MDSS_PLL_REG_W(pll->pll_base, DSIPHY_PLL_CLKBUFLR_EN, 0);

	/* stop clk */
	MDSS_PLL_REG_W(pll->pll_base, DSIPHY_CMN_GLBL_TEST_CTRL, 0);

	mdss_pll_resource_enable(pll, false);

	pll->pll_on = false;

	pr_debug("DSI PLL Disabled\n");
	return;
}

static void mdss_dsi_pll_8996_input_init(struct dsi_pll_db *pdb)
{
	pdb->in.fref = 19200000;	/* 19.2 Mhz*/
	pdb->in.fdata = 0;		/* bit clock rate */
	pdb->in.dsiclk_sel = 1;		/* 1, reg: 0x0014 */
	pdb->in.ssc_en = 0;		/* 1, reg: 0x0494, bit 0 */
	pdb->in.ldo_en = 0;		/* 0,  reg: 0x004c, bit 0 */

	/* fixed  input */
	pdb->in.refclk_dbler_en = 0;	/* 0, reg: 0x04c0, bit 1 */
	pdb->in.vco_measure_time = 5;	/* 5, unknown */
	pdb->in.kvco_measure_time = 5;	/* 5, unknown */
	pdb->in.bandgap_timer = 4;	/* 4, reg: 0x0430, bit 3 - 5 */
	pdb->in.pll_wakeup_timer = 5;	/* 5, reg: 0x043c, bit 0 - 2 */
	pdb->in.plllock_cnt = 1;	/* 1, reg: 0x0488, bit 1 - 2 */
	pdb->in.plllock_rng = 0;	/* 0, reg: 0x0488, bit 3 - 4 */
	pdb->in.ssc_center_spread = 0;	/* 0, reg: 0x0494, bit 1 */
	pdb->in.ssc_adj_per = 37;	/* 37, reg: 0x498, bit 0 - 9 */
	pdb->in.ssc_spread = 5;		/* 0.005, 5kppm */
	pdb->in.ssc_freq = 31500;	/* 31.5 khz */

	pdb->in.pll_ie_trim = 4;	/* 4, reg: 0x0400 */
	pdb->in.pll_ip_trim = 4;	/* 4, reg: 0x0404 */
	pdb->in.pll_cpcset_cur = 1;	/* 1, reg: 0x04f0, bit 0 - 2 */
	pdb->in.pll_cpmset_cur = 1;	/* 1, reg: 0x04f0, bit 3 - 5 */
	pdb->in.pll_icpmset = 4;	/* 4, reg: 0x04fc, bit 3 - 5 */
	pdb->in.pll_icpcset = 4;	/* 4, reg: 0x04fc, bit 0 - 2 */
	pdb->in.pll_icpmset_p = 0;	/* 0, reg: 0x04f4, bit 0 - 2 */
	pdb->in.pll_icpmset_m = 0;	/* 0, reg: 0x04f4, bit 3 - 5 */
	pdb->in.pll_icpcset_p = 0;	/* 0, reg: 0x04f8, bit 0 - 2 */
	pdb->in.pll_icpcset_m = 0;	/* 0, reg: 0x04f8, bit 3 - 5 */
	pdb->in.pll_lpf_res1 = 3;	/* 3, reg: 0x0504, bit 0 - 3 */
	pdb->in.pll_lpf_cap1 = 11;	/* 11, reg: 0x0500, bit 0 - 3 */
	pdb->in.pll_lpf_cap2 = 14;	/* 14, reg: 0x0500, bit 4 - 7 */
	pdb->in.pll_iptat_trim = 7;
	pdb->in.pll_c3ctrl = 2;		/* 2 */
	pdb->in.pll_r3ctrl = 1;		/* 1 */
}

static void pll_8996_dec_frac_calc(struct dsi_pll_db *pdb,
			 s64 vco_clk_rate, s64 fref)
{
	struct dsi_pll_input *pin = &pdb->in;
	struct dsi_pll_output *pout = &pdb->out;
	s64 multiplier = BIT(20);
	s64 dec_start_multiple, dec_start, pll_comp_val;
	s32 duration, div_frac_start;

	pr_debug("vco_clk_rate=%lld ref_clk_rate=%lld\n",
				vco_clk_rate, fref);

	dec_start_multiple = div_s64(vco_clk_rate * multiplier, fref);
	div_s64_rem(vco_clk_rate * multiplier, fref, &div_frac_start);

	dec_start = div_s64(dec_start_multiple, multiplier);

	pout->dec_start = (u32)dec_start;
	pout->div_frac_start = div_frac_start;

	if (pin->plllock_cnt == 0)
		duration = 1024;
	else if (pin->plllock_cnt == 1)
		duration = 256;
	else if (pin->plllock_cnt == 2)
		duration = 128;
	else
		duration = 32;

	pll_comp_val =  duration * dec_start_multiple;
	pll_comp_val =  div_s64(pll_comp_val, multiplier);
	pll_comp_val /= 10;

	pout->plllock_cmp = (u32)pll_comp_val;

	pout->pll_txclk_en = 1;
	pout->cmn_ldo_cntrl = 0x1c;
}

static u32 pll_8996_kvco_slop(u32 vrate)
{
	u32 slop = 0;

	if (vrate > 1300000000 && vrate <= 1800000000)
		slop =  600;
	else if (vrate > 1800000000 && vrate < 2300000000)
		slop = 400;
	else if (vrate > 2300000000 && vrate < 2600000000)
		slop = 280;

	return slop;
}

static void pll_8996_calc_vco_count(struct dsi_pll_db *pdb,
			 s64 vco_clk_rate, s64 fref)
{
	struct dsi_pll_input *pin = &pdb->in;
	struct dsi_pll_output *pout = &pdb->out;
	s64 data;
	u32 cnt;

	data = fref * pin->vco_measure_time;
	data /= 1000000;
	data &= 0x03ff;	/* 10 bits */
	data -= 2;
	pout->pll_vco_div_ref = data;

	data = vco_clk_rate / 1000000;	/* unit is Mhz */
	data *= pin->vco_measure_time;
	data /= 10;
	pout->pll_vco_count = data; /* reg: 0x0474, 0x0478 */

	data = fref * pin->kvco_measure_time;
	data /= 1000000;
	data &= 0x03ff;	/* 10 bits */
	data -= 1;
	pout->pll_kvco_div_ref = data;

	cnt = pll_8996_kvco_slop(vco_clk_rate);
	cnt *= 2;
	cnt /= 100;
	cnt *= pin->kvco_measure_time;
	pout->pll_kvco_count = cnt;

	pout->pll_misc1 = 16;
	pout->pll_resetsm_cntrl = 0;
	pout->pll_resetsm_cntrl2 = pin->bandgap_timer << 3;
	pout->pll_resetsm_cntrl5 = pin->pll_wakeup_timer;
	pout->pll_kvco_code = 0;
}

static void pll_db_commit_common(void __iomem *pll_base,
					struct dsi_pll_db *pdb)
{
	struct dsi_pll_input *pin = &pdb->in;
	struct dsi_pll_output *pout = &pdb->out;
	char data;

	/* confgiure the non frequency dependent pll registers */
	data = 0;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_SYSCLK_EN_RESET, data);

	/* DSIPHY_PLL_CLKBUFLR_EN updated at dsi phy */

	data = pout->pll_txclk_en;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_TXCLK_EN, data);

	data = pout->pll_resetsm_cntrl;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_RESETSM_CNTRL, data);
	data = pout->pll_resetsm_cntrl2;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_RESETSM_CNTRL2, data);
	data = pout->pll_resetsm_cntrl5;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_RESETSM_CNTRL5, data);

	data = pout->pll_vco_div_ref;
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_VCO_DIV_REF1, data);
	data = (pout->pll_vco_div_ref >> 8);
	data &= 0x03;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_VCO_DIV_REF2, data);

	data = pout->pll_kvco_div_ref;
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_KVCO_DIV_REF1, data);
	data = (pout->pll_kvco_div_ref >> 8);
	data &= 0x03;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_KVCO_DIV_REF2, data);

	data = pout->pll_misc1;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLL_MISC1, data);

	data = pin->pll_ie_trim;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_IE_TRIM, data);

	data = pin->pll_ip_trim;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_IP_TRIM, data);

	data = ((pin->pll_cpmset_cur << 3) | pin->pll_cpcset_cur);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_CP_SET_CUR, data);

	data = ((pin->pll_icpcset_p << 3) | pin->pll_icpcset_m);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLL_ICPCSET, data);

	data = ((pin->pll_icpmset_p << 3) | pin->pll_icpcset_m);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLL_ICPMSET, data);

	data = ((pin->pll_icpmset << 3) | pin->pll_icpcset);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLL_ICP_SET, data);

	data = ((pdb->in.pll_lpf_cap2 << 4) | pdb->in.pll_lpf_cap1);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLL_LPF1, data);

	data = pin->pll_iptat_trim;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_IPTAT_TRIM, data);

	data = (pdb->in.pll_c3ctrl | (pdb->in.pll_r3ctrl << 4));
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLL_CRCTRL, data);
}

static void pll_db_commit_8996(void __iomem *pll_base,
					struct dsi_pll_db *pdb)
{
	struct dsi_pll_input *pin = &pdb->in;
	struct dsi_pll_output *pout = &pdb->out;
	char data;

	data = pout->cmn_ldo_cntrl;
	MDSS_PLL_REG_W(pll_base, DSIPHY_CMN_LDO_CNTRL, data);

	pll_db_commit_common(pll_base, pdb);

	/* de assert pll start and apply pll sw reset */
	/* stop pll */
	MDSS_PLL_REG_W(pll_base, DSIPHY_CMN_PLL_CNTRL, 0);

	/* pll sw reset */
	MDSS_PLL_REG_W(pll_base, DSIPHY_CMN_CTRL_1, 0x20);
	wmb();	/* make sure register committed */
	udelay(10);

	MDSS_PLL_REG_W(pll_base, DSIPHY_CMN_CTRL_1, 0);
	wmb();	/* make sure register committed */

	data = pdb->in.dsiclk_sel; /* set dsiclk_sel = 1  */
	MDSS_PLL_REG_W(pll_base, DSIPHY_CMN_CLK_CFG1, data);

	data = 0xff; /* data, clk, pll normal operation */
	MDSS_PLL_REG_W(pll_base, DSIPHY_CMN_CTRL_0, data);

	/* confgiure the frequency dependent pll registers */
	data = pout->dec_start;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_DEC_START, data);

	data = pout->div_frac_start;
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_DIV_FRAC_START1, data);
	data = (pout->div_frac_start >> 8);
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_DIV_FRAC_START2, data);
	data = (pout->div_frac_start >> 16);
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_DIV_FRAC_START3, data);

	data = pout->plllock_cmp;
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLLLOCK_CMP1, data);
	data = (pout->plllock_cmp >> 8);
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLLLOCK_CMP2, data);
	data = (pout->plllock_cmp >> 16);
	data &= 0x03;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLLLOCK_CMP3, data);

	data = ((pin->plllock_cnt << 1) | (pin->plllock_rng << 3));
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLLLOCK_CMP_EN, data);

	data = pout->pll_vco_count;
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_VCO_COUNT1, data);
	data = (pout->pll_vco_count >> 8);
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_VCO_COUNT2, data);

	data = pout->pll_kvco_count;
	data &= 0x0ff;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_KVCO_COUNT1, data);
	data = (pout->pll_kvco_count >> 8);
	data &= 0x03;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_KVCO_COUNT2, data);

	/*
	 * tx_band = pll_postdiv
	 * 0: divided by 1 <== for now
	 * 1: divided by 2
	 * 2: divided by 4
	 * 3: divided by 8
	 */
	data = (((pout->pll_postdiv - 1) << 4) | pdb->in.pll_lpf_res1);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PLL_LPF2_POSTDIV, data);

	data = (pout->pll_n1div | (pout->pll_n2div << 4));
	MDSS_PLL_REG_W(pll_base, DSIPHY_CMN_CLK_CFG0, data);

	wmb();	/* make sure register committed */
}

int pll_vco_set_rate_8996(struct clk *c, unsigned long rate)
{
	int rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;
	struct mdss_pll_resources *slave;
	struct dsi_pll_db *pdb;

	pdb = (struct dsi_pll_db *)pll->priv;
	if (!pdb) {
		pr_err("No prov found\n");
		return -EINVAL;
	}

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	pr_debug("%s: rate=%lu\n", __func__, rate);

	pll->vco_current_rate = rate;
	pll->vco_ref_clk_rate = vco->ref_clk_rate;

	mdss_dsi_pll_8996_input_init(pdb);

	pll_8996_dec_frac_calc(pdb, pll->vco_current_rate,
					pll->vco_ref_clk_rate);

	pll_8996_calc_vco_count(pdb, pll->vco_current_rate,
					pll->vco_ref_clk_rate);

	/* commit slave if split display is enabled */
	slave = pll->slave;
	if (slave)
		pll_db_commit_8996(slave->pll_base, pdb);

	/* commit master itself */
	pll_db_commit_8996(pll->pll_base, pdb);

	mdss_pll_resource_enable(pll, false);

	return rc;
}

unsigned long pll_vco_get_rate_8996(struct clk *c)
{
	u64 vco_rate, multiplier = (1 << 20);
	s32 div_frac_start;
	u32 dec_start;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	u64 ref_clk = vco->ref_clk_rate;
	int rc;
	struct mdss_pll_resources *pll = vco->priv;

	if (is_gdsc_disabled(pll))
		return 0;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	dec_start = MDSS_PLL_REG_R(pll->pll_base,
			DSIPHY_PLL_DEC_START);
	dec_start &= 0x0ff;
	pr_debug("dec_start = 0x%x\n", dec_start);

	div_frac_start = (MDSS_PLL_REG_R(pll->pll_base,
			DSIPHY_PLL_DIV_FRAC_START3) & 0x0ff) << 16;
	div_frac_start |= (MDSS_PLL_REG_R(pll->pll_base,
			DSIPHY_PLL_DIV_FRAC_START2) & 0x0ff) << 8;
	div_frac_start |= MDSS_PLL_REG_R(pll->pll_base,
			DSIPHY_PLL_DIV_FRAC_START1) & 0x0ff;
	pr_debug("div_frac_start = 0x%x\n", div_frac_start);

	vco_rate = ref_clk * 2 * dec_start;
	vco_rate += ((ref_clk * div_frac_start) / multiplier);

	pr_debug("returning vco rate = %lu\n", (unsigned long)vco_rate);

	mdss_pll_resource_enable(pll, false);

	return (unsigned long)vco_rate;
}

long pll_vco_round_rate_8996(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;
	u32 div;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);

	div = vco->min_rate / rate;
	if (div > 15) {
		/* rate < 86.67 Mhz */
		pr_err("rate=%lu NOT supportted\n", rate);
		return -EINVAL;
	}

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	return rrate;
}

enum handoff pll_vco_handoff_8996(struct clk *c)
{
	int rc;
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	if (is_gdsc_disabled(pll))
		return HANDOFF_DISABLED_CLK;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return ret;
	}

	if (pll_is_pll_locked_8996(pll)) {
		pll->handoff_resources = true;
		pll->pll_on = true;
		c->rate = pll_vco_get_rate_8996(c);
		ret = HANDOFF_ENABLED_CLK;
	} else {
		mdss_pll_resource_enable(pll, false);
	}

	return ret;
}

int pll_vco_prepare_8996(struct clk *c)
{
	int rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	if (!pll) {
		pr_err("Dsi pll resources are not available\n");
		return -EINVAL;
	}

	if ((pll->vco_cached_rate != 0)
	    && (pll->vco_cached_rate == c->rate)) {
		rc = c->ops->set_rate(c, pll->vco_cached_rate);
		if (rc) {
			pr_err("vco_set_rate failed. rc=%d\n", rc);
			goto error;
		}
	}

	rc = dsi_pll_enable(c);

error:
	return rc;
}

void pll_vco_unprepare_8996(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	if (!pll) {
		pr_err("Dsi pll resources are not available\n");
		return;
	}

	pll->vco_cached_rate = c->rate;
	dsi_pll_disable(c);
}
