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

/* Serialize/parse the "<impu1><impu2>..." list format used for pcontact IMPU columns */
#ifndef PCONTACT_SERIALIZE_H
#define PCONTACT_SERIALIZE_H
#include "../../core/str.h"

int pcscf_serialize_impus(str *impus, int n, str *out, int out_size);
int pcscf_serialize_impus_barred(str *barred, int n, str *out, int out_size);
int pcscf_parse_impus(str *in, str *parsed, int max);
int pcscf_apply_barred_flags(
		str *impus, int n, str *barred, int n_barred, char *flags);

#endif
