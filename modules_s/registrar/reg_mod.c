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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-21  save_noreply added, provided by Maxim Sobolev <sobomax@portaone.com> (janakj)
 */

#include <stdio.h>
#include "../../sr_module.h"
#include "../../timer.h"
#include "../../dprint.h"
#include "../../error.h"
#include "save.h"
#include "lookup.h"
#include "reply.h"
#include "reg_mod.h"


MODULE_VERSION


static int mod_init(void);                           /* Module init function */
static int domain_fixup(void** param, int param_no); /* Fixup that converts domain name */
static void mod_destroy(void);

usrloc_api_t ul;            /* Structure containing pointers to usrloc functions */

int default_expires = 3600;           /* Default expires value in seconds */
qvalue_t default_q  = Q_UNSPECIFIED;  /* Default q value multiplied by 1000 */
int append_branches = 1;              /* If set to 1, lookup will put all contacts found in msg structure */
int case_sensitive  = 0;              /* If set to 1, username in aor will be case sensitive */
int desc_time_order = 0;              /* By default do not order according to the descending modification time */
int nat_flag        = 4;              /* SER flag marking contacts behind NAT */
int min_expires     = 60;             /* Minimum expires the phones are allowed to use in seconds,
			               * use 0 to switch expires checking off */
int max_expires     = 0;              /* Minimum expires the phones are allowed to use in seconds,
			               * use 0 to switch expires checking off */
int max_contacts = 0;                 /* Maximum number of contacts per AOR */
int retry_after = 0;                  /* The value of Retry-After HF in 5xx replies */

int use_domain = 0;
char* realm_pref    = "";   /* Realm prefix to be removed */
str realm_prefix;

#define RCV_NAME "received"
#define RCV_NAME_LEN (sizeof(RCV_NAME) - 1)

str rcv_param = {RCV_NAME, RCV_NAME_LEN};
int rcv_avp_no=42;


/*
 * sl_send_reply function pointer
 */
int (*sl_reply)(struct sip_msg* _m, char* _s1, char* _s2);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"save",         save,         1, domain_fixup, REQUEST_ROUTE                },
	{"save_noreply", save_noreply, 1, domain_fixup, REQUEST_ROUTE                },
	{"save_memory",  save_memory,  1, domain_fixup, REQUEST_ROUTE                },
	{"lookup",       lookup,       1, domain_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"registered",   registered,   1, domain_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_expires", INT_PARAM, &default_expires},
	{"default_q",       INT_PARAM, &default_q      },
	{"append_branches", INT_PARAM, &append_branches},
	{"case_sensitive",  INT_PARAM, &case_sensitive },
	{"desc_time_order", INT_PARAM, &desc_time_order},
	{"nat_flag",        INT_PARAM, &nat_flag       },
	{"realm_prefix",    STR_PARAM, &realm_pref     },
	{"min_expires",     INT_PARAM, &min_expires    },
	{"max_expires",     INT_PARAM, &max_expires    },
        {"received_param",  STR_PARAM, &rcv_param  },
	{"received_avp",    INT_PARAM, &rcv_avp_no     },
	{"use_domain",      INT_PARAM, &use_domain     },
	{"max_contacts",    INT_PARAM, &max_contacts   },
	{"retry_after",     INT_PARAM, &retry_after    },
	{0, 0, 0}
};


/*
 * Module exports structure
 */
struct module_exports exports = {
	"registrar", 
	cmds,        /* Exported functions */
	params,      /* Exported parameters */
	mod_init,    /* module initialization function */
	0,
	mod_destroy, /* destroy function */
	0,           /* oncancel function */
	0            /* Per-child init function */
};


/*
 * Initialize parent
 */
static int mod_init(void)
{
	bind_usrloc_t bind_usrloc;

	DBG("registrar - initializing\n");

             /*
              * We will need sl_send_reply from stateless
	      * module for sending replies
	      */
        sl_reply = find_export("sl_send_reply", 2, 0);
	if (!sl_reply) {
		LOG(L_ERR, "registrar: This module requires sl module\n");
		return -1;
	}

	realm_prefix.s = realm_pref;
	realm_prefix.len = strlen(realm_pref);
	
	rcv_param.len = strlen(rcv_param.s);

	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
	if (!bind_usrloc) {
		LOG(L_ERR, "registrar: Can't bind usrloc\n");
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
	

	if (bind_usrloc(&ul) < 0) {
		return -1;
	}

	return 0;
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


static void mod_destroy(void)
{
	free_contact_buf();
}
