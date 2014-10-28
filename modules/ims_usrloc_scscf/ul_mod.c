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
#include "ul_mod.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../rpc_lookup.h"
#include "../../timer.h"     /* register_timer */
#include "../../globals.h"   /* is_main */
#include "../../ut.h"        /* str_init */
#include "dlist.h"           /* register_udomain */
#include "udomain.h"         /* {insert,delete,get,release}_urecord */
#include "impurecord.h"         /* {insert,delete,get}_ucontact */
#include "ucontact.h"        /* update_ucontact */
#include "ul_rpc.h"
#include "ul_callback.h"
#include "usrloc.h"
#include "hslot_sp.h"
#include "usrloc_db.h"
#include "contact_hslot.h"
#include "../presence/bind_presence.h"
#include "../presence/hash.h"
#include "../../modules/dialog_ng/dlg_load.h"
#include "../../modules/dialog_ng/dlg_hash.h"

MODULE_VERSION

#define DEFAULT_DBG_FILE "/var/log/usrloc_debug"

static FILE *debug_file;

static int mod_init(void);                          /*!< Module initialization function */
static void destroy(void);                          /*!< Module destroy function */
static void timer(unsigned int ticks, void* param); /*!< Timer handler */
static int child_init(int rank);                    /*!< Per-child init function */

extern int bind_usrloc(usrloc_api_t* api);
extern int ul_locks_no;
extern int subs_locks_no;
extern int contacts_locks_no;
/*
 * Module parameters and their default values
 */
str usrloc_debug_file = str_init(DEFAULT_DBG_FILE);
char *scscf_user_data_dtd=0;      		/* Path to "CxDataType.dtd" */
char *scscf_user_data_xsd=0;			/* Path to "CxDataType_Rel6.xsd" or "CxDataType_Rel7.xsd" */
int timer_interval  = 90;				/*!< Timer interval in seconds */
int desc_time_order = 0;				/*!< By default do not enable timestamp ordering */
int usrloc_debug 	= 0;
int scscf_support_wildcardPSI = 0;
int unreg_validity = 1800;				/*!< default validity time in secs for unreg assignment to SCSCF */
int maxcontact = 0;						/*!< max number of contacts allowed per IMPU */
int maxcontact_behaviour = 0;			/*!< max contact behaviour - 0-disabled(default),1-reject,2-overwrite*/

int ul_fetch_rows = 2000;				/*!< number of rows to fetch from result */
int ul_hash_size = 9;
int subs_hash_size = 9;					/*!<number of ims subscription slots*/
int contacts_hash_size = 9;

struct contact_list* contact_list;

int db_mode = 0;						/*!<database mode*/
db1_con_t* ul_dbh = 0;
db_func_t ul_dbf;
str db_url          = str_init(DEFAULT_DB_URL);	/*!< Database URL */

/* flags */
unsigned int nat_bflag = (unsigned int)-1;
unsigned int init_flag = 0;

struct dlg_binds dlgb;

int sub_dialog_hash_size = 9;
shtable_t sub_dialog_table;

new_shtable_t pres_new_shtable;
insert_shtable_t pres_insert_shtable;
search_shtable_t pres_search_shtable;
update_shtable_t pres_update_shtable;
delete_shtable_t pres_delete_shtable;
destroy_shtable_t pres_destroy_shtable;
extract_sdialog_info_t pres_extract_sdialog_info;

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"ul_bind_usrloc",        (cmd_function)bind_usrloc,        1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*! \brief
 * Exported parameters 
 */
static param_export_t params[] = {
	{"timer_interval",    	INT_PARAM, &timer_interval  },
	{"desc_time_order",   	INT_PARAM, &desc_time_order },
	{"matching_mode",     	INT_PARAM, &matching_mode   },
	{"cseq_delay",        	INT_PARAM, &cseq_delay      },
	{"fetch_rows",        	INT_PARAM, &ul_fetch_rows   },
	{"hash_size",         	INT_PARAM, &ul_hash_size    },
	{"subs_hash_size",    	INT_PARAM, &subs_hash_size  },
	{"nat_bflag",         	INT_PARAM, &nat_bflag       },
	{"usrloc_debug_file", 	PARAM_STR, &usrloc_debug_file},
	{"enable_debug_file", 	INT_PARAM, &usrloc_debug},
    {"user_data_dtd",     	PARAM_STRING, &scscf_user_data_dtd},
    {"user_data_xsd",     	PARAM_STRING, &scscf_user_data_xsd},
    {"support_wildcardPSI",	INT_PARAM, &scscf_support_wildcardPSI},
    {"unreg_validity",		INT_PARAM, &unreg_validity},
    {"maxcontact_behaviour",INT_PARAM, &maxcontact_behaviour},
    {"maxcontact",			INT_PARAM, &maxcontact},
    {"sub_dialog_hash_size",INT_PARAM, &sub_dialog_hash_size},
    {"db_mode",				INT_PARAM, &db_mode},
    {"db_url", 				PARAM_STR, &db_url},
	{0, 0, 0}
};


