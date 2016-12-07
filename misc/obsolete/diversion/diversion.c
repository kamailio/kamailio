/*
 * $Id$
 *
 * Diversion Header Field Support
 *
 * Copyright (C) 2004 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"


MODULE_VERSION

#define DIVERSION_HF "Diversion"
#define DIVERSION_HF_LEN (sizeof(DIVERSION_HF) - 1)

#define DIVERSION_PREFIX     DIVERSION_HF ": <"
#define DIVERSION_PREFIX_LEN (sizeof(DIVERSION_PREFIX) - 1)

#define DIVERSION_SUFFIX     ">;reason="
#define DIVERSION_SUFFIX_LEN (sizeof(DIVERSION_SUFFIX) - 1)



str suffix = STR_STATIC_INIT("");

int add_diversion(struct sip_msg* msg, char* r, char* s);


/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"add_diversion",    add_diversion,    1, fixup_var_str_1, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"suffix", PARAM_STR, &suffix},
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"diversion",
	cmds,       /* Exported functions */
	0,          /* RPC methods */
	params,     /* Exported parameters */
        mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	0,          /* oncancel function */
	0           /* child initialization function */
};


static int mod_init(void)
{
	return 0;
}


static inline int add_diversion_helper(struct sip_msg* msg, str* s)
{
	char *ptr;

	static struct lump* anchor = 0;
	static int msg_id = 0;

	if (msg_id != msg->id) {
		msg_id = msg->id;
		anchor = 0;
	}

	if (!msg->diversion && parse_headers(msg, HDR_DIVERSION_F, 0) == -1) {
		LOG(L_ERR, "add_diversion_helper: Header parsing failed\n");
		return -1;
	}

	if (msg->diversion) {
		     /* Insert just before the topmost Diversion header */
		ptr = msg->diversion->name.s;
	} else {
		     /* Insert at the end */
		ptr = msg->unparsed;
	}

	if (!anchor) {
		anchor = anchor_lump(msg, ptr - msg->buf, 0, 0);
		if (!anchor) {
			LOG(L_ERR, "add_diversion_helper: Can't get anchor\n");
			return -2;
		}
	}

	if (!insert_new_lump_before(anchor, s->s, s->len, 0)) {
		LOG(L_ERR, "add_diversion_helper: Can't insert lump\n");
		return -3;
	}

	return 0;
}


int add_diversion(struct sip_msg* msg, char* r, char* s)
{
	str div_hf;
	char *at;
	str* uri;
	str reason;

	if (get_str_fparam(&reason, msg, (fparam_t*)r) < 0) return -1;

	uri = &msg->first_line.u.request.uri;

	div_hf.len = DIVERSION_PREFIX_LEN + uri->len + DIVERSION_SUFFIX_LEN + reason.len + CRLF_LEN;
	div_hf.s = pkg_malloc(div_hf.len);
	if (!div_hf.s) {
		LOG(L_ERR, "add_diversion: No memory left\n");
		return -1;
	}

	at = div_hf.s;
	memcpy(at, DIVERSION_PREFIX, DIVERSION_PREFIX_LEN);
	at += DIVERSION_PREFIX_LEN;

	memcpy(at, uri->s, uri->len);
	at += uri->len;

	memcpy(at, DIVERSION_SUFFIX, DIVERSION_SUFFIX_LEN);
	at += DIVERSION_SUFFIX_LEN;

	memcpy(at, reason.s, reason.len);
	at += reason.len;

	memcpy(at, CRLF, CRLF_LEN);

	if (add_diversion_helper(msg, &div_hf) < 0) {
		pkg_free(div_hf.s);
		return -1;
	}

	return 1;
}
