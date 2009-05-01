/*
 * $Id$
 *
 * Registrar module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
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
 *  2006-09-19  AOR may be provided via an AVP instead of being fetched
 *              from URI (bogdan)
 *  2006-10-04  removed the "desc_time_order" parameter, as its functionality
 *              was moved to usrloc (Carsten Bock, BASIS AudioNet GmbH)
 *  2006-11-22  save_noreply and save_memory merged into save();
 *              removed the module parameter "use_domain" - now it is
 *              imported from usrloc module (bogdan)
 *  2006-11-28  Added statistics tracking for the number of accepted/rejected
 *              registrations, as well as for the max expiry time, max 
 *              contacts and default expiry time(Jeffrey Magder-SOMA Networks)
 *  2007-02-24  sip_natping_flag moved into branch flags, so migrated to 
 *              nathelper module (bogdan)
 *
 */

/*!
 * \defgroup registrar Registrar :: SIP Registrar support
 * The module contains REGISTER processing logic.
 */  

/*!
 * \file
 * \brief SIP registrar module - interface
 * \ingroup registrar   
 *
 * - Module: \ref registrar
 */  

#include <stdio.h>
#include "../../sr_module.h"
#include "../../timer.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../socket_info.h"
#include "../../pvar.h"
#include "../../lib/kcore/km_ut.h"
#include "../usrloc/ul_mod.h"
#include "../sl/sl_api.h"
#include "../../mod_fix.h"

#include "save.h"
#include "lookup.h"
#include "regpv.h"
#include "reply.h"
#include "reg_mod.h"


MODULE_VERSION

usrloc_api_t ul;/*!< Structure containing pointers to usrloc functions*/

/*! \brief Module init & destroy function */
static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);
/*! \brief Fixup functions */
static int domain_fixup(void** param, int param_no);
static int save_fixup(void** param, int param_no);
static int unreg_fixup(void** param, int param_no);
static int fetchc_fixup(void** param, int param_no);
/*! \brief Functions */
static int add_sock_hdr(struct sip_msg* msg, char *str, char *foo);


int default_expires = 3600; 			/*!< Default expires value in seconds */
qvalue_t default_q  = Q_UNSPECIFIED;		/*!< Default q value multiplied by 1000 */
int append_branches = 1;			/*!< If set to 1, lookup will put all contacts found in msg structure */
int case_sensitive  = 0;			/*!< If set to 1, username in aor will be case sensitive */
int tcp_persistent_flag = -1;			/*!< if the TCP connection should be kept open */
int min_expires     = 60;			/*!< Minimum expires the phones are allowed to use in seconds
 						 * use 0 to switch expires checking off */
int max_expires     = 0;			/*!< Maximum expires the phones are allowed to use in seconds,
 						 * use 0 to switch expires checking off */
int max_contacts = 0;				/*!< Maximum number of contacts per AOR (0=no checking) */
int retry_after = 0;				/*!< The value of Retry-After HF in 5xx replies */
int method_filtering = 0;			/*!< if the looked up contacts should be filtered based on supported methods */
int path_enabled = 0;				/*!< if the Path HF should be handled */
int path_mode = PATH_MODE_STRICT;		/*!< if the Path HF should be inserted in the reply.
 			*   - STRICT (2): always insert, error if no support indicated in request
 			*   - LAZY   (1): insert only if support indicated in request
 			*   - OFF    (0): never insert */

int path_use_params = 0;			/*!< if the received- and nat-parameters of last Path uri should be used
 						 * to determine if UAC is nat'ed */

char *aor_avp_param =0;				/*!< if instead of extacting the AOR from the request, it should be 
 						 * fetched via this AVP ID */
unsigned short aor_avp_type=0;
int_str aor_avp_name;

/* Populate this AVP if testing for specific registration instance. */
char *reg_callid_avp_param = 0;
unsigned short reg_callid_avp_type = 0;
int_str reg_callid_avp_name;

char* rcv_avp_param = 0;
unsigned short rcv_avp_type = 0;
int_str rcv_avp_name;