stat_export_t mod_stats[] = {
	{"registered_users" ,  STAT_IS_FUNC, (stat_var**)get_number_of_users  },
	{0,0,0}
};


static mi_export_t mi_cmds[] = {
	{ 0, 0, 0, 0, 0}
};


struct module_exports exports = {
	"ims_usrloc_scscf",
	DEFAULT_DLFLAGS, /*!< dlopen flags */
	cmds,       /*!< Exported functions */
	params,     /*!< Export parameters */
	mod_stats,  /*!< exported statistics */
	mi_cmds,    /*!< exported MI functions */
	0,          /*!< exported pseudo-variables */
	0,          /*!< extra processes */
	mod_init,   /*!< Module initialization function */
	0,          /*!< Response function */
	destroy,    /*!< Destroy function */
	child_init  /*!< Child initialization function */
};


/*! \brief
 * Module initialization function
 */
static int mod_init(void) {
	int i;
	load_dlg_f load_dlg;
	if (usrloc_debug){
		LM_INFO("Logging usrloc records to %.*s\n", usrloc_debug_file.len, usrloc_debug_file.s);
		debug_file = fopen(usrloc_debug_file.s, "a");
		fprintf(debug_file, "starting\n");
		fflush(debug_file);
	}

#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	if (rpc_register_array(ul_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if (ul_hash_size <= 1)
		ul_hash_size = 512;
	else
		ul_hash_size = 1 << ul_hash_size;
	ul_locks_no = ul_hash_size;

	if (subs_hash_size <= 1)
		subs_hash_size = 512;
	else
		subs_hash_size = 1 << subs_hash_size;
	subs_locks_no = subs_hash_size;
	
	if (contacts_hash_size <= 1)
		contacts_hash_size = 512;
	else
		contacts_hash_size = 1 << contacts_hash_size;
	contacts_locks_no = contacts_hash_size;

	/* check matching mode */
	switch (matching_mode) {
		case CONTACT_ONLY:
		case CONTACT_CALLID:
		case CONTACT_PATH:
		case CONTACT_PORT_IP_ONLY:
			break;
		default:
			LM_ERR("invalid matching mode %d\n", matching_mode);
	}

	if (ul_init_locks() != 0) {
		LM_ERR("locks array initialization failed\n");
		return -1;
	}

	if (subs_init_locks() != 0) {
		LM_ERR("IMS Subscription locks array initialization failed\n");
		return -1;
	}
	
	/* create hash table for storing registered contacts */
	if (init_contacts_locks() !=0) {
	    LM_ERR("failed to initialise locks array for contacts\n");
	    return -1;
	}
	contact_list = (struct contact_list*)shm_malloc(sizeof(struct contact_list));
	if (!contact_list) {
	    LM_ERR("no more memory to create contact list structure\n");
	    return -1;
	}
	contact_list->slot = (struct contact_hslot*) shm_malloc(sizeof(struct contact_hslot) * contacts_hash_size);
	if (!contact_list->slot) {
	    LM_ERR("no more memory to create contact list structure\n");
	    return -1;
	}
	for (i=0; i<contacts_hash_size;i++) {
	    init_contact_slot(&contact_list->slot[i], i);
	} 
	contact_list->size = contacts_hash_size;
	
	/* presence binding for subscribe processing*/
	presence_api_t pres;
	bind_presence_t bind_presence;

	bind_presence= (bind_presence_t)find_export("bind_presence", 1,0);
	if (!bind_presence) {
	    LM_ERR("can't bind presence\n");
	    return -1;
	}
	if (bind_presence(&pres) < 0) {
	    LM_ERR("can't bind pua\n");
	    return -1;
	}

	pres_extract_sdialog_info= pres.extract_sdialog_info;
	pres_new_shtable          = pres.new_shtable;
	pres_destroy_shtable      = pres.destroy_shtable;
	pres_insert_shtable       = pres.insert_shtable;
	pres_delete_shtable       = pres.delete_shtable;
	pres_update_shtable       = pres.update_shtable;
	pres_search_shtable       = pres.search_shtable;


	if(!pres_new_shtable || !pres_destroy_shtable || !pres_insert_shtable || !pres_delete_shtable
		     || !pres_update_shtable || !pres_search_shtable || !pres_extract_sdialog_info) {
	    LM_ERR("could not import add_event\n");
	    return -1;
	}

	/* subscriber dialog hash table */
	if(sub_dialog_hash_size<=1) {
	    sub_dialog_hash_size= 512;
	}
	else {
	    sub_dialog_hash_size = 1<<sub_dialog_hash_size;
	}

	sub_dialog_table= pres_new_shtable(sub_dialog_hash_size);
	if(sub_dialog_table== NULL)
	{
		LM_ERR("while creating new hash table\n");
		return -1;
	}

	/* Shall we use database ? */
	if (db_mode != NO_DB) { /* Yes */
		if (db_bind_mod(&db_url, &ul_dbf) < 0) { /* Find database module */
			LM_ERR("failed to bind database module\n");
			return -1;
		}
		if (!DB_CAPABILITY(ul_dbf, DB_CAP_ALL)) {
			LM_ERR("database module does not implement all functions"
					" needed by the module\n");
			return -1;
		}
		if (ul_fetch_rows <= 0) {
			LM_ERR("invalid fetch_rows number '%d'\n", ul_fetch_rows);
			return -1;
		}
	}

	if (!(load_dlg = (load_dlg_f) find_export("load_dlg", 0, 0))) { /* bind to dialog module */
		LM_ERR("can not import load_dlg. This module requires Kamailio dialog module.\n");
	}
	if (load_dlg(&dlgb) == -1) {
		return -1;
	}
	if (load_dlg_api(&dlgb) != 0) { /* load the dialog API */
		LM_ERR("can't load Dialog API\n");
		return -1;
	}
	
	/* Register cache timer */
	register_timer(timer, 0, timer_interval);

	/* init the callbacks list */
	if (init_ulcb_list() < 0) {
		LM_ERR("usrloc/callbacks initialization failed\n");
		return -1;
	}

	if (nat_bflag == (unsigned int) -1) {
		nat_bflag = 0;
	} else if (nat_bflag >= 8 * sizeof(nat_bflag)) {
		LM_ERR("bflag index (%d) too big!\n", nat_bflag);
		return -1;
	} else {
		nat_bflag = 1 << nat_bflag;
	}

	init_flag = 1;
	
	return 0;
}


static int child_init(int rank)
{
	dlist_t* ptr;

	/* connecting to DB ? */
	switch (db_mode) {
	case NO_DB:
		return 0;
	case WRITE_THROUGH:
		/* we need connection from working SIP and TIMER and MAIN
		 * processes only */
		if (rank <= 0 && rank != PROC_TIMER && rank != PROC_MAIN)
			return 0;
		break;
	}

	ul_dbh = ul_dbf.init(&db_url); /* Get a database connection per child */
	if (!ul_dbh) {
		LM_ERR("child(%d): failed to connect to database\n", rank);
		return -1;
	}

	/* _rank==PROC_SIPINIT is used even when fork is disabled */
	if (rank == PROC_SIPINIT && db_mode != DB_ONLY) {
		/* if cache is used, populate from DB */
		for (ptr = root; ptr; ptr = ptr->next) {
			if (preload_udomain(ul_dbh, ptr->d) < 0) {
				LM_ERR("child(%d): failed to preload domain '%.*s'\n",
						rank, ptr->name.len, ZSW(ptr->name.s));
				return -1;
			}
		}
	}

	return 0;
}


/*! \brief
 * Module destroy function
 */
static void destroy(void) {
	if (sub_dialog_table) {
		pres_destroy_shtable(sub_dialog_table, sub_dialog_hash_size);
	}

	if (ul_dbh) {
		ul_unlock_locks();
		if (synchronize_all_udomains() != 0) {
			LM_ERR("flushing cache failed\n");
		}
		ul_dbf.close(ul_dbh);
	}

	free_all_udomains();
	ul_destroy_locks();
	subs_destroy_locks();
	destroy_contacts_locks();

	/* free callbacks list */
	destroy_ulcb_list();
}


/*! \brief
 * Timer handler
 */
static void timer(unsigned int ticks, void* param) {
	if (usrloc_debug) {
		print_all_udomains(debug_file);
		fflush(debug_file);
	}

	LM_DBG("Syncing cache\n");
	if (synchronize_all_udomains() != 0) {
		LM_ERR("synchronizing cache failed\n");
	}
}

