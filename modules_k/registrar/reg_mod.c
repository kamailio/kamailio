/* 
 * $Id$
 *
 * Registrar module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-21  save_noreply added, provided by Maxim Sobolev
 *              <sobomax@portaone.com> (janakj)
 *  2005-07-11  added sip_natping_flag for nat pinging with SIP method
 *              instead of UDP package (bogdan)
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
static int str_fixup(void** param, int param_no);
static int add_sock_hdr(struct sip_msg* msg, char *str, char *foo);
static void mod_destroy(void);

/* Structure containing pointers to usrloc functions */
usrloc_api_t ul;

/* Default expires value in seconds */
int default_expires = 3600;
/* Default q value multiplied by 1000 */
qvalue_t default_q  = Q_UNSPECIFIED;
/* If set to 1, lookup will put all contacts found in msg structure */
int append_branches = 1;
/* If set to 1, username in aor will be case sensitive */
int case_sensitive  = 0;
/* By default do not order according to the descending modification time */
int desc_time_order = 0;
/* flag marking contacts behind NAT */
int nat_flag        = -1;
/* flag marking nated contacts to be pinged with SIP method  */
int sip_natping_flag  = -1;
/* Minimum expires the phones are allowed to use in seconds
 * use 0 to switch expires checking off */
int min_expires     = 60;
/* Minimum expires the phones are allowed to use in seconds,
 * use 0 to switch expires checking off */
int max_expires     = 0;
/* Maximum number of contacts per AOR */
int max_contacts = 0;
/* The value of Retry-After HF in 5xx replies */
int retry_after = 0;

int use_domain = 0;
char* realm_pref    = "";   /* Realm prefix to be removed */
str realm_prefix;

int sock_flag = -1;
str sock_hdr_name = {0,0};

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
	{"add_sock_hdr", add_sock_hdr, 1, str_fixup,    REQUEST_ROUTE                },
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_expires",   INT_PARAM, &default_expires   },
	{"default_q",         INT_PARAM, &default_q         },
	{"append_branches",   INT_PARAM, &append_branches   },
	{"case_sensitive",    INT_PARAM, &case_sensitive    },
	{"desc_time_order",   INT_PARAM, &desc_time_order   },
	{"nat_flag",          INT_PARAM, &nat_flag          },
	{"sip_natping_flag",  INT_PARAM, &sip_natping_flag  },
	{"realm_prefix",      STR_PARAM, &realm_pref        },
	{"min_expires",       INT_PARAM, &min_expires       },
	{"max_expires",       INT_PARAM, &max_expires       },
	{"received_param",    STR_PARAM, &rcv_param         },
	{"received_avp",      INT_PARAM, &rcv_avp_no        },
	{"use_domain",        INT_PARAM, &use_domain        },
	{"max_contacts",      INT_PARAM, &max_contacts      },
	{"retry_after",       INT_PARAM, &retry_after       },
	{"sock_flag",         INT_PARAM, &sock_flag         },
	{"sock_hdr_name",     STR_PARAM, &sock_hdr_name.s   },
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
			DBG("registrar: default_q = %d, lowering to MAX_Q: %d\n",
				default_q, MAX_Q);
			default_q = MAX_Q;
		} else if (default_q < MIN_Q) {
			DBG("registrar: default_q = %d, raising to MIN_Q: %d\n",
				default_q, MIN_Q);
			default_q = MIN_Q;
		}
	}
	

	if (bind_usrloc(&ul) < 0) {
		return -1;
	}

	/*
	 * Test if use_domain parameters of usrloc and registrar
	 * module are equal
	 */
	if (ul.use_domain != use_domain) {
		LOG(L_ERR, "ERROR: 'use_domain' parameters of 'usrloc' and "
			"'registrar' modules must have the same value !\n");
		LOG(L_ERR, "(Hint: Did you forget to use modparam(\"registrar\","
			" \"use_domain\", 1) in in your ser.cfg ?)\n");
		return -1;
	}

	if (sock_hdr_name.s) {
		sock_hdr_name.len = strlen(sock_hdr_name.s);
		if (sock_hdr_name.len==0 || sock_flag==-1) {
			LOG(L_WARN,"WARN:registrar:init: empty sock_hdr_name or "
				"sock_flag no set -> reseting\n");
			pkg_free(sock_hdr_name.s);
			sock_hdr_name.s = 0;
			sock_hdr_name.len = 0;
			sock_flag = -1;
		}
	} else if (sock_flag!=-1) {
		LOG(L_WARN,"WARN:registrar:init: sock_flag defined but no "
			"sock_hdr_name -> reseting flag\n");
		sock_flag = -1;
	}

	/* fix the flags */
	if (sock_flag!=-1)
		sock_flag = 1 << sock_flag;
	if (nat_flag!=-1)
		nat_flag = 1 << nat_flag;
	if (sip_natping_flag!=-1)
		sip_natping_flag = 1 << sip_natping_flag;

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

