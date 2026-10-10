/* Cx operations on the in-memory S-CSCF registration cache.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef IMS_CX_PROFILE_H
#define IMS_CX_PROFILE_H
#include "udomain.h"

/* 0 success, -1 unknown/mismatched identity, -2 busy/resource failure,
 * -3 unsupported scope/backend. Caller retains its reference to profile. */
int cx_replace_profile(udomain_t *d, ims_subscription *profile);
int cx_deregister(udomain_t *d, str *private_id, str *public_ids, int count,
		int reason, cx_notify_contact_f notify);
#endif
