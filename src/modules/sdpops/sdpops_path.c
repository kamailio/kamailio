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


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../core/dprint.h"

#include "sdpops_path.h"

/**
 *
 */
int sdpops_path_parse(str *vpath, sdp_path_t *rpath)
{
	int n;
	int i;
	if(vpath == NULL || vpath->s == NULL || vpath->len <= 0 || rpath == NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}
	if(vpath->s[0] != '/') {
		LM_ERR("sdp path must start with '/'\n");
		return -1;
	}
	if(vpath->len <= 1 || vpath->len >= SDPOPS_SDPPATH_SIZE) {
		LM_ERR("sdp path is too short or long [%.*s] (%d)\n", vpath->len,
				vpath->s, vpath->len);
		return -1;
	}
	memset(rpath, 0, sizeof(sdp_path_t));
	memcpy(rpath->spbuf, vpath->s, vpath->len);
	rpath->spbuf[vpath->len] = '\0';
	rpath->spath.s = rpath->spbuf;
	rpath->spath.len = vpath->len;

	n = 0;
	i = 1;
	while(i < rpath->spath.len) {
		rpath->ilist[n].name.s = rpath->spath.s + 1;
		while((i < rpath->spath.len) && (rpath->spath.s[i] != '[')
				&& (rpath->spath.s[i] != '/')) {
			i++;
		}
		rpath->ilist[n].name.len = rpath->spath.s + i - rpath->ilist[n].name.s;
		if(rpath->ilist[n].name.len <= 0) {
			LM_ERR("invalid field in sdp path [%.*s] (n: %d)\n", vpath->len,
					vpath->s, n);
			return -1;
		}
		if(rpath->ilist[n].name.s[0] == '@') {
			rpath->ilist[n].itype = SDPOPS_SDPTYPE_FATTR;
		}
		if(i == rpath->spath.len) {
			goto done;
		}
		if(rpath->spath.s[i] == '[') {
			/* index */
			if(i > rpath->spath.len - 2) {
				LM_ERR("invalid sdp path [%.*s] (n: %d)\n", vpath->len,
						vpath->s, n);
				return -1;
			}
			i++;
			while((i < rpath->spath.len) && (rpath->spath.s[i] != ']')) {
				if(rpath->spath.s[i] >= '0' && rpath->spath.s[i] <= '9') {
					rpath->ilist[n].index = rpath->ilist[n].index * 10
											+ (rpath->spath.s[i] - '0');
				} else {
					LM_ERR("invalid index insdp path [%.*s] (n: %d)\n",
							vpath->len, vpath->s, n);
					return -1;
				}
				i++;
			}

		} else {
			/* path item */
			if(n < SDPOPS_SDPPATH_DEPTH - 1) {
				n++;
			} else {
				LM_ERR("overflowing sdp path depth: %d\n", n);
				return -1;
			}
		}
		i++;
	}

done:
	if(rpath->ilist[n].name.s != NULL) {
		rpath->icount = n + 1;
	} else {
		rpath->icount = n;
	}
	return 0;
}

/**
 *
 */
int sdpops_path_debug(sdp_path_t *rpath)
{
	int i;
	if(rpath == NULL) {
		return -1;
	}
	for(i = 0; i < rpath->icount; i++) {
		LM_DBG("item[%d]: name='%.*s' index=%d type=%d\n", i,
				rpath->ilist[i].name.len, rpath->ilist[i].name.s,
				rpath->ilist[i].index, rpath->ilist[i].itype);
	}
	return 0;
}