int reg_use_domain = 0;
char* realm_pref    = "";   			/*!< Realm prefix to be removed */
str realm_prefix;

int sock_flag = -1;
str sock_hdr_name = {0,0};

#define RCV_NAME "received"
str rcv_param = str_init(RCV_NAME);

stat_var *accepted_registrations;
stat_var *rejected_registrations;
stat_var *max_expires_stat;
stat_var *max_contacts_stat;
stat_var *default_expire_stat;

/** SL binds */
struct sl_binds slb;

/*! \brief
 * Exported PV
 */
static pv_export_t mod_pvs[] = {
	{ {"ulc", sizeof("ulc")-1}, PVT_OTHER, pv_get_ulc, pv_set_ulc,
		pv_parse_ulc_name, pv_parse_index, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"save",         (cmd_function)save,         1,    save_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE },
	{"save",         (cmd_function)save,         2,    save_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE },
	{"lookup",       (cmd_function)lookup,       1,  domain_fixup, 0,
			REQUEST_ROUTE | FAILURE_ROUTE },
	{"registered",   (cmd_function)registered,   1,  domain_fixup, 0,
			REQUEST_ROUTE | FAILURE_ROUTE },
	{"add_sock_hdr", (cmd_function)add_sock_hdr, 1,fixup_str_null, 0,
			REQUEST_ROUTE },
	{"unregister",   (cmd_function)unregister,   2,   unreg_fixup, 0,
			REQUEST_ROUTE| FAILURE_ROUTE },
	{"reg_fetch_contacts", (cmd_function)pv_fetch_contacts, 3, 
			fetchc_fixup, 0,
			REQUEST_ROUTE| FAILURE_ROUTE },
	{"reg_free_contacts", (cmd_function)pv_free_contacts,   1,
			fixup_str_null, 0,
			REQUEST_ROUTE| FAILURE_ROUTE },
	{0, 0, 0, 0, 0, 0}
};


/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"default_expires",    INT_PARAM, &default_expires     },
	{"default_q",          INT_PARAM, &default_q           },
	{"append_branches",    INT_PARAM, &append_branches     },
	{"case_sensitive",     INT_PARAM, &case_sensitive      },
	{"tcp_persistent_flag",INT_PARAM, &tcp_persistent_flag },
	{"realm_prefix",       STR_PARAM, &realm_pref          },
	{"min_expires",        INT_PARAM, &min_expires         },
	{"max_expires",        INT_PARAM, &max_expires         },
	{"received_param",     STR_PARAM, &rcv_param           },
	{"received_avp",       STR_PARAM, &rcv_avp_param       },
	{"aor_avp",            STR_PARAM, &aor_avp_param       },
	{"reg_callid_avp",     STR_PARAM, &reg_callid_avp_param},
	{"max_contacts",       INT_PARAM, &max_contacts        },
	{"retry_after",        INT_PARAM, &retry_after         },
	{"sock_flag",          INT_PARAM, &sock_flag           },
	{"sock_hdr_name",      STR_PARAM, &sock_hdr_name.s     },
	{"method_filtering",   INT_PARAM, &method_filtering    },
	{"use_path",           INT_PARAM, &path_enabled        },
	{"path_mode",          INT_PARAM, &path_mode           },
	{"path_use_received",  INT_PARAM, &path_use_params     },
	{0, 0, 0}
};


/*! \brief We expose internal variables via the statistic framework below.*/
stat_export_t mod_stats[] = {
	{"max_expires",       STAT_NO_RESET, &max_expires_stat        },
	{"max_contacts",      STAT_NO_RESET, &max_contacts_stat       },
	{"default_expire",    STAT_NO_RESET, &default_expire_stat     },
	{"accepted_regs",                 0, &accepted_registrations  },
	{"rejected_regs",                 0, &rejected_registrations  },
	{0, 0, 0}
};


/*! \brief
 * Module exports structure
 */
