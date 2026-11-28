/*
 * Cx operations on the in-memory S-CSCF registration cache.
 *
 * Written by Abdul Karim Barbour <a(dot)barbour(at)telekom(punkt)de>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef IMS_CX_PROFILE_H
#define IMS_CX_PROFILE_H
#include "udomain.h"

/* 0 success, -1 unknown private identity, -2 busy/resource failure,
 * -3 unsupported scope/backend. Caller retains its reference to profile. */
int cx_replace_profile(udomain_t *d, ims_subscription *profile);
int cx_update_profile(udomain_t *d, str *private_id, ims_subscription *profile,
		cx_charging_t *charging, cx_notify_contact_f notify);
/* RTR: 0 success, 1 limited success, 2 all public identities protected;
 * negative results have the same meaning as profile operations above. */
int cx_deregister(udomain_t *d, str *private_ids, int private_count,
		str *public_ids, int count, int reason, unsigned int flags,
		cx_notify_contact_f notify, cx_dereg_result_t *response);
#endif
