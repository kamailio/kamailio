/*
 * $Id$
 *
 * Registrar module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */
/*
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-21  save_noreply added, provided by Maxim Sobolev <sobomax@sippysoft.com> (janakj)
 *  2006-02-07  named flag support (andrei)
 */

#include <stdio.h>
#include "../../sr_module.h"
#include "../../timer.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../usr_avp.h"
#include "../../trim.h"
#include "save.h"
#include "lookup.h"
#include "reply.h"
#include "reg_mod.h"


MODULE_VERSION


static int mod_init(void);                           /* Module init function */
static int fix_save_nat_flag( modparam_t type, void* val);
static int fix_load_nat_flag( modparam_t type, void* val);
static int fix_trust_received_flag( modparam_t type, void* val);
static int domain_fixup(void** param, int param_no); /* Fixup that converts domain name */
static int domain2_fixup(void** param, int param_no); /* Fixup that converts domain name */
static void mod_destroy(void);

usrloc_api_t ul;            /* Structure containing pointers to usrloc functions */

int default_expires = 3600;           /* Default expires value in seconds */
qvalue_t default_q  = Q_UNSPECIFIED;  /* Default q value multiplied by 1000 */
int append_branches = 1;              /* If set to 1, lookup will put all contacts found in msg structure */
int case_sensitive  = 0;              /* If set to 1, username in aor will be case sensitive */
int save_nat_flag   = 4;              /* The contact will be marked as behind NAT if this flag is set before calling save */
int load_nat_flag   = 4;              /* This flag will be set by lookup if a contact is behind NAT*/
int trust_received_flag = -2;         /* if this flag is set (>=0), a contact
										 received param. will be trusted
										 (otherwise it will be ignored)
										 -1 = disable
										 -2 = trust all.
									   */
int min_expires     = 60;             /* Minimum expires the phones are allowed to use in seconds,
			               * use 0 to switch expires checking off */
int max_expires     = 0;              /* Minimum expires the phones are allowed to use in seconds,
			               * use 0 to switch expires checking off */
int max_contacts = 0;                 /* Maximum number of contacts per AOR */

str rcv_param = STR_STATIC_INIT("received");
int received_to_uri = 0;  /* copy received to uri, don't add it to dst_uri */

/* Attribute names */
str reply_code_attr = STR_STATIC_INIT("$code");
str reply_reason_attr = STR_STATIC_INIT("$reason");
str contact_attr = STR_STATIC_INIT("$contact");
str aor_attr = STR_STATIC_INIT("$aor");
str server_id_attr = STR_STATIC_INIT("$server_id");

avp_ident_t avpid_code, avpid_reason, avpid_contact;


/*
 * sl module api
 */
