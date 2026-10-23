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

/* path.c - build and insert Path header */
#include "path.h"
#include "../../core/data_lump.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include <stdio.h>

int pcscf_build_path_uri(str *uri, str *out, char *buf, int buf_len)
{
	int len;

	if(!uri || !uri->s || !out || !buf)
		return -1;
	len = snprintf(buf, buf_len, "%.*s;lr", uri->len, uri->s);
	if(len <= 0 || len >= buf_len)
		return -1;
	out->s = buf;
	out->len = len;
	return 0;
}

int pcscf_format_route_header(str *path, char *buf, int buf_len)
{
	int len;

	if(!path || !path->s || !buf || buf_len <= 0)
		return -1;
	len = snprintf(buf, buf_len, "Route: <%.*s>\r\n", path->len, path->s);
	if(len <= 0 || len >= buf_len)
		return -1;
	return len;
}

int pcscf_insert_path_on_register(struct sip_msg *msg, str *path_uri)
{
	char *hdr;
	int hdr_len;
	int need;
	struct lump *anchor;

	if(!msg || !path_uri || !path_uri->s)
		return -1;
	need = path_uri->len + 32;
	hdr = (char *)pkg_malloc(need);
	if(!hdr) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	hdr_len =
			snprintf(hdr, need, "Path: <%.*s>\r\n", path_uri->len, path_uri->s);
	if(hdr_len <= 0 || hdr_len >= need) {
		pkg_free(hdr);
		return -1;
	}
	anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0, HDR_PATH_T);
	if(!anchor) {
		pkg_free(hdr);
		return -1;
	}
	if(!insert_new_lump_before(anchor, hdr, hdr_len, HDR_PATH_T)) {
		pkg_free(hdr);
		return -1;
	}
	/* lump now owns hdr and will pkg_free it */
	return 0;
}