struct module_exports exports = {
	"registrar", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,        /* Exported functions */
	params,      /* Exported parameters */
	mod_stats,   /* exported statistics */
	0,           /* exported MI functions */
	mod_pvs,     /* exported pseudo-variables */
	0,           /* extra processes */
	mod_init,    /* module initialization function */
	0,
	mod_destroy, /* destroy function */
	child_init,  /* Per-child init function */
};


/*! \brief
 * Initialize parent
 */
static int mod_init(void)
{
	pv_spec_t avp_spec;
	str s;
	bind_usrloc_t bind_usrloc;


#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	/* load the SL API */
	if (load_sl_api(&slb)!=0) {
		LM_ERR("can't load SL API\n");
		return -1;
	}

	realm_prefix.s = realm_pref;
	realm_prefix.len = strlen(realm_pref);

	rcv_param.len = strlen(rcv_param.s);

	if (rcv_avp_param && *rcv_avp_param) {
		s.s = rcv_avp_param; s.len = strlen(s.s);
		if (pv_parse_spec(&s, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", rcv_avp_param);
			return -1;
		}

		if(pv_get_avp_name(0, &avp_spec.pvp, &rcv_avp_name, &rcv_avp_type)!=0)
		{
			LM_ERR("[%s]- invalid AVP definition\n", rcv_avp_param);
			return -1;
		}
	} else {
		rcv_avp_name.n = 0;
		rcv_avp_type = 0;
	}
	if (aor_avp_param && *aor_avp_param) {
		s.s = aor_avp_param; s.len = strlen(s.s);
		if (pv_parse_spec(&s, &avp_spec)==0
				|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", aor_avp_param);
			return -1;
		}

		if(pv_get_avp_name(0, &avp_spec.pvp, &aor_avp_name, &aor_avp_type)!=0)
		{
			LM_ERR("[%s]- invalid AVP definition\n", aor_avp_param);
			return -1;
		}
	} else {
		aor_avp_name.n = 0;
		aor_avp_type = 0;
	}

	if (reg_callid_avp_param && *reg_callid_avp_param) {
		s.s = reg_callid_avp_param; s.len = strlen(s.s);
		if (pv_parse_spec(&s, &avp_spec)==0
			|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", reg_callid_avp_param);
			return -1;
		}

		if(pv_get_avp_name(0, &avp_spec.pvp, &reg_callid_avp_name, &reg_callid_avp_type)!=0)
		{
			LM_ERR("[%s]- invalid AVP definition\n", reg_callid_avp_param);
			return -1;
		}
	} else {
		reg_callid_avp_name.n = 0;
		reg_callid_avp_type = 0;
	}

	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
	if (!bind_usrloc) {
		LM_ERR("can't bind usrloc\n");
		return -1;
	}

	/* Normalize default_q parameter */
	if (default_q != Q_UNSPECIFIED) {
		if (default_q > MAX_Q) {
			LM_DBG("default_q = %d, lowering to MAX_Q: %d\n", default_q, MAX_Q);
			default_q = MAX_Q;
		} else if (default_q < MIN_Q) {
			LM_DBG("default_q = %d, raising to MIN_Q: %d\n", default_q, MIN_Q);
			default_q = MIN_Q;
		}
	}
	

	if (bind_usrloc(&ul) < 0) {
		return -1;
	}

	/*
	 * Import use_domain parameter from usrloc
	 */
	reg_use_domain = ul.use_domain;

	if (sock_hdr_name.s) {
		sock_hdr_name.len = strlen(sock_hdr_name.s);
		if (sock_hdr_name.len==0 || sock_flag==-1) {
			LM_WARN("empty sock_hdr_name or sock_flag no set -> reseting\n");
			pkg_free(sock_hdr_name.s);
			sock_hdr_name.s = 0;
			sock_hdr_name.len = 0;
			sock_flag = -1;
		}
	} else if (sock_flag!=-1) {
		LM_WARN("sock_flag defined but no sock_hdr_name -> reseting flag\n");
		sock_flag = -1;
	}

	/* fix the flags */
	sock_flag = (sock_flag!=-1)?(1<<sock_flag):0;
	tcp_persistent_flag = (tcp_persistent_flag!=-1)?(1<<tcp_persistent_flag):0;

	return 0;
}


static int child_init(int rank)
{
	if (rank==1) {
		/* init stats */
		update_stat( max_expires_stat, max_expires );
		update_stat( max_contacts_stat, max_contacts );
		update_stat( default_expire_stat, default_expires );
	}

	return 0;
}


/*! \brief
 * Convert char* parameter to udomain_t* pointer
 */
static int domain_fixup(void** param, int param_no)
{
	udomain_t* d;

	if (param_no == 1) {
		if (ul.register_udomain((char*)*param, &d) < 0) {
			LM_ERR("failed to register domain\n");
			return E_UNSPEC;
		}

		*param = (void*)d;
	}
	return 0;
}

/*! \brief
 * Convert char* parameter to udomain_t* pointer
 * Convert char* parameter to pv_elem_t* pointer
 */
static int unreg_fixup(void** param, int param_no)
{
	if (param_no == 1) {
		return domain_fixup(param, 1);
	} else if (param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	return 0;
}



/*! \brief
 * Fixup for "save" function - both domain and flags
 */
static int save_fixup(void** param, int param_no)
{
	unsigned int flags;
	str s;

	if (param_no == 1) {
		return domain_fixup(param,param_no);
	} else {
		s.s = (char*)*param;
		s.len = strlen(s.s);
		flags = 0;
		if ( (strno2int(&s, &flags )<0) || (flags>REG_SAVE_ALL_FL) ) {
			LM_ERR("bad flags <%s>\n", (char *)(*param));
			return E_CFG;
		}
		if (ul.db_mode==DB_ONLY && flags&REG_SAVE_MEM_FL) {
			LM_ERR("MEM flag set while using the DB_ONLY mode in USRLOC\n");
			return E_CFG;
		}
		pkg_free(*param);
		*param = (void*)(unsigned long int)flags;
		return 0;
	}
}

/*! \brief
 * Convert char* parameter to udomain_t* pointer
 * Convert char* parameter to pv_elem_t* pointer
 * Convert char* parameter to str* pointer
 */
static int fetchc_fixup(void** param, int param_no)
{
	if (param_no == 1) {
		return domain_fixup(param, 1);
	} else if (param_no == 2) {
		return fixup_spve_null(param, 1);
	} else if (param_no == 3) {
		return fixup_str_null(param, 1);
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
	struct socket_info* si;
	struct lump* anchor;
	str *hdr_name;
	str hdr;
	char *p;

	hdr_name = (str*)name;
	si = msg->rcv.bind_address;

	if (parse_headers( msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse message\n");
		goto error;
	}

	anchor = anchor_lump( msg, msg->unparsed-msg->buf, 0, 0);
	if (anchor==0) {
		LM_ERR("can't get anchor\n");
		goto error;
	}

	hdr.len = hdr_name->len + 2 + si->sock_str.len + CRLF_LEN;
	if ( (hdr.s=(char*)pkg_malloc(hdr.len))==0 ) {
		LM_ERR("no more pkg mem\n");
		goto error;
	}

	p = hdr.s;
	memcpy( p, hdr_name->s, hdr_name->len);
	p += hdr_name->len;
	*(p++) = ':';
	*(p++) = ' ';

	memcpy( p, si->sock_str.s, si->sock_str.len);
	p += si->sock_str.len;

	memcpy( p, CRLF, CRLF_LEN);
	p += CRLF_LEN;

	if ( p-hdr.s!=hdr.len ) {
		LM_CRIT("buffer overflow (%d!=%d)\n", (int)(long)(p-hdr.s),hdr.len);
		goto error1;
	}

	if (insert_new_lump_before( anchor, hdr.s, hdr.len, 0) == 0) {
		LM_ERR("can't insert lump\n");
		goto error1;
	}

	return 1;
error1:
	pkg_free(hdr.s);
error:
	return -1;
}