/*
 * Convert char* parameter to str*
 */
static int str_fixup(void** param, int param_no)
{
	str *s;

	if (param_no == 1) {
		s = (str*)pkg_malloc( sizeof(str) );
		if (s==0) {
			LOG(L_ERR,"ERROR:registrar:str_fixup: no more pkg mem\n");
			return E_UNSPEC;
		}
		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}
	return 0;
}



static void mod_destroy(void)
{
	free_contact_buf();
}


#include "../../data_lump.h"
#include "../../ip_addr.h"
#include "../../ut.h"

static int add_sock_hdr(struct sip_msg* msg, char *name, char *foo)
{
	struct lump* anchor;
	str *hdr_name;
	str hdr;
	char *p;

	hdr_name = (str*)name;

	if (parse_headers( msg, HDR_EOH_F, 0) == -1) {
		LOG(L_ERR,"ERROR:registrar:add_sock_hdr: failed to parse message\n");
		goto error;
	}

	anchor = anchor_lump( msg, msg->unparsed-msg->buf, 0, 0);
	if (anchor==0) {
		LOG(L_ERR,"ERROR:registrar:add_sock_hdr: can't get anchor\n");
		goto error;
	}

	hdr.len = hdr_name->len + 2 + msg->rcv.bind_address->address_str.len + 1
		+ msg->rcv.bind_address->port_no_str.len + CRLF_LEN;
	if ( (hdr.s=(char*)pkg_malloc(hdr.len))==0 ) {
		LOG(L_ERR,"ERROR:registrar:add_sock_hdr: no more pkg mem\n");
		goto error;
	}

	p = hdr.s;
	memcpy( p, hdr_name->s, hdr_name->len);
	p += hdr_name->len;
	*(p++) = ':';
	*(p++) = ' ';
	memcpy( p, msg->rcv.bind_address->address_str.s,
		msg->rcv.bind_address->address_str.len);
	p += msg->rcv.bind_address->address_str.len;
	*(p++) = '_';
	memcpy( p, msg->rcv.bind_address->port_no_str.s,
		msg->rcv.bind_address->port_no_str.len);
	p += msg->rcv.bind_address->port_no_str.len;
	memcpy( p, CRLF, CRLF_LEN);
	p += CRLF_LEN;

	if ( p-hdr.s!=hdr.len ) {
		LOG(L_CRIT,"BUG:registrar:add_sock_hdr: buffer overflow (%d!=%d)\n",
			(int)(long)(p-hdr.s),hdr.len);
		goto error1;
	}

	if (insert_new_lump_before( anchor, hdr.s, hdr.len, 0) == 0) {
		LOG(L_ERR, "ERROR:registrar:add_sock_hdr: can't insert lump\n");
		goto error1;
	}

	return 1;
error1:
	pkg_free(hdr.s);
error:
	return -1;
}

