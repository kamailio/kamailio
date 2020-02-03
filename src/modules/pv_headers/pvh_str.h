/*
 * PV Headers
 *
 * Copyright (C) 2018 Kirill Solomko <ksolomko@sipwise.com>
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef PV_STR_H
#define PV_STR_H

#include "../../core/str.h"

#include "pv_headers.h"

int pvh_str_new(str *s, int size);
int pvh_str_free(str *s);
int pvh_str_copy(str *dst, str *src, unsigned int max_size);
int pvh_extract_display_uri(char *suri, str *display, str *duri);
int pvh_split_values(
		str *s, char d[][header_value_size], int *d_size, int keep_spaces);

#endif /* PV_STR_H */