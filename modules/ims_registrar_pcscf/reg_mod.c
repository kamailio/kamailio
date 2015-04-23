/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 */


#include <stdio.h>
#include "../../data_lump.h"
#include "../../ip_addr.h"
#include "../../ut.h"
#include "../../sr_module.h"
#include "../../timer.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../socket_info.h"
#include "../../pvar.h"
#include "../ims_usrloc_pcscf/usrloc.h"
#include "../../lib/kcore/statistics.h"
#include "../../modules/sl/sl.h"
#include "../../mod_fix.h"
#include "../../cfg/cfg_struct.h"

/* Bindings to PUA */
#include "../pua/pua_bind.h"
#include "notify.h"

#include "async_reginfo.h"

#include "reg_mod.h"
#include "save.h"
#include "service_routes.h"
#include "lookup.h"
MODULE_VERSION

usrloc_api_t ul;						/**!< Structure containing pointers to usrloc functions*/
sl_api_t slb;							/**!< SL API structure */
struct tm_binds tmb;					/**!< TM API structure */
pua_api_t pua; 							/**!< PUA API structure */

int publish_reginfo = 0;
int subscribe_to_reginfo = 0;
int subscription_expires = 3600;
int ignore_reg_state = 0;
int ignore_contact_rxport_check = 0;                             /**!< ignore port checks between received port on message and 
registration received port. 
                                                                 * this is useful for example if you register with UDP but possibly send invite over TCP (message too big)*/

time_t time_now;

str pcscf_uri = str_init("sip:pcscf.ims.smilecoms.com:4060");
str force_icscf_uri = str_init("");

unsigned int pending_reg_expires = 30;			/**!< parameter for expiry time of a pending registration before receiving confirmation from SCSCF */

int is_registered_fallback2ip = 0;

int reginfo_queue_size_threshold = 0;    /**Threshold for size of reginfo queue after which a warning is logged */


char* rcv_avp_param = 0;
unsigned short rcv_avp_type = 0;
int_str rcv_avp_name;

// static str orig_prefix = {"sip:orig@",9};

/*! \brief Module init & destroy function */
static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);
static int w_save(struct sip_msg* _m, char* _d, char* _cflags);
static int w_save_pending(struct sip_msg* _m, char* _d, char* _cflags);

static int w_follows_service_routes(struct sip_msg* _m, char* _d, char* _foo);
static int w_force_service_routes(struct sip_msg* _m, char* _d, char* _foo);
static int w_is_registered(struct sip_msg* _m, char* _d, char* _foo);
static int w_reginfo_handle_notify(struct sip_msg* _m, char* _d, char* _foo);

static int w_assert_identity(struct sip_msg* _m, char* _d, char* _preferred_uri);
static int w_assert_called_identity(struct sip_msg* _m, char* _d, char* _foo);

static int w_lookup_transport(struct sip_msg* _m, char* _d, char* _uri);

/*! \brief Fixup functions */
static int domain_fixup(void** param, int param_no);
static int domain_uri_fixup(void** param, int param_no);
static int save_fixup2(void** param, int param_no);
static int assert_identity_fixup(void ** param, int param_no);

/* Pseudo-Variables */
static int pv_get_asserted_identity_f(struct sip_msg *, pv_param_t *, pv_value_t *);
static int pv_get_registration_contact_f(struct sip_msg *, pv_param_t *, pv_value_t *);

/**
 * Update the time.
 */
inline void pcscf_act_time()
{
        time_now=time(0);
}

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"pcscf_save",     		(cmd_function)w_save,                   1,  	save_fixup2,            0,ONREPLY_ROUTE },
	{"pcscf_save_pending",          (cmd_function)w_save_pending,       	1,  	save_fixup2,            0,REQUEST_ROUTE },
	{"pcscf_follows_service_routes",(cmd_function)w_follows_service_routes, 1,  	save_fixup2,            0,REQUEST_ROUTE },
	{"pcscf_force_service_routes",  (cmd_function)w_force_service_routes,   1,  	save_fixup2,            0,REQUEST_ROUTE },
	{"pcscf_is_registered",         (cmd_function)w_is_registered,          1,  	save_fixup2,            0,REQUEST_ROUTE|ONREPLY_ROUTE },
	{"pcscf_assert_identity",       (cmd_function)w_assert_identity,        2,  	assert_identity_fixup,  0,REQUEST_ROUTE },
	{"pcscf_assert_called_identity",(cmd_function)w_assert_called_identity, 1,      assert_identity_fixup,  0,ONREPLY_ROUTE },
	{"reginfo_handle_notify",       (cmd_function)w_reginfo_handle_notify,  1,      domain_fixup,           0,REQUEST_ROUTE},
        {"lookup_transport",		(cmd_function)w_lookup_transport,       1,      domain_fixup,           0,REQUEST_ROUTE|FAILURE_ROUTE},
        {"lookup_transport",		(cmd_function)w_lookup_transport,       2,      domain_uri_fixup,        0,REQUEST_ROUTE|FAILURE_ROUTE},
	
	{0, 0, 0, 0, 0, 0}
};


/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"pcscf_uri",                   PARAM_STR, &pcscf_uri                           },
	{"pending_reg_expires",         INT_PARAM, &pending_reg_expires			},
	{"received_avp",                PARAM_STR, &rcv_avp_param       		},
	{"is_registered_fallback2ip",	INT_PARAM, &is_registered_fallback2ip           },
	{"publish_reginfo",             INT_PARAM, &publish_reginfo                     },
        {"subscribe_to_reginfo",        INT_PARAM, &subscribe_to_reginfo                },
        {"subscription_expires",        INT_PARAM, &subscription_expires                },
        {"ignore_contact_rxport_check", INT_PARAM, &ignore_contact_rxport_check         },
        {"ignore_reg_state",		INT_PARAM, &ignore_reg_state			},
	{"force_icscf_uri",		PARAM_STR, &force_icscf_uri			},
	{"reginfo_queue_size_threshold",	INT_PARAM, &reginfo_queue_size_threshold		},
//	{"store_profile_dereg",	INT_PARAM, &store_data_on_dereg},
	{0, 0, 0}
};


/*! \brief We expose internal variables via the statistic framework below.*/
stat_export_t mod_stats[] = {
	{0, 0, 0}
};


static pv_export_t mod_pvs[] = {
    {{"pcscf_asserted_identity", (sizeof("pcscf_asserted_identity")-1)}, /* The first identity of the contact. */
     PVT_OTHER, pv_get_asserted_identity_f, 0, 0, 0, 0, 0},
    {{"pcscf_registration_contact", (sizeof("pcscf_registration_contact")-1)}, /* The contact used during REGISTER */
     PVT_OTHER, pv_get_registration_contact_f, 0, 0, 0, 0, 0},
    {{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

/*! \brief
 * Module exports structure
 */
struct module_exports exports = {
	"ims_registrar_pcscf",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,        	/* Exported functions */
	params,      	/* Exported parameters */
	mod_stats,   	/* exported statistics */
	0,           	/* exported MI functions */
	mod_pvs,     	/* exported pseudo-variables */
	0,           	/* extra processes */
	mod_init,    	/* module initialization function */
	0,
	mod_destroy, 	/* destroy function */
	child_init,  	/* Per-child init function */
};

int fix_parameters() {
	str s;
	pv_spec_t avp_spec;

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

	return 1;
}

/*! \brief
 * Initialize parent
 */
static int mod_init(void) {
	bind_usrloc_t bind_usrloc;
	bind_pua_t bind_pua;

	/*register space for event processor*/
	register_procs(1);
	
	if (!fix_parameters()) goto error;

	/* bind the SL API */
	if (sl_load_api(&slb) != 0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}
	LM_DBG("Successfully bound to SL module\n");

	/* load the TM API */
	if (load_tm_api(&tmb) != 0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}
	LM_DBG("Successfully bound to TM module\n");

	bind_usrloc = (bind_usrloc_t) find_export("ul_bind_ims_usrloc_pcscf", 1, 0);
	if (!bind_usrloc) {
		LM_ERR("can't bind ims_usrloc_pcscf\n");
		return -1;
	}

	if (bind_usrloc(&ul) < 0) {
		return -1;
	}
	LM_DBG("Successfully bound to PCSCF Usrloc module\n");

       if(subscribe_to_reginfo == 1){
               /* Bind to PUA: */
               bind_pua = (bind_pua_t) find_export("bind_pua", 1, 0);
               if (!bind_pua) {
                       LM_ERR("Can't bind pua\n");
                       return -1;
               }
               if (bind_pua(&pua) < 0) {
                       LM_ERR("Can't bind pua\n");
                       return -1;
               }
               /* Check for Publish/Subscribe methods */
               if (pua.send_publish == NULL) {
                       LM_ERR("Could not import send_publish\n");
                       return -1;
               }
               if (pua.send_subscribe == NULL) {
                       LM_ERR("Could not import send_subscribe\n");
                       return -1;
               }
	       if (pua.get_subs_list == NULL) {
                       LM_ERR("Could not import get_subs_list\n");
                       return -1;
               }
	       LM_DBG("Successfully bound to PUA module\n");
	       
	       /*init cdb cb event list*/
		if (!init_reginfo_event_list()) {
		    LM_ERR("unable to initialise reginfo_event_list\n");
		    return -1;
		}
	       LM_DBG("Successfully initialised reginfo_event_list\n");
       }

	return 0;

error:
	return -1;
}

static void mod_destroy(void)
{

}

static int child_init(int rank)
{
    
	LM_DBG("Initialization of module in child [%d] \n", rank);
        if ((subscribe_to_reginfo == 1) && (rank == PROC_MAIN)) {
	     LM_DBG("Creating RegInfo Event Processor process\n");
	    int pid = fork_process(PROC_SIPINIT, "RegInfo Event Processor", 1);
	    if (pid < 0)
		return -1; //error
	    if (pid == 0) {
		if (cfg_child_init())
		    return -1; //error
		reginfo_event_process();
	    }
	}
    
	if (rank == PROC_MAIN || rank == PROC_TCP_MAIN)
		return 0;
	if (rank == 1) {
		/* init stats */
		//TODO if parameters are modified via cfg framework do i change them?
		//update_stat( max_expires_stat, default_registrar_cfg.max_expires ); update_stat( max_contacts_stat, default_registrar_cfg.max_contacts ); update_stat( default_expire_stat, default_registrar_cfg.default_expires );
	}

	/* don't do anything for main process and TCP manager process */
	if (rank == PROC_MAIN || rank == PROC_TCP_MAIN)
		return 0;

	return 0;
}

/* fixups */
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

static int domain_uri_fixup(void** param, int param_no)
{
	udomain_t* d;

	if (param_no == 1) {
		if (ul.register_udomain((char*)*param, &d) < 0) {
			LM_ERR("failed to register domain\n");
			return E_UNSPEC;
		}
		*param = (void*)d;
	} else {
            fixup_var_pve_12(param, param_no);
        }
        
	return 0;
}

/*! \brief
 * Fixup for "save" function - both domain and flags
 */
static int save_fixup2(void** param, int param_no)
{
	if (param_no == 1) {
		return domain_fixup(param,param_no);
	}
        return 0;
}

/*! \brief
 * Fixup for "assert_identity" function - both domain and URI to be asserted
 */
static int assert_identity_fixup(void ** param, int param_no) {
	if (param_no == 1) {
		return domain_fixup(param,param_no);
	}
	if (param_no == 2) {
		pv_elem_t *model=NULL;
		str s;

		/* convert to str */
		s.s = (char*)*param;
		s.len = strlen(s.s);

		model = NULL;
		if(s.len==0) {
			LM_ERR("no param!\n");
			return E_CFG;
		}
		if(pv_parse_format(&s, &model)<0 || model==NULL) {
			LM_ERR("wrong format [%s]!\n", s.s);
			return E_CFG;
		}
		*param = (void*)model;
		return 0;
	}
	return E_CFG;
}

/*! \brief
 * Wrapper to save(location)
 */
static int w_save(struct sip_msg* _m, char* _d, char* _cflags)
{
	return save(_m, (udomain_t*)_d, ((int)(unsigned long)_cflags));
}

/*! \brief
 * Wrapper to lookup_transport(location)
 */
static int w_lookup_transport(struct sip_msg* _m, char* _d, char* _uri)
{
	str uri = {0};
	if(_uri!=NULL && (fixup_get_svalue(_m, (gparam_p)_uri, &uri)!=0 || uri.len<=0))
	{
		LM_ERR("invalid uri parameter\n");
		return -1;
	}

	return lookup_transport(_m, (udomain_t*)_d, (uri.len>0)?&uri:NULL);
}

static int w_save_pending(struct sip_msg* _m, char* _d, char* _cflags)
{
	return save_pending(_m, (udomain_t*)_d);
}

static int w_follows_service_routes(struct sip_msg* _m, char* _d, char* _foo)
{
	return check_service_routes(_m, (udomain_t*)_d);
}

static int w_force_service_routes(struct sip_msg* _m, char* _d, char* _foo)
{
	return force_service_routes(_m, (udomain_t*)_d);
}

static int w_is_registered(struct sip_msg* _m, char* _d, char* _foo)
{
	return is_registered(_m, (udomain_t*)_d);
}

static int w_reginfo_handle_notify(struct sip_msg* _m, char* _d, char* _foo)
{
       return reginfo_handle_notify(_m, _d, _foo);
}

static int w_assert_identity(struct sip_msg* _m, char* _d, char* _preferred_uri) {
	pv_elem_t *model;
	str identity;

	if(_preferred_uri == NULL) {
		LM_ERR("error - bad parameters\n");
		return -1;
	}

	model = (pv_elem_t*)_preferred_uri;
	if (pv_printf_s(_m, model, &identity)<0) {
		LM_ERR("error - cannot print the format\n");
		return -1;
	}

	return assert_identity( _m, (udomain_t*)_d, identity);
}

static int w_assert_called_identity(struct sip_msg* _m, char* _d, char* _foo) {
	return assert_called_identity( _m, (udomain_t*)_d);
}

/*
 * Get the asserted Identity for the current user
 */
static int
pv_get_asserted_identity_f(struct sip_msg *msg, pv_param_t *param,
		  pv_value_t *res)
{
	str * ret_val = get_asserted_identity(msg);
	if (ret_val != NULL) return pv_get_strval(msg, param, res, ret_val);
	else return -1;
}


/*
 * Get the asserted Identity for the current user
 */
static int
pv_get_registration_contact_f(struct sip_msg *msg, pv_param_t *param,
		  pv_value_t *res)
{
	str * ret_val = get_registration_contact(msg);
	if (ret_val != NULL) return pv_get_strval(msg, param, res, ret_val);
	else return -1;
}
