/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 * Copyright (C) 2025-2026 Michael Furmur (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _VL_PARAM_MAP_PARSER_H_
#define _VL_PARAM_MAP_PARSER_H_

#include "../../core/str.h"

/**
 * callback invoked by parse_log_mapping():
 *  - once with name==NULL when a new group key is found
 *  - once per "name:type" pair inside the group
 */
typedef int (*parser_cb)(
		str *key, str *name, int type, str *stype, void *usrdata);

/**
 * parse mapping string of format "key(name:type[,name:type...])"
 */
int parse_log_mapping(char *val, parser_cb cb, void *userdata);

#endif