sl_api_t slb;


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"save_contacts",         save,         1, domain_fixup,  REQUEST_ROUTE | FAILURE_ROUTE },
	{"save_contacts",         save,         2, domain2_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"save",                  save,         1, domain_fixup,  REQUEST_ROUTE | FAILURE_ROUTE },
	{"save",                  save,         2, domain2_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"save_contacts_noreply", save_noreply, 1, domain_fixup,  REQUEST_ROUTE | FAILURE_ROUTE },
	{"save_contacts_noreply", save_noreply, 2, domain2_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"save_noreply",          save_noreply, 1, domain_fixup,  REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"save_noreply",          save_noreply, 2, domain2_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{"save_memory",           save_memory,  1, domain_fixup,  REQUEST_ROUTE | FAILURE_ROUTE },
	{"save_memory",           save_memory,  2, domain2_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"save_mem_nr",           save_mem_nr,  1, domain_fixup,  REQUEST_ROUTE | FAILURE_ROUTE },
	{"save_mem_nr",           save_mem_nr,  2, domain2_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"lookup_contacts",       lookup,       1, domain_fixup,  REQUEST_ROUTE | FAILURE_ROUTE },
	{"lookup_contacts",       lookup2,      2, domain2_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"lookup",                lookup,       1, domain_fixup,  REQUEST_ROUTE | FAILURE_ROUTE },
	{"lookup",                lookup2,      2, domain2_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{"registered",            registered,   1, domain_fixup,  REQUEST_ROUTE | FAILURE_ROUTE },
	{"registered",            registered2,  2, domain2_fixup, REQUEST_ROUTE | FAILURE_ROUTE },
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_expires",   PARAM_INT, &default_expires},
	{"default_q",         PARAM_INT, &default_q      },
	{"append_branches",   PARAM_INT, &append_branches},
	{"save_nat_flag",     PARAM_INT, &save_nat_flag  },
	{"save_nat_flag",     PARAM_STRING|PARAM_USE_FUNC, fix_save_nat_flag},

	{"load_nat_flag",     PARAM_INT, &load_nat_flag  },
	{"load_nat_flag",     PARAM_STRING|PARAM_USE_FUNC, fix_load_nat_flag},

	{"min_expires",       PARAM_INT, &min_expires    },
	{"max_expires",       PARAM_INT, &max_expires    },
	{"received_param",    PARAM_STR, &rcv_param      },
	{"max_contacts",      PARAM_INT, &max_contacts   },
	{"received_to_uri",   PARAM_INT, &received_to_uri},
	{"reply_code_attr",   PARAM_STR, &reply_code_attr},
	{"reply_reason_attr", PARAM_STR, &reply_reason_attr},
	{"contact_attr",      PARAM_STR, &contact_attr},
	{"aor_attr",          PARAM_STR, &aor_attr},
	{"server_id_attr",    PARAM_STR, &server_id_attr},
	{"trust_received_flag",  PARAM_INT, &trust_received_flag},
	{"trust_received_flag",  PARAM_STRING|PARAM_USE_FUNC,
										fix_trust_received_flag},
	{0, 0, 0}
};


/*
 * Module exports structure
 */
struct module_exports exports = {
	"registrar",
	cmds,        /* Exported functions */
	0,           /* RPC methods */
	params,      /* Exported parameters */
	mod_init,    /* module initialization function */
	0,
	mod_destroy, /* destroy function */
	0,           /* oncancel function */
	0            /* Per-child init function */
};


static int parse_attr_params(void)
{
	str tmp;

	if (reply_code_attr.len) {
		tmp = reply_code_attr;
		trim(&tmp);
		if (!tmp.len || tmp.s[0] != '$') {
			ERR("Invalid attribute name '%.*s'\n", tmp.len, tmp.s);
			return -1;
		}
		tmp.s++; tmp.len--;
		if (parse_avp_ident(&tmp, &avpid_code) < 0) {
			ERR("Error while parsing attribute name '%.*s'\n",
					tmp.len, tmp.s);
			return -1;
		}
	}

	if (reply_reason_attr.len) {
		tmp = reply_reason_attr;
		trim(&tmp);
		if (!tmp.len || tmp.s[0] != '$') {
			ERR("Invalid attribute name '%.*s'\n", tmp.len, tmp.s);
			return -1;
		}
		tmp.s++; tmp.len--;
		if (parse_avp_ident(&tmp, &avpid_reason) < 0) {
			ERR("Error while parsing attribute name '%.*s'\n",
					tmp.len, tmp.s);
			return -1;
		}
	}

	if (contact_attr.len) {
		tmp = contact_attr;
		trim(&tmp);
		if (!tmp.len || tmp.s[0] != '$') {
			ERR("Invalid attribute name '%.*s'\n", tmp.len, tmp.s);
			return -1;
		}
		tmp.s++; tmp.len--;
		if (parse_avp_ident(&tmp, &avpid_contact) < 0) {
			ERR("Error while parsing attribute name '%.*s'\n",
					tmp.len, tmp.s);
			return -1;
		}
	}
	return 0;
}


/*
 * Initialize parent
 */
static int mod_init(void)
{
	bind_usrloc_t bind_usrloc;

	DBG("registrar - initializing\n");

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
	if (!bind_usrloc) {
		ERR("Can't bind usrloc\n");
		return -1;
	}

	     /* Normalize default_q parameter */
	if (default_q != Q_UNSPECIFIED) {
		if (default_q > MAX_Q) {
			DBG("registrar: default_q = %d, lowering to MAX_Q: %d\n", default_q, MAX_Q);
			default_q = MAX_Q;
		} else if (default_q < MIN_Q) {
			DBG("registrar: default_q = %d, raising to MIN_Q: %d\n", default_q, MIN_Q);
			default_q = MIN_Q;
		}
	}

	if (parse_attr_params() < 0) return -1;

	if (bind_usrloc(&ul) < 0) {
		return -1;
	}

	return 0;
}



/* fixes nat_flag param (resolves possible named flags) */
static int fix_save_nat_flag( modparam_t type, void* val)
{
	return fix_flag(type, val, "registrar", "save_nat_flag", &save_nat_flag);
}


/* fixes nat_flag param (resolves possible named flags) */
static int fix_load_nat_flag( modparam_t type, void* val)
{
	return fix_flag(type, val, "registrar", "load_nat_flag", &load_nat_flag);
}

/* fixes trust_received_flag param (resolves possible named flags) */
static int fix_trust_received_flag( modparam_t type, void* val)
{
	return fix_flag(type, val, "registrar", "trust_received_flag",
					&trust_received_flag);
}


/*
 * Convert char* parameter to udomain_t* pointer
 */
static int domain_fixup(void** param, int param_no)
{
	udomain_t* d;

	if (param_no == 1) {
		if (ul.register_udomain((char*)*param, &d) < 0) {
			LOG(L_ERR, "domain_fixup(): Error while registering domain\n");
			return E_UNSPEC;
		}

		*param = (void*)d;
	}
	return 0;
}


/*
 * Convert char* parameter to udomain_t* pointer
 */
static int domain2_fixup(void** param, int param_no)
{
	if (param_no == 1) {
	    return domain_fixup(param, param_no);
	} else {
	    return fixup_var_str_12(param, 2);
	}
	return 0;
}


static void mod_destroy(void)
{
	free_contact_buf();
}
