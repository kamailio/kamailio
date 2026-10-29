/* Cx operations on the in-memory S-CSCF registration cache.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * As a special exception, the copyright holders of the new contributions
 * permit linking those contributions with the OpenSSL library and
 * distributing the resulting combined work. The GNU General Public
 * License applies to all other code.
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
