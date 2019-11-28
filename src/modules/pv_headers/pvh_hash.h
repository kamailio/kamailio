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

#ifndef PVH_HASH_H
#define PVH_HASH_H

#include "../../core/str_hash.h"

extern struct str_hash_table skip_headers;
extern struct str_hash_table split_headers;
extern struct str_hash_table single_headers;

int pvh_str_hash_init(struct str_hash_table *ht, str *keys, char *desc);
int pvh_str_hash_add_key(struct str_hash_table *ht, str *key);
int pvh_str_hash_free(struct str_hash_table *ht);
int pvh_skip_header(str *hname);
int pvh_single_header(str *hname);

#endif /* PVH_HASH_H */
