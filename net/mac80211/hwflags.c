/*
 * Copyright 2015	Intel Deutschland GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/jump_label.h>
#include <net/mac80211.h>
#include "ieee80211_i.h"

struct static_key_false hwflags_keys[NUM_IEEE80211_HW_FLAGS] = {
	[0 ... NUM_IEEE80211_HW_FLAGS - 1] = STATIC_KEY_FALSE_INIT,
};

static s8 hwflags_defstate[] = {
#define DEFINE_HWFLAG(_flg)	\
	[IEEE80211_HW_##_flg] = HWFLAGS_DEFSTATE_##_flg,
#include <net/mac80211-hwflags.h>
#undef DEFINE_HWFLAG
};

void ieee80211_hw_mod_flag(struct ieee80211_hw *hw,
			   enum ieee80211_hw_flags flg, bool set)
{
	struct ieee80211_local *local = hw_to_local(hw);

	if (set) {
		if (test_bit(flg, hw->flags))
			return;
		__set_bit(flg, hw->flags);
	} else {
		if (!test_bit(flg, hw->flags))
			return;
		__clear_bit(flg, hw->flags);
	}

	if (!local->registered)
		return;

	if (hwflags_defstate[flg] < 0)
		return;

	if (hwflags_defstate[flg] == !!test_bit(flg, hw->flags))
		static_branch_dec(&hwflags_keys[flg]);
	else
		static_branch_inc(&hwflags_keys[flg]);
}
EXPORT_SYMBOL_GPL(ieee80211_hw_mod_flag);

void ieee80211_hwflags_sync_add(unsigned long *flags)
{
	unsigned int flg;

	for (flg = 0; flg < NUM_IEEE80211_HW_FLAGS; flg++) {
		if (hwflags_defstate[flg] != !!test_bit(flg, flags))
			static_branch_inc(&hwflags_keys[flg]);
	}
}

void ieee80211_hwflags_sync_del(unsigned long *flags)
{
	unsigned int flg;

	for (flg = 0; flg < NUM_IEEE80211_HW_FLAGS; flg++) {
		if (hwflags_defstate[flg] != !!test_bit(flg, flags))
			static_branch_dec(&hwflags_keys[flg]);
	}
}

void ieee80211_test_hwflags(void)
{
	struct {
		struct {
			unsigned long flags[2];
		} hw;
	} _local = {};

	__set_bit(IEEE80211_HW_HAS_RATE_CONTROL, _local.hw.flags);
	__clear_bit(IEEE80211_HW_SUPPORT_FAST_XMIT, _local.hw.flags);

	/* before the sync_add(), we expect only as per Kconfig */
	if (ieee80211_local_check(&_local, HAS_RATE_CONTROL))
		printk(KERN_DEBUG "BAD: HW rate control\n");
	if (!ieee80211_local_check(&_local, SUPPORT_FAST_XMIT))
		printk(KERN_DEBUG "BAD: !SUPPORT_FAST_XMIT\n");

	ieee80211_hwflags_sync_add(_local.hw.flags);
	printk(KERN_DEBUG "added\n");

	/* now it should be like as per flags */
	if (!ieee80211_local_check(&_local, HAS_RATE_CONTROL))
		printk(KERN_DEBUG "BAD: !HW rate control\n");
	if (ieee80211_local_check(&_local, SUPPORT_FAST_XMIT))
		printk(KERN_DEBUG "BAD: SUPPORT_FAST_XMIT\n");

	ieee80211_hwflags_sync_del(_local.hw.flags);
	printk(KERN_DEBUG "removed\n");

	/* after remove it should be as before */
	if (ieee80211_local_check(&_local, HAS_RATE_CONTROL))
		printk(KERN_DEBUG "BAD: HW rate control\n");
	if (!ieee80211_local_check(&_local, SUPPORT_FAST_XMIT))
		printk(KERN_DEBUG "BAD: !SUPPORT_FAST_XMIT\n");
}
