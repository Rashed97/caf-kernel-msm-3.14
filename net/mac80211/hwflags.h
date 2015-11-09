/*
 * Copyright 2015	Intel Deutschland GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __mac80211_hwflags_h
#define __mac80211_hwflags_h
#include <linux/jump_label.h>
#include <net/mac80211.h>

extern struct static_key_false hwflags_keys[NUM_IEEE80211_HW_FLAGS];

#ifdef CONFIG_JUMP_LABEL
#define _HWFLAGS_DEFSTATE(_name, _on, _off)				\
	HWFLAGS_DEFSTATE_##_name = -1 + ((_on) ^ (_off)) * (1 + _on)
#define HWFLAGS_DEFSTATE(_name)						\
	_HWFLAGS_DEFSTATE(_name,					\
			  IS_ENABLED(CONFIG_MAC80211_HW_##_name##_ON),	\
			  IS_ENABLED(CONFIG_MAC80211_HW_##_name##_OFF))

enum hwflags_defstates {
HWFLAGS_DEFSTATE(HAS_RATE_CONTROL),
HWFLAGS_DEFSTATE(RX_INCLUDES_FCS),
HWFLAGS_DEFSTATE(HOST_BROADCAST_PS_BUFFERING),
HWFLAGS_DEFSTATE(SIGNAL_UNSPEC),
HWFLAGS_DEFSTATE(SIGNAL_DBM),
HWFLAGS_DEFSTATE(NEED_DTIM_BEFORE_ASSOC),
HWFLAGS_DEFSTATE(SPECTRUM_MGMT),
HWFLAGS_DEFSTATE(AMPDU_AGGREGATION),
HWFLAGS_DEFSTATE(SUPPORTS_PS),
HWFLAGS_DEFSTATE(PS_NULLFUNC_STACK),
HWFLAGS_DEFSTATE(SUPPORTS_DYNAMIC_PS),
HWFLAGS_DEFSTATE(MFP_CAPABLE),
HWFLAGS_DEFSTATE(WANT_MONITOR_VIF),
HWFLAGS_DEFSTATE(NO_AUTO_VIF),
HWFLAGS_DEFSTATE(SW_CRYPTO_CONTROL),
HWFLAGS_DEFSTATE(SUPPORT_FAST_XMIT),
HWFLAGS_DEFSTATE(REPORTS_TX_ACK_STATUS),
HWFLAGS_DEFSTATE(CONNECTION_MONITOR),
HWFLAGS_DEFSTATE(QUEUE_CONTROL),
HWFLAGS_DEFSTATE(SUPPORTS_PER_STA_GTK),
HWFLAGS_DEFSTATE(AP_LINK_PS),
HWFLAGS_DEFSTATE(TX_AMPDU_SETUP_IN_HW),
HWFLAGS_DEFSTATE(SUPPORTS_RC_TABLE),
HWFLAGS_DEFSTATE(P2P_DEV_ADDR_FOR_INTF),
HWFLAGS_DEFSTATE(TIMING_BEACON_ONLY),
HWFLAGS_DEFSTATE(SUPPORTS_HT_CCK_RATES),
HWFLAGS_DEFSTATE(CHANCTX_STA_CSA),
HWFLAGS_DEFSTATE(SUPPORTS_CLONED_SKBS),
HWFLAGS_DEFSTATE(SINGLE_SCAN_ON_ALL_BANDS),
HWFLAGS_DEFSTATE(TDLS_WIDER_BW),
HWFLAGS_DEFSTATE(SUPPORTS_AMSDU_IN_AMPDU),
HWFLAGS_DEFSTATE(BEACON_TX_STATUS),
};

bool _____optimisation_missing(void);

#define ieee80211_local_check(local, flg)				\
({									\
	enum ieee80211_hw_flags flag = IEEE80211_HW_##flg;		\
	bool result;							\
									\
	if (HWFLAGS_DEFSTATE_##flg == -1)				\
		result = test_bit(flag, (local)->hw.flags);		\
	else if (HWFLAGS_DEFSTATE_##flg == 1)				\
		result = (!static_branch_unlikely(&hwflags_keys[flag]) ||\
			  test_bit(flag, (local)->hw.flags));		\
	else if (HWFLAGS_DEFSTATE_##flg == 0)				\
		result = (static_branch_unlikely(&hwflags_keys[flag]) &&\
			  test_bit(flag, (local)->hw.flags));		\
	else								\
		result = _____optimisation_missing();			\
									\
	result;								\
})

void ieee80211_hwflags_sync_add(unsigned long *flags);
void ieee80211_hwflags_sync_del(unsigned long *flags);
#else /* CONFIG_JUMP_LABEL */
#define ieee80211_local_check(local, flg)	\
	test_bit(IEEE80211_HW_##flg, local->hw.flags)

static inline void ieee80211_hwflags_sync_add(unsigned long *flags) {}
static inline void ieee80211_hwflags_sync_del(unsigned long *flags) {}
#endif /* CONFIG_JUMP_LABEL */

#endif /* __mac80211_hwflags_h */
