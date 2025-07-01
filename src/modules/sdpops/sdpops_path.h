/*
 * Copyright (C) 2024 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
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


#ifndef _SDPOPS_PATH_H_
#define _SDPOPS_PATH_H_

#include "../../core/str.h"
#include "../../core/parser/sdp/sdp.h"

#define SDPOPS_SDPPATH_SIZE 256
#define SDPOPS_SDPPATH_DEPTH 4

#define SDPOPS_SDPTYPE_FIELD 0
#define SDPOPS_SDPTYPE_FATTR 1

/* clang-format off */
typedef struct sdp_path_item {
	str name;
	int index;
	int itype;
} sdp_path_item_t;

typedef struct sdp_path {
	char spbuf[SDPOPS_SDPPATH_SIZE];
	str spath;
	int icount;
	sdp_path_item_t ilist[SDPOPS_SDPPATH_DEPTH];
} sdp_path_t;
/* clang-format on */

int sdpops_path_parse(str *vpath, sdp_path_t *rpath);
int sdpops_path_debug(sdp_path_t *rpath);

#endif
