/*
 * Copyright (C) 2026 toharishs@gmail.com
 *
 * The initial version of this code is written by Harish S
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/* Matches an AOR/IMPU pair, including SIP/tel user-part comparison */
#ifndef IMPU_MATCH_H
#define IMPU_MATCH_H
#include "../../core/str.h"
#include "usrloc.h"

int pcscf_impu_matches_aor(str *aor, str *impu);
int pcscf_contact_has_impu(pcontact_t *c, str *aor);
int is_impu_barred(pcontact_t *c, str *impu);
#endif
