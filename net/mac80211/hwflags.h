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
enum hwflags_defstates {
#define __DEFINE_HWFLAG(_flg, _on, _off)				\
	HWFLAGS_DEFSTATE_##_flg = -1 + ((_on) ^ (_off)) * (1 + _on),
#define DEFINE_HWFLAG(_flg)						\
	__DEFINE_HWFLAG(_flg,						\
			IS_ENABLED(CONFIG_MAC80211_HW_##_flg##_ON),	\
			IS_ENABLED(CONFIG_MAC80211_HW_##_flg##_OFF))
#include <net/mac80211-hwflags.h>
#undef DEFINE_HWFLAG
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

void ieee80211_test_hwflags(void);
#else /* CONFIG_JUMP_LABEL */
#define ieee80211_local_check(local, flg)	\
	test_bit(IEEE80211_HW_##flg, local->hw.flags)

static inline void ieee80211_hwflags_sync_add(unsigned long *flags) {}
static inline void ieee80211_hwflags_sync_del(unsigned long *flags) {}

static inline void ieee80211_test_hwflags(void) {}
#endif /* CONFIG_JUMP_LABEL */

#endif /* __mac80211_hwflags_h */
