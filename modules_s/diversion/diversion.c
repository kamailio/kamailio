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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#define DIVERSION_PREFIX     DIVERSION_HF ": >"
#define DIVERSION_PREFIX_LEN (sizeof(DIVERSION_PREFIX) - 1)

#define DIVERSION_SUFFIX     ">;reason="
#define DIVERSION_SUFFIX_LEN (sizeof(DIVERSION_SUFFIX) - 1)



str suffix = {"", 0};

int add_diversion(struct sip_msg* msg, char* r, char* s);
static int str_fixup(void** param, int param_no);


/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"add_diversion",    add_diversion,    1, str_fixup, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"suffix", STR_PARAM, &suffix.s},
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"diversion", 
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
        mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	0,          /* oncancel function */
	0           /* child initialization function */
};


static int mod_init(void)
{
	suffix.len = strlen(suffix.s);
	return 0;
}


/*
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
	str* s;

	if (param_no == 1 || param_no == 2) {
		s = (str*)pkg_malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "str_fixup: No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}


static inline int add_diversion_helper(struct sip_msg* msg, str* s)
{
	char *str, *ptr;

	static struct lump* anchor = 0;
	static int msg_id = 0;

	if (msg_id != msg->id) {
		msg_id = msg->id;
		anchor = 0;
	}
	
	if (!msg->diversion && parse_headers(msg, HDR_DIVERSION, 0) == -1) {
		LOG(L_ERR, "add_diversion_helper: Header parsing failed\n");
		return -1;
	}
	
	if (msg->diversion) {
		     /* Insert just before the topmost Diversio header */
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
	
	str = pkg_malloc(s->len);
	if (!str) {
		LOG(L_ERR, "add_diversion_helper: No memory left\n");
	}
	
	memcpy(str, s->s, s->len);
	if (!insert_new_lump_before(anchor, str, s->len, 0)) {
		LOG(L_ERR, "add_diversion_helper: Can't insert lump\n");
		pkg_free(str);
		return -3;
	}

	return 0;
}


int add_diversion(struct sip_msg* msg, char* r, char* s)
{
	str div_hf;
	char *at;
	str* uri;
	str* reason;

	reason = (str*)r;

	uri = &msg->first_line.u.request.uri;

	div_hf.len = DIVERSION_PREFIX_LEN + uri->len + DIVERSION_SUFFIX_LEN + reason->len + CRLF_LEN;
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

	memcpy(at, reason->s, reason->len);
	at += reason->len;

	memcpy(at, CRLF, CRLF_LEN);

	add_diversion_helper(msg, &div_hf);
	pkg_free(div_hf.s);
	return 1;
}
