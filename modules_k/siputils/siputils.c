/*
 * $Id$
 *
 * Copyright (C) 2008-2009 1&1 Internet AG
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file
 * \brief Module with several utiltity functions related to SIP messages handling
 * \ingroup siputils
 * - Module; \ref siputils
 */

/*!
 * \defgroup siputils UTILS :: Various SIP message handling functions
 *
 * \note A Kamailio module (modules_k)
 *
   This module implement various functions and checks related to
   SIP message handling and URI handling.

   It offers some functions related to handle ringing. In a
   parallel forking scenario you get several 183s with SDP. You
   don't want that your customers hear more than one ringtone or
   answer machine in parallel on the phone. So its necessary to
   drop the 183 in this cases and send a 180 instead.

   This module provides a function to answer OPTIONS requests
   which are directed to the server itself. This means an OPTIONS
   request which has the address of the server in the request
   URI, and no username in the URI. The request will be answered
   with a 200 OK which the capabilities of the server.

   To answer OPTIONS request directed to your server is the
   easiest way for is-alive-tests on the SIP (application) layer
   from remote (similar to ICMP echo requests, also known as
   "ping", on the network layer).

 */

#include <assert.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../script_cb.h"
#include "../../locking.h"
#include "../../ut.h"
#include "../../mod_fix.h"
#include "../../error.h"

#include "ring.h"
#include "options.h"

#include "checks.h"

#include "utils.h"
#include "contact_ops.h"
#include "sipops.h"

MODULE_VERSION

gen_lock_t *ring_lock = NULL;
unsigned int ring_timeout = 0;
/* for options functionality */
str opt_accept = str_init(ACPT_DEF);
str opt_accept_enc = str_init(ACPT_ENC_DEF);
str opt_accept_lang = str_init(ACPT_LAN_DEF);
str opt_supported = str_init(SUPT_DEF);
/** SL binds */
struct sl_binds opt_slb;

static int mod_init(void);
static void mod_destroy(void);

char *contact_flds_separator = DEFAULT_SEPARATOR;

static cmd_export_t cmds[]={
	{"ring_insert_callid", (cmd_function)ring_insert_callid, 0, ring_fixup, 0, REQUEST_ROUTE|FAILURE_ROUTE},
	{"options_reply",      (cmd_function)opt_reply, 0, 0, 0, REQUEST_ROUTE},
	{"is_user",            (cmd_function)is_user,        1, fixup_str_null, 0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"has_totag", 	       (cmd_function)has_totag,      0, 0, 0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"uri_param",          (cmd_function)uri_param_1,    1, fixup_str_null, 0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"uri_param",          (cmd_function)uri_param_2,    2, fixup_str_str, 0, REQUEST_ROUTE|LOCAL_ROUTE},
	{"add_uri_param",      (cmd_function)add_uri_param,  1, fixup_str_null, 0, REQUEST_ROUTE},
	{"tel2sip",            (cmd_function)tel2sip,        0, 0,         0, REQUEST_ROUTE},
	{"is_uri_user_e164",   (cmd_function)is_uri_user_e164, 1, fixup_pvar_null, fixup_free_pvar_null, REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"encode_contact",     (cmd_function)encode_contact,2,0, 0, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"decode_contact",     (cmd_function)decode_contact,0,0, 0, REQUEST_ROUTE},
	{"decode_contact_header", (cmd_function)decode_contact_header,0,0,0,REQUEST_ROUTE|ONREPLY_ROUTE},
	{"cmp_uri",  (cmd_function)w_cmp_uri, 2,
		fixup_spve_spve, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"cmp_aor",  (cmd_function)w_cmp_aor, 2,
		fixup_spve_spve, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[] = {
	{"ring_timeout",            INT_PARAM, &ring_timeout},
	{"options_accept",          STR_PARAM, &opt_accept.s},
	{"options_accept_encoding", STR_PARAM, &opt_accept_enc.s},
	{"options_accept_language", STR_PARAM, &opt_accept_lang.s},
	{"options_support",         STR_PARAM, &opt_supported.s},
	{"contact_flds_separator",  STR_PARAM,&contact_flds_separator},
	{0, 0, 0}
};


struct module_exports exports= {
	"siputils",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported functions */
	params,          /* param exports */
	0,               /* exported statistics */
	0,               /* exported MI functions */
	0,               /* exported pseudo-variables */
	0,               /* extra processes */
	mod_init,        /* initialization function */
	0,               /* Response function */
	mod_destroy,     /* Destroy function */
	0,               /* Child init function */
};


static int mod_init(void)
{
	if(ring_timeout > 0) {
		ring_init_hashtable();

		ring_lock = lock_alloc();
		assert(ring_lock);
		if (lock_init(ring_lock) == 0) {
			LM_CRIT("cannot initialize lock.\n");
			return -1;
		}
		if (register_script_cb(ring_filter, PRE_SCRIPT_CB|ONREPLY_CB, 0) != 0) {
			LM_ERR("could not insert callback");
			return -1;
		}
	}

		/* load the SL API */
	if (load_sl_api(&opt_slb)!=0) {
		LM_ERR("can't load SL API\n");
		return -1;
	}

	opt_accept.len = strlen(opt_accept.s);
	opt_accept_enc.len = strlen(opt_accept_enc.s);
	opt_accept_lang.len = strlen(opt_accept_lang.s);
	opt_supported.len = strlen(opt_supported.s);

	return 0;
}


static void mod_destroy(void)
{
	if (ring_lock) {
		lock_destroy(ring_lock);
		lock_dealloc((void *)ring_lock);
		ring_lock = NULL;
	}

	ring_destroy_hashtable();
}
