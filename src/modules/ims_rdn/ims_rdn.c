/*
 * ims_rdn module
 *
 * Copyright (C) 2016 Peter Friedrich  
 * Copyright (C) 2016 - 2025 Kontron Transportation GmbH,
 *               theodor.scherney@kontron.com
 *               christoph.eckl@kontron.com
 *               luca.nardin@kontron.com
 *               christoph.valentin@kontron.com
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
 */
// This file added by %KTRVICT%
// Creating branch: vict-tagging
// Modified by branches: vict-tagging devel_ood_refer devel_voipdemo
//                       bugfix_iwf_demo_mty bugfix_hardcodedruri_fqdn
//                       devel_iwf_wle rebase_stichtag_5.5.1
//                       fix_PR-0002452_ims_rdn_return_values
//                       devel_iwf_DI2.57CEP

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../lib/ims/ims_getters.h"
// %KTAVICT% - BEGIN - improve_registrar_api
#include "../ims_registrar_scscf/api.h"
// %KTAVICT% - END - improve_registrar_api
#include "../ims_usrloc_scscf/udomain.h"
#include "../../../src/core/parser/parse_uri.h"
#include "../../../src/core/parser/msg_parser.h"
#include "../../../src/core/parser/parser_f.h"
#include "../../../src/core/usr_avp.h"
#include "../../../src/core/mod_fix.h"
#include "../../../src/core/pvar.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "../../../src/core/cfg/cfg_struct.h"
#include "../../../src/core/lvalue.h"
#include "ims_rdn.h"

MODULE_VERSION

// exported module parameters
char *rdnlistfile = CFG_DIR "ims_rdn.list";
char *rdnlistfile_dpsi = CFG_DIR "dpsi.list";
char *cx_userdata_templates = CFG_DIR "CxUserData_tmpl.xml";
int rdn_node_type = RDN_NODE_TYPE_DEFAULT;

// %KTAVICT% - BEGIN - improve_registrar_api
// module APIs (ims_usrloc_scscf, tm, ims_registrar_scscf)
// %KTAVICT% - END - improve_registrar_api
usrloc_api_t ul; /*!< Structure containing pointers to usrloc functions*/
struct tm_binds tmb;
// %KTAVICT% - BEGIN - improve_registrar_api
registrar_api_t ra;
// %KTAVICT% - END - improve_registrar_api

// pointer to the first levels of the digit trees in shared memory
// currently, only the USER_PARAM_E164 and the USER_PARAM_EIRENE are used
static digit_node_t *rdn_first_levels[] = {0, 0, 0, 0};

// the user parameters for the types of digit trees
// currently, only the USER_PARAM_E164 and the USER_PARAM_EIRENE are used
static char *rdn_user[] = {
		USER_PARAM_INET, USER_PARAM_E164, USER_PARAM_DIAL, USER_PARAM_EIRENE};

// pointer to the DPSI table in shared memory
struct hash_entry **dpsi_table = 0;

// pointer to the XML template for unreg/term delivery in shared memory
str unreg_term_template = {0, 0};

// global pv_spec parameters
static pv_spec_t rdn_as_name_pv;
static pv_spec_t rdn_scscf_name_pv;
static pv_spec_t rdn_sessiontype_pv;
static pv_spec_t rdn_iwf_domain_pv;
static pv_spec_t rdn_hardcoded_ruri_pv;
static pv_spec_t rdn_del_pv;
static pv_spec_t rdn_insrt_pv;
static pv_spec_t rdn_mcx_domain_avp;

/** module functions */
static int mod_init(void);
static void mod_destroy(void);

/* global variable for special handling */
special_handling_t special_handling_global = {
		{0, 0}, // hardcoded_ruri
		0,		// del
		{0, 0}, // insrt
		{0, 0}, // mcx_domain
		{0, 0}	// iwf_domain
};


//
// 1) Utilities for the handling of the DPSI Table
//

// String hash function
static unsigned int calc_hash(str *key)
{
	char *p;
	unsigned int h, len, i;

	h = 0;
	p = key->s;
	len = key->len;

	for(i = 0; i < len; i++) {
		h = (h << 5) - h + *(p + i);
	}

	return h % HASH_SIZE;
}

// Create new hash_entry structure from given key and service
static struct hash_entry *new_hash_entry(str *key, rdn_service_t *service)
{
	struct hash_entry *e;

	if(!key || !service) {
		ERR("Invalid parameter value\n");
		return 0;
	}

	e = (struct hash_entry *)shm_malloc(sizeof(struct hash_entry));
	if(!e) {
		ERR("Not enough memory left\n");
		return 0;
	}
	e->key = *key;
	e->service = service;
	e->next = 0;
	return e;
}

// Lookup key in dpsi_table
int key_exists(str *search_key)
{
	struct hash_entry *np;

	for(np = dpsi_table[calc_hash(search_key)]; np != NULL; np = np->next) {
		if((np->key.len == search_key->len)
				&& (strncmp(np->key.s, search_key->s, search_key->len) == 0)) {
			return 1;
		}
	}
	return 0;
}

static int allocate_dpsi_table(void)
{
	dpsi_table = (struct hash_entry **)shm_malloc(sizeof(struct hash_entry *)

												  * HASH_SIZE);

	if(!dpsi_table) {
		ERR("No memory left\n");
		return -1;
	}
	memset(dpsi_table, 0, sizeof(struct hash_entry *) * HASH_SIZE);
	return 0;
}

//
// 2) Fixup of Input Parameter during startup
//

static int domain_fixup(void **param, int param_no)
{
	udomain_t *d;

	if((rdn_node_type & RDN_NODE_TYPE_MASK) != RDN_NODE_TYPE_SCSCF) {
		LM_ERR("script programming error. This function needs API of module "
			   "ims_usrloc_scscf and must run on S-CSCF.\n");
		return E_SCRIPT;
	}
	if(param_no == 1) {
		if(ul.register_udomain((char *)*param, &d) < 0) {
			LM_ERR("failed to register domain\n");
			return E_UNSPEC;
		}

		*param = (void *)d;
	}
	return 0;
}


//
// 4) Utilities for Startup / Shutdown
//

//
// 4.a) Utilities for Shutdown
//

void deleteSpecialHandling(special_handling_t **temp_special_handling)
{
	if(*temp_special_handling) {
		if((*temp_special_handling)->mcx_domain.s)
			shm_free((*temp_special_handling)->mcx_domain.s);
		if((*temp_special_handling)->iwf_domain.s)
			shm_free((*temp_special_handling)->iwf_domain.s);
		if((*temp_special_handling)->hardcoded_ruri.s)
			shm_free((*temp_special_handling)->hardcoded_ruri.s);
		if((*temp_special_handling)->insrt.s)
			shm_free((*temp_special_handling)->insrt.s);
		shm_free(*temp_special_handling);
		*temp_special_handling = 0;
	}
}

void free_digit_node(digit_node_t *dignode)
{
	int idx;
	for(idx = 0; idx < NUM_DIGITS; idx++) {
		if(dignode[idx].service) {

			if(dignode[idx].service->special_handling)
				deleteSpecialHandling(&dignode[idx].service->special_handling);

			shm_free(dignode[idx].service->as_name.s);
			shm_free(dignode[idx].service->session_type.s);
			shm_free(dignode[idx].service);
			// TODO: what about the special handling???
		}

		if(dignode[idx].next_level)
			free_digit_node(dignode[idx].next_level);
	}
}
static void mod_destroy(void)
{
	int flix = sizeof(rdn_first_levels) / sizeof(digit_node_t *);

	while((--flix >= 0))
		if(rdn_first_levels[flix])
			free_digit_node(rdn_first_levels[flix]);

	if(unreg_term_template.s)
		shm_free(unreg_term_template.s);

	LM_INFO("ims_rdn module destroyed\n");
}


//
// 4.b) Exporting the functions
//

static cmd_export_t cmds[] = {
		{"prepare_term_unreg", (cmd_function)prepare_term_unreg, 1,
				domain_fixup, 0, REQUEST_ROUTE},
		{"analyse_eirene_ruri", (cmd_function)analyse_eirene_ruri, 0, 0, 0,
				REQUEST_ROUTE},
		{"analyse_e164_ruri", (cmd_function)analyse_e164_ruri, 0, 0, 0,
				REQUEST_ROUTE},
		{"analyse_dpsi_ruri", (cmd_function)analyse_dpsi_ruri, 0, 0, 0,
				REQUEST_ROUTE},
		{"analyse_domain_ruri", (cmd_function)analyse_domain_ruri, 0, 0, 0,
				REQUEST_ROUTE},
		{"analyse_e164_pvar", (cmd_function)analyse_e164_pvar, 1,
				fixup_pvar_null, 0, ANY_ROUTE},
		{"analyse_eirene_pvar", (cmd_function)analyse_eirene_pvar, 1,
				fixup_pvar_null, 0, ANY_ROUTE},
		{"analyse_dpsi_pvar", (cmd_function)analyse_dpsi_pvar, 1,
				fixup_pvar_null, 0, ANY_ROUTE},
		{"analyse_domain_pvar", (cmd_function)analyse_domain_pvar, 1,
				fixup_pvar_null, 0, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

//
// 4.c) Exporting the module parameters
//

static param_export_t params[] = {{"list_file", PARAM_STRING, &rdnlistfile},
		{"dpsi_file", PARAM_STRING, &rdnlistfile_dpsi},
		{"cx_userdata_templates", PARAM_STRING, &cx_userdata_templates},
		{"rdn_node_type", INT_PARAM, &rdn_node_type}, {0, 0, 0}};

//
// 4.d) General export structure of the module
//

struct module_exports exports = {
		"ims_rdn",
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* Exported functions */
		params,			 /* Exported parameters */
		0,				 /* exported RPC functions */
		0,				 /* exported pseudo-variables */
		0,				 /* response function */
		mod_init,		 /* module initialization function */
		0,
		mod_destroy, /* destroy function */
};

//
// 5) Utilities for the handling of the RDN Digit Tree and of the DPSI Table
//

//
// 5.a) Utilities for the special treatment of services (destination is MCx AS)
//

special_handling_t *createSpecialHandling(special_handling_t *temp)
{
	int err = 0;
	special_handling_t *new_sh = 0;
	if(temp) {
		new_sh = (special_handling_t *)shm_malloc(sizeof(special_handling_t));
		if(new_sh) {
			str nilstr = {0, 0};
			new_sh->mcx_domain = nilstr;
			new_sh->iwf_domain = nilstr;
			new_sh->hardcoded_ruri = nilstr;
			new_sh->del = 0;
			new_sh->insrt = nilstr;
			if(temp->mcx_domain.s && temp->mcx_domain.len) {
				new_sh->mcx_domain.s = (char *)shm_malloc(
						(temp->mcx_domain.len) * sizeof(char));
				if(new_sh->mcx_domain.s) {
					new_sh->mcx_domain.len = temp->mcx_domain.len;
					strncpy(new_sh->mcx_domain.s, temp->mcx_domain.s,
							temp->mcx_domain.len);
				} else
					err = 1;
			}
			if(temp->iwf_domain.s && temp->iwf_domain.len) {
				new_sh->iwf_domain.s = (char *)shm_malloc(
						(temp->iwf_domain.len) * sizeof(char));
				if(new_sh->iwf_domain.s) {
					new_sh->iwf_domain.len = temp->iwf_domain.len;
					strncpy(new_sh->iwf_domain.s, temp->iwf_domain.s,
							temp->iwf_domain.len);
				} else
					err = 1;
			}
			if(temp->hardcoded_ruri.s && temp->hardcoded_ruri.len) {
				new_sh->hardcoded_ruri.s = (char *)shm_malloc(
						(temp->hardcoded_ruri.len) * sizeof(char));
				if(new_sh->hardcoded_ruri.s) {
					new_sh->hardcoded_ruri.len = temp->hardcoded_ruri.len;
					strncpy(new_sh->hardcoded_ruri.s, temp->hardcoded_ruri.s,
							temp->hardcoded_ruri.len);
				} else
					err = 1;
			}
			new_sh->del = temp->del;
			if(temp->insrt.s && temp->insrt.len) {
				new_sh->insrt.s =
						(char *)shm_malloc((temp->insrt.len) * sizeof(char));
				if(new_sh->insrt.s) {
					new_sh->insrt.len = temp->insrt.len;
					strncpy(new_sh->insrt.s, temp->insrt.s, temp->insrt.len);
				} else
					err = 1;
			}
			if(err) {
				shm_free(new_sh->mcx_domain.s);
				shm_free(new_sh->iwf_domain.s);
				shm_free(new_sh->hardcoded_ruri.s);
				shm_free(new_sh->insrt.s);
				shm_free(new_sh);
				new_sh = 0;
			}
		} else {
			err = 1;
		}
		if(err)
			LM_ERR("Cannot allocate memory for special handling");
	}
	return new_sh;
}


//
// 5.b) General handling of the RDN Digit Tree
//

static int lookup_userix(char *user_parm)
{
	int max_users = sizeof(rdn_user) / sizeof(char *);

	int res = sizeof(rdn_first_levels) / sizeof(digit_node_t *);
	if(res > max_users)
		res = max_users;

	while((--res >= 0) && strncasecmp(user_parm, rdn_user[res], 20))
		;

	// return -1, if user not found, otherwise rdn_user[res] and rdn_first_levels[res] can be used
	return res;
}


rdn_service_t *createService(int scscf_flag, str as_name, str session_type,
		special_handling_t *special_handling)
{
	rdn_service_t *new_service =
			(rdn_service_t *)shm_malloc(sizeof(rdn_service_t));
	if(new_service) {
		new_service->scscf_flag = scscf_flag;
		new_service->as_name.s =
				(char *)shm_malloc((as_name.len) * sizeof(char));
		new_service->session_type.s =
				(char *)shm_malloc((session_type.len) * sizeof(char));
		new_service->special_handling = createSpecialHandling(special_handling);
		if(!new_service->as_name.s || !new_service->session_type.s
				|| (special_handling && !new_service->special_handling)) {
			shm_free(new_service->as_name.s);
			shm_free(new_service->session_type.s);
			shm_free(new_service);
			deleteSpecialHandling(&(new_service->special_handling));
			new_service = 0;
		} else {
			new_service->as_name.len = as_name.len;
			strncpy(new_service->as_name.s, as_name.s, as_name.len);
			new_service->session_type.len = session_type.len;
			strncpy(new_service->session_type.s, session_type.s,
					session_type.len);
		}
	}
	return new_service;
}

digit_node_t *createLevel(void)
{
	digit_node_t *new_dignode =
			(digit_node_t *)shm_malloc(NUM_DIGITS * sizeof(digit_node_t));
	if(new_dignode) {
		int idx;
		for(idx = 0; idx < NUM_DIGITS; idx++) {
			new_dignode[idx].next_level = 0;
			new_dignode[idx].service = 0;
		}
	}
	return new_dignode;
}

//
// 5.c) General handling of the DPSI Table
//

distinct_psi_t *createDp(str user_part, int scscf_flag, str as_name,
		str session_type, special_handling_t *special_handling)
{
	distinct_psi_t *new_dp =
			(distinct_psi_t *)shm_malloc(sizeof(distinct_psi_t));

	if(new_dp) {
		new_dp->key.s = (char *)shm_malloc((user_part.len) * sizeof(char));
		new_dp->service = createService(
				scscf_flag, as_name, session_type, special_handling);
		if(!(new_dp->service) || !(new_dp->key.s)) {
			if(new_dp->service) {
				shm_free(new_dp->service->as_name.s);
				shm_free(new_dp->service->session_type.s);
				shm_free(new_dp->service);
				deleteSpecialHandling(&(new_dp->service->special_handling));
				new_dp->service = 0;
			}
			shm_free(new_dp->key.s);
			new_dp = 0;
			LM_ERR("could not allocate memory for part of distinct PSI");
		} else {
			new_dp->key.len = user_part.len;
			strncpy(new_dp->key.s, user_part.s, user_part.len);
		}
	} else
		LM_ERR("could not allocate memory for distinct PSI / domain based PSI");
	return new_dp;
}

//
// 5.d) Main entry point for search of a service in the
//      DPSI table (based either on the user part of the
//      R-URI or on the domain part and METHOD of the R-URI)
//

//rdn_service_t* find_service_from_user_part(str* user_part){
rdn_service_t *find_service_from_key(str *search_key)
{
	if(dpsi_table) {
		struct hash_entry *np;

		for(np = dpsi_table[calc_hash(search_key)]; np != NULL; np = np->next) {
			if((np->key.len == search_key->len)
					&& (strncmp(np->key.s, search_key->s, search_key->len)
							== 0)) {
				return np->service;
			}
		}
	}
	return 0;
}

//
// 5.e) Reading the config files
//

void add_dp_2_config(str add_key, int scscf_flag, str as_name, str session_type,
		special_handling_t *special_handling)
{
	// only add service, if not already added
	if(dpsi_table) {
		if(!key_exists(&add_key)) {
			distinct_psi_t *new_dp = 0;
			new_dp = createDp(add_key, scscf_flag, as_name, session_type,
					special_handling);
			if(new_dp) {
				struct hash_entry *e;
				unsigned int slot;
				LM_DBG("adding PSI (distinct or domain based): service in "
					   "dpsi_table: key=.%.*s. as=.%.*s. si=.%.*s.",
						new_dp->key.len, new_dp->key.s,
						new_dp->service->as_name.len,
						new_dp->service->as_name.s,
						new_dp->service->session_type.len,
						new_dp->service->session_type.s);
				e = new_hash_entry(&new_dp->key, new_dp->service);
				if(!e)
					LM_ERR("could not create hash entry for PSI");
				else {
					slot = calc_hash(&new_dp->key);
					e->next = dpsi_table[slot];
					dpsi_table[slot] = e;
					LM_DBG("adding PSI (distinct or domain based): using slot "
						   "in hash table: %i",
							slot);
				}
			} else
				LM_ERR("Could not create PSI!");
		} else
			LM_INFO("inconsistency in config: cannot add PSI, key already "
					"used");
	} else
		LM_ERR("dpsi_table not allocated: cannot add PSI");
}

void add_service_2_config(str digits, int scscf_flag, str as_name,
		str session_type, special_handling_t *special_handling, int flix)
{

	int counter = 0;
	digit_node_t *level;

	LM_DBG("adding service to RDN digit tree (user=%s): as=%s st=%s",
			rdn_user[flix], as_name.s, session_type.s);
	if(!rdn_first_levels[flix])
		rdn_first_levels[flix] = createLevel();

	level = rdn_first_levels[flix];

	while(*digits.s && level && counter != digits.len) {
		LM_DBG("when adding RDN Service (user=%s): working on character \'%c\'",
				rdn_user[flix], *digits.s);
		short digval = DIGIT_2_SHORT(*digits.s);
		digits.s++;
		counter++;
		LM_DBG("when adding RDN Service (user=%s): working on value %i",
				rdn_user[flix], digval);
		if(*digits.s && counter != digits.len)
			if(level[digval].next_level)
				level = level[digval].next_level;
			else
				level = level[digval].next_level = createLevel();
		else {
			if((level[digval].service = createService(
						scscf_flag, as_name, session_type, special_handling)))
				LM_DBG("adding RDN Service (user=%s) in dignode: as=.%.*s. "
					   "st=.%.*s. sh->mcxd=.%.*s. sh->iwfd=.%.*s. "
					   "sh->hcr=.%.*s. sh->del=.%d. sh->insrt=.%.*s.",
						rdn_user[flix], level[digval].service->as_name.len,
						level[digval].service->as_name.s,
						level[digval].service->session_type.len,
						level[digval].service->session_type.s,
						level[digval].service->special_handling
								? level[digval]
										  .service->special_handling->mcx_domain
										  .len
								: 0,
						level[digval].service->special_handling
								? level[digval]
										  .service->special_handling->mcx_domain
										  .s
								: 0,
						level[digval].service->special_handling
								? level[digval]
										  .service->special_handling->iwf_domain
										  .len
								: 0,
						level[digval].service->special_handling
								? level[digval]
										  .service->special_handling->iwf_domain
										  .s
								: 0,
						level[digval].service->special_handling
								? level[digval]
										  .service->special_handling
										  ->hardcoded_ruri.len
								: 0,
						level[digval].service->special_handling
								? level[digval]
										  .service->special_handling
										  ->hardcoded_ruri.s
								: 0,
						level[digval].service->special_handling
								? level[digval].service->special_handling->del
								: 0,
						level[digval].service->special_handling
								? level[digval]
										  .service->special_handling->insrt.len
								: 0,
						level[digval].service->special_handling
								? level[digval]
										  .service->special_handling->insrt.s
								: 0);
			else
				LM_ERR("Could not create EIRENE service!");
		}
	}
}
/*! \brief load ims_rdn or distinct_psi data from file */
int load_ims_rdn_config(char *lfile, config_type config)
{
	char line[256], *p;
	FILE *f = NULL;
	str as_name;
	str digits;
	str act_digit;
	str session_type;
	short digval;
	struct sip_uri parsed_uri;
	special_handling_t special_handling;
	int scscf_flag;
	int sh_present;
	int e164_present;
	int is_first_digit;

	if(lfile == NULL || strlen(lfile) <= 0) {
		LM_ERR("bad list file\n");
		return -1;
	}

	f = fopen(lfile, "r");
	if(f == NULL) {
		LM_ERR("can't open list file [%s]\n", lfile);
		return -1;
	}


	p = fgets(line, 256, f);
	while(p) {
		str nilstr = {0, 0};
		// initialize special handling
		special_handling.hardcoded_ruri = nilstr;
		special_handling.del = 0;
		special_handling.insrt = nilstr;
		special_handling.mcx_domain = nilstr;
		special_handling.iwf_domain = nilstr;
		sh_present = 0;

		// initialize E.164 handling
		e164_present = 0;
		is_first_digit = 1;

		// initialize SCSCF flag
		scscf_flag = 0;

		/* eat all white spaces */
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;
		if(*p == '\0' || *p == '#')
			goto next_line;

		if(config == dpsi_config) {
			/* get set user part */
			digits.s = p;
			while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'
					&& *p != '#' && *p != '@')
				p++;
			if(*p == '@') {
				/* It is a Domain based PSI like "MESSAGE@klab4800". */
				/* Read now the Domain part beginning with the "@". */
				while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'
						&& *p != '#')
					p++;
				/* get key for Domain based PSI (METHOD@domain) */
				digits.len = p - digits.s;
			} else {
				/* get key for Distinct PSI (only user part) */
				digits.len = p - digits.s;
			}

		} else {

			/* get set digits */
			digits.s = p;
			act_digit.s = p;
			while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'
					&& *p != '#') {
				if((digval = DIGIT_2_SHORT(*act_digit.s)) < 0) {
					if(is_first_digit && *p == '+') {
						e164_present = 1;
						p++;
						digits.s = p;
						act_digit.s = p;
						goto skipdig;
					} else {
						LM_ERR("Invalid digits parameter, only digits 0 1 2 3 "
							   "4 5 6 7 8 9 a b c d e f A B C D E F allowed");
						p++;
						LM_ERR("Suspect digits:  %.*s", (int)(p - digits.s),
								digits.s);
						goto error;
					}
				}
				p++;
				act_digit.s = p;
			skipdig:
				is_first_digit = 0;
			}
			digits.len = p - digits.s;
		}


		/* eat all white spaces */
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;
		if(*p == '\0' || *p == '#') {
			LM_ERR("bad line [%s]\n", line);
			goto error;
		}

		/* get scscf_flag */
		if(*p == '0' || *p == '1') {
			scscf_flag = (*p++) - '0';

			while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'
					&& *p != '#')
				p++;
		}
		LM_DBG("*******EXPERIMENTAL LOG: scscf_flag %d\n", scscf_flag);

		/* eat all white spaces */
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;
		if(*p == '\0' || *p == '#') {
			LM_ERR("bad line [%s]\n", line);
			goto error;
		}

		/* get as_name */
		as_name.s = p;
		while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'
				&& *p != '#')
			p++;
		as_name.len = p - as_name.s;
		if(as_name.len == 0
				|| parse_uri(as_name.s, as_name.len, &parsed_uri) < 0) {
			LM_ERR("Invalid as_name parameter, must be valid sip uri");
			LM_ERR("Suspect as_name:  %.*s", as_name.len, as_name.s);
			goto error;
		}

		/* eat all white spaces */
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;

		/* get session_type */
		session_type.s = p;
		while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'
				&& *p != '#')
			p++;
		session_type.len = p - session_type.s;

		if(config == rdn_config || config == dpsi_config) {
			/* eat all white spaces */
			while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
				p++;

			if(*p != '\0' && *p != '#') {
				/* special handling exists */
				sh_present = 1;

				/* ---- Multi MCx Domain Support ---- */
				if(rdn_node_type & RDN_NODE_TYPE_MULTI_MC_MASK) {

					/* get MCx Domain */
					special_handling.mcx_domain.s = p;

					while(*p && *p != ' ' && *p != '\t' && *p != '\r'
							&& *p != '\n' && *p != '#')
						p++;

					special_handling.mcx_domain.len =
							p - special_handling.mcx_domain.s;

					/* eat all white spaces */
					while(*p
							&& (*p == ' ' || *p == '\t' || *p == '\r'
									|| *p == '\n'))
						p++;

					if(*p != '\0' && *p != '#') {
						/* get IWF Domain */
						special_handling.iwf_domain.s = p;

						while(*p && *p != ' ' && *p != '\t' && *p != '\r'
								&& *p != '\n' && *p != '#')
							p++;
						special_handling.iwf_domain.len =
								p - special_handling.iwf_domain.s;
					}
				}

				/* eat all white spaces */
				while(*p
						&& (*p == ' ' || *p == '\t' || *p == '\r'
								|| *p == '\n'))
					p++;

				if(*p != '\0' && *p != '#') {

					/* get hardcoded RURI */
					special_handling.hardcoded_ruri.s = p;
					while(*p && *p != ' ' && *p != '\t' && *p != '\r'
							&& *p != '\n' && *p != '#')
						p++;
					special_handling.hardcoded_ruri.len =
							p - special_handling.hardcoded_ruri.s;

					/* eat all white spaces */
					while(*p
							&& (*p == ' ' || *p == '\t' || *p == '\r'
									|| *p == '\n'))
						p++;

					/* get DEL */
					special_handling.del = 0;
					while(*p && *p >= '0' && *p <= '9')
						special_handling.del =
								10 * special_handling.del + (*(p++) - '0');

					/* eat all white spaces */
					while(*p
							&& (*p == ' ' || *p == '\t' || *p == '\r'
									|| *p == '\n'))
						p++;

					if(*p != '\0' && *p != '#') {
						/* get INSRT digits */
						special_handling.insrt.s = act_digit.s = p;
						while(*p && *p != ' ' && *p != '\t' && *p != '\r'
								&& *p != '\n' && *p != '#') {
							if((digval = DIGIT_2_SHORT(*act_digit.s)) < 0) {
								LM_ERR("Invalid digits parameter, only digits "
									   "0 1 2 3 4 5 6 7 8 9 a b c d e f A B C "
									   "D E F allowed");
								p++;
								LM_ERR("Suspect digits:  %.*s",
										(int)(p - special_handling.insrt.s),
										special_handling.insrt.s);
								goto error;
							}
							p++;
							act_digit.s = p;
						}
						special_handling.insrt.len =
								p - special_handling.insrt.s;
					}
					LM_INFO("adding special handling mcx_domain=.%.*s. "
							"iwf_domain=.%.*s. hardcoded_ruri=.%.*s. del=%d "
							"insrt=.%.*s.",
							special_handling.mcx_domain.len,
							special_handling.mcx_domain.s,
							special_handling.iwf_domain.len,
							special_handling.iwf_domain.s,
							special_handling.hardcoded_ruri.len,
							special_handling.hardcoded_ruri.s,
							special_handling.del, special_handling.insrt.len,
							special_handling.insrt.s);
				} else {
					LM_ERR("Cannot add RDN service. Special handling needs "
						   "hardcoded RURI(%.*s)",
							digits.len, digits.s);
					goto error;
				}
			}

			// copy special handling to global:
			if(special_handling_global.mcx_domain.s) {
				// free old memory to prevent leaks
				shm_free(special_handling_global.mcx_domain.s);
				special_handling_global.mcx_domain.s = NULL;
			}

			if(special_handling.mcx_domain.len > 0) {
				// Allocate enough space (including null terminator if you need a c-string)
				special_handling_global.mcx_domain.s =
						(char *)shm_malloc(special_handling.mcx_domain.len + 1);

				if(!special_handling_global.mcx_domain.s) {
					LM_ERR("Unable to allocate shared memory for mcx_domain");
					special_handling_global.mcx_domain.len = 0;
				} else {
					// Copy the data and null-terminate
					memcpy(special_handling_global.mcx_domain.s,
							special_handling.mcx_domain.s,
							special_handling.mcx_domain.len);
					special_handling_global.mcx_domain
							.s[special_handling.mcx_domain.len] = '\0';
					special_handling_global.mcx_domain.len =
							special_handling.mcx_domain.len;
				}
			} else {
				special_handling_global.mcx_domain.s = NULL;
				special_handling_global.mcx_domain.len = 0;
			}

			// same approach for iwf_domain:
			if(special_handling_global.iwf_domain.s) {
				shm_free(special_handling_global.iwf_domain.s);
				special_handling_global.iwf_domain.s = NULL;
			}

			if(special_handling.iwf_domain.len > 0) {
				special_handling_global.iwf_domain.s =
						(char *)shm_malloc(special_handling.iwf_domain.len + 1);

				if(!special_handling_global.iwf_domain.s) {
					LM_ERR("Unable to allocate shared memory for iwf_domain");
					special_handling_global.iwf_domain.len = 0;
				} else {
					memcpy(special_handling_global.iwf_domain.s,
							special_handling.iwf_domain.s,
							special_handling.iwf_domain.len);
					special_handling_global.iwf_domain
							.s[special_handling.iwf_domain.len] = '\0';
					special_handling_global.iwf_domain.len =
							special_handling.iwf_domain.len;
				}
			} else {
				special_handling_global.iwf_domain.s = NULL;
				special_handling_global.iwf_domain.len = 0;
			}


			if((scscf_flag == 0
					   && (rdn_node_type & RDN_NODE_TYPE_MASK)
								  == RDN_NODE_TYPE_SCSCF)
					|| (scscf_flag == 1
							&& (rdn_node_type & RDN_NODE_TYPE_MASK)
									   == RDN_NODE_TYPE_ICSCF)) {
				if(config == dpsi_config) {
					add_dp_2_config(digits, scscf_flag, as_name, session_type,
							sh_present ? &special_handling : 0);
				} else {
					int flix = lookup_userix(
							e164_present ? USER_PARAM_E164 : USER_PARAM_EIRENE);
					if(flix < 0) {
						LM_ERR("Cannot add RDN service. Digit Tree not "
							   "prepared for user=%s",
								e164_present ? USER_PARAM_E164
											 : USER_PARAM_EIRENE);
						goto error;
					}
					add_service_2_config(digits, scscf_flag, as_name,
							session_type, sh_present ? &special_handling : 0,
							flix); // special handling may exist
				}
			} else {
				LM_ERR("SCSCF Flag does not fit environment (0 = SCSCF, 1 = "
					   "ICSCF). ");
				goto error;
			}
		}
	next_line:
		p = fgets(line, 256, f);
	}


	fclose(f);
	f = NULL;
	return 0;

error:
	if(f != NULL)
		fclose(f);
	return -1;
}

static int load_cx_userdata_template(FILE *f)
{

	int size = 0, rc = -1;

	while(fgetc(f) != EOF)
		size++;

	if(fseek(f, 0, SEEK_SET))
		goto finished;

	unreg_term_template.s = shm_malloc((size + 1) * sizeof(unsigned char));
	if(unreg_term_template.s) {
		int ch;
		while((unreg_term_template.len < size) && ((ch = fgetc(f)) != EOF))
			unreg_term_template.s[unreg_term_template.len++] =
					(unsigned char)ch;
		if(ch != EOF && fgetc(f) != EOF)
			LM_INFO("problem reading file for Cx Userdata Template\n");
		else {
			unreg_term_template.s[unreg_term_template.len] = '\0';
			rc = 0;
		}
	} else
		LM_INFO("out of memory, when allocating SHM for Cx Userdata "
				"Template\n");

finished:
	if(rc < 0 && unreg_term_template.s) {
		shm_free(unreg_term_template.s);
		unreg_term_template.s = 0;
		unreg_term_template.len = 0;
	}
	return rc;
}

//
// 6) Loading the template file for the XML dummy IMS subscription (quasi Cx)
//

static int load_template(char *tfile, template_type config)
{

	FILE *f = NULL;
	int rc = -1;

	if(tfile == NULL || strlen(tfile) <= 0) {
		LM_ERR("bad template file\n");
		goto finished;
	}

	f = fopen(tfile, "r");
	if(f == NULL) {
		LM_ERR("can't open template file [%s]\n", tfile);
		goto finished;
	}

	switch(config) {
		case cx_userdata_template:
			rc = load_cx_userdata_template(f);
			break;
		default:
			LM_INFO("internal software error: unknown template type [%s]\n",
					tfile);
			goto finished;
	}

finished:
	if(f != NULL)
		fclose(f);
	f = NULL;
	if(rc < 0)
		LM_ERR("bad rc\n");
	return rc;
}

//
// 7) module startup
//

static int mod_init(void)
{
	bind_usrloc_t bind_usrloc;

	// check preconditions

	// bind usrloc API

	bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);

	if(!bind_usrloc) {
		if(!rdn_node_type) {
			LM_ERR("can't bind usrloc\n");
			return -1;
		}
	} else {

		if(bind_usrloc(&ul) < 0) {
			LM_ERR("can't bind usrloc ul\n");
			return -1;
		}
	}

	// load the TM API

	if(load_tm_api(&tmb) != 0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}
	// %KTAVICT% - BEGIN - improve_registrar_api
	if((rdn_node_type & RDN_NODE_TYPE_MASK) == RDN_NODE_TYPE_SCSCF) {
		if(registrar_load_api(&ra) != 0) {
			LM_ERR("can't load registrar API\n");
			return -1;
		}
	}
	// %KTAVICT% - END - improve_registrar_api

	if(allocate_dpsi_table() < 0) {
		return -1;
	}

	// load the config files

	if(load_ims_rdn_config(rdnlistfile, rdn_config) != 0) {
		LM_ERR("no ims_rdn config loaded from file\n");
		return -1;
	} else {
		LM_INFO("successfully loaded ims_rdn config\n");
	}

	if(load_ims_rdn_config(rdnlistfile_dpsi, dpsi_config) != 0) {
		LM_ERR("no distinct_psi config loaded from file\n");
		return -1;
	} else {
		LM_INFO("successfully loaded distinct_psi config\n");
	}

	// load the CX UserData template file

	// PR-0006479: DG: 05.06.2024: ICSCF requires userdata template (but is not needed) -> added check if i_am_at_icscf is false
	if((rdn_node_type & RDN_NODE_TYPE_MASK) == RDN_NODE_TYPE_SCSCF) {

		if(load_template(cx_userdata_templates, cx_userdata_template) != 0) {
			LM_ERR("no cx userdata template loaded from file\n");
			return -1;
		} else {
			LM_INFO("successfully loaded cx userdata template for unreg/term "
					"delivery\n");
		}
	}

	str s_as_name = str_init("$var(rdn_as_name)");
	if(pv_parse_spec(&s_as_name, &rdn_as_name_pv) == NULL
			|| !pv_is_w(&rdn_as_name_pv)) {
		LM_ERR("Failed to parse rdn_as_name_pv\n");
		return -1;
	}

	str s_scscf_name = str_init("$var(rdn_scscf_name)");
	if(pv_parse_spec(&s_scscf_name, &rdn_scscf_name_pv) == NULL
			|| !pv_is_w(&rdn_scscf_name_pv)) {
		LM_ERR("Failed to parse rdn_scscf_name_pv\n");
		return -1;
	}

	str s_sessiontype = str_init("$var(rdn_sessiontype)");
	if(pv_parse_spec(&s_sessiontype, &rdn_sessiontype_pv) == NULL
			|| !pv_is_w(&rdn_sessiontype_pv)) {
		LM_ERR("Failed to parse rdn_sessiontype_pv\n");
		return -1;
	}

	str s_iwf_domain = str_init("$var(rdn_iwf_domain)");
	if(pv_parse_spec(&s_iwf_domain, &rdn_iwf_domain_pv) == NULL
			|| !pv_is_w(&rdn_iwf_domain_pv)) {
		LM_ERR("Failed to parse rdn_iwf_domain_pv\n");
		return -1;
	}

	str s_hardcoded_ruri = str_init("$var(rdn_hardcoded_ruri)");
	if(pv_parse_spec(&s_hardcoded_ruri, &rdn_hardcoded_ruri_pv) == NULL
			|| !pv_is_w(&rdn_hardcoded_ruri_pv)) {
		LM_ERR("Failed to parse rdn_hardcoded_ruri_pv\n");
		return -1;
	}

	str s_insrt = str_init("$var(rdn_insrt)");
	if(pv_parse_spec(&s_insrt, &rdn_insrt_pv) == NULL
			|| !pv_is_w(&rdn_insrt_pv)) {
		LM_ERR("Failed to parse rdn_insrt_pv\n");
		return -1;
	}

	str s_mcx_domain_avp = str_init("$var(rdn_mcx_domain)");
	if(pv_parse_spec(&s_mcx_domain_avp, &rdn_mcx_domain_avp) == NULL
			|| !pv_is_w(&rdn_mcx_domain_avp)) {
		LM_ERR("Failed to parse rdn_mcx_domain_avp\n");
		return -1;
	}

	str s_del = str_init("$var(rdn_del)");
	if(pv_parse_spec(&s_del, &rdn_del_pv) == NULL || !pv_is_w(&rdn_del_pv)) {
		LM_ERR("Failed to parse rdn_del_pv\n");
		return -1;
	}

	return 0;
};

//
// 8) Several Utilities
//

void extract_digit_block(
		char *pChar, int iChar, int remaining, short **pDigblk, int *pLen)
{
	// recursive function to extract a digit block from a string
	// Parameters
	//    char*   pChar ......points to the first character in the string that shall
	//                        be analyzed; MUST NOT be NULL
	//    int     iChar ......pChar[iChar] is the currently processed character
	//    int     remaining...pChar+remaining points to the memory right behind the string
	//    short** pDigBlk.....on return from recursion *pDigblk holds the pointer
	//                        to the allocated digit block
	//    int*    pLen........*pLen is counted up, until the end of the block is
	//                        reached, on return from recursion it holds the length
	//                        of the digit block
	//assert(pChar);  // darf man im kamailio assert verwenden?
	//assert(iChar >= 0);
	//assert(remaining >= iChar);
	//assert(pDigblk);
	//assert(pLen);

	short digval; // value of the current digit
	if(iChar >= remaining || (digval = DIGIT_2_SHORT(pChar[iChar])) < 0) {
		// no characters left or not a digit -> stop recursion and allocate digit block
		if(*pLen)
			(*pDigblk) = (short *)malloc(sizeof(short) * (*pLen));
	} else {
		// it's a valid digit and we are not behind the last character
		(*pLen)++;
		extract_digit_block(pChar, iChar + 1, remaining, pDigblk, pLen);
		// on return from recursion store value of digit (if memory allocated)
		if(*pDigblk)
			(*pDigblk)[iChar] = digval;
	}
}

// recursive function to find an RDN service in the digit tree
// Parameters
//    digit_node_t* rdn_level......points to the level that shall be processes
//                                 MUST NOT be NULL
//    short*        pDigit.........points to the current digit (value)
//                                 MUST NOT be NULL
//    int           remaining......pDigit[remaining] points to the memory right
//                                 behind the digit block. MUST BE >= 0
rdn_service_t *find_rdn_service(
		digit_node_t *rdn_level, short *pDigit, int remaining)
{
	digit_node_t *dignode = rdn_level + *pDigit; // pointer to DIGIT node
	digit_node_t *next_level = dignode->next_level;
	if(next_level && --remaining) {
		rdn_service_t *service =
				find_rdn_service(next_level, pDigit + 1, remaining);
		if(service) {
			LM_DBG("[EL-KW] Found service for child digit node <%i>", *pDigit);
			return service;
		}
	}

	if(dignode->service)
		LM_INFO("found E.164/EIRENE service in RDN digit tree: as=.%.*s. "
				"si=.%.*s.",
				dignode->service->as_name.len, dignode->service->as_name.s,
				dignode->service->session_type.len,
				dignode->service->session_type.s);
	else
		LM_INFO("found NIL service");
	return dignode->service;
}

rdn_service_t *find_service_from_digits(
		char *pChar, int remaining, char *user_param)
{
	// function to find a service from digits in a string
	// Parameters
	//    char*  pChar ......points to the first character in the string that shall
	//                       be analyzed; MUST NOT be NULL
	//    int    remaining ..pChar + remaining points to the memory right behind the
	//                       string; MUST BE >= 0

	//assert(pChar);  // darf man im kamailio assert verwenden?
	//assert(remaining >= 0);

	// we will extract a block of contiguous digits (char) from the string and
	// store their values (short) on the heap
	short *digblk = NULL;
	int digblklen = 0;
	int flix = lookup_userix(user_param);
	int count = remaining;

	// we will search for a service in the digit tree
	rdn_service_t *service = 0;

	if(flix < 0 || !rdn_first_levels[flix]) {
		LM_INFO("rdn_first_levels[flix(user_param)] is 0 (no valid config "
				"found)");
		return 0;
	}

	// eat leading non-digits
	while(DIGIT_2_SHORT(*pChar) < 0 && count-- > 0)
		pChar++, remaining--;

	if(remaining)
		// extract block of contiguous digits
		extract_digit_block(pChar, 0, remaining, &digblk, &digblklen);

	if(digblk) {
		LM_INFO("found digit block in Request URI: first=%i len=%i", *digblk,
				digblklen);
		// if found block of digits -> analyse and find service in digit tree
		service = find_rdn_service(rdn_first_levels[flix], digblk, digblklen);
	} else
		LM_INFO("after extract_digit_block from Request URI: no digit block "
				"found");

	free(digblk); // free heap memory
	return service;
}

rdn_service_t *find_service_from_ruri(struct sip_uri *uri)
{
	if(uri->user_param_val.len == strlen(USER_PARAM_EIRENE)) {
		if(memcmp(uri->user_param_val.s, USER_PARAM_EIRENE, 4) == 0) {
			// yes, we have "user=gsmr", try digits
			return find_service_from_digits(
					uri->user.s, uri->user.len, USER_PARAM_EIRENE);
		}
	}
	LM_INFO("find_service_from_ruri: user=gsmr missing in Request URI");
	// no user=gsmr -> look for distinct PSI
	return find_service_from_key(&uri->user);
}

rdn_service_t *find_service_from_ruri2(struct sip_uri *uri, char *user_param)
{
	return find_service_from_digits(uri->user.s, uri->user.len, user_param);
}

/*
* This function parses the R-URI if not already done, replaces the
* host part of the URI with @new_domain, and resets msg->new_uri
* so Kamailio rebuilds the request URI on transmission.
*/
#include "../../core/parser/parse_uri.h"
/*
void update_sip_uri_domain(struct sip_msg *msg, str *new_domain)
{
    // 1) Basic checks
    if (!msg || !new_domain || !new_domain->s) {
        LM_ERR("update_sip_uri_domain: invalid parameters\n");
        return;
    }

    // 2) If not parsed yet, parse the R-URI
    if (!msg->parsed_uri_ok) {
        if (parse_sip_msg_uri(msg) < 0) {
            LM_ERR("update_sip_uri_domain: failed to parse R-URI\n");
            return;
        }
    }

    // 3) Access the parsed URI
    struct sip_uri *uri = &msg->parsed_uri.uri;
    if (!uri) {
        LM_ERR("update_sip_uri_domain: no parsed URI\n");
        return;
    }

    // 4) Overwrite the host portion
    uri->host.s   = new_domain->s;
    uri->host.len = new_domain->len;

    // 5) Reset msg->new_uri to force rebuild
    msg->new_uri.s   = NULL;
    msg->new_uri.len = 0;

    LM_INFO("update_sip_uri_domain: changed domain to '%.*s'\n",
            new_domain->len, new_domain->s);
}


//
// 9) functions for the routing config
//
// Updates SIP Routing logic with AVPs
static void modify_routing_logic(struct sip_msg *msg)
{
    // 1) Retrieve your mcx_domain and iwf_domain however you store them
    //    e.g., from a global or from special_handling_t
    str mcx_domain = special_handling_global.mcx_domain;
    str iwf_domain = special_handling_global.iwf_domain;

    // 2) Update the R-URI domain if mcx_domain is set
    if (mcx_domain.s && mcx_domain.len > 0) {
        LM_INFO("modify_routing_logic: rewriting domain to '%.*s'\n",
                mcx_domain.len, mcx_domain.s);
        update_sip_uri_domain(msg, &mcx_domain);
    }

    // 3) Optionally do something with iwf_domain if needed
    if (iwf_domain.s && iwf_domain.len > 0) {
        LM_INFO("modify_routing_logic: rewriting domain to IWF='%.*s'\n",
                iwf_domain.len, iwf_domain.s);
        update_sip_uri_domain(msg, &iwf_domain);
    }
}
*/


int prepare_term_unreg(struct sip_msg *_m, char *_t, char *_s)
{
	struct sip_msg *req;

	char xml_string[4000];
	ims_subscription *s = 0;
	str xml_data = {0, 0}, ccf1 = {0, 0}, ccf2 = {0, 0}, ecf1 = {0, 0},
		ecf2 = {0, 0};
	str impu = {0, 0};
	int success = 0;
	avp_value_t hc_ruri, as_name, sess_type;
	avp_name_t name;

	if((rdn_node_type & RDN_NODE_TYPE_MASK) != RDN_NODE_TYPE_SCSCF) {
		LM_ERR("prepare_term_unreg: only useable at S-CSCF!!!");
		return -1;
	}


	req = _m;
	if(!req) {
		LM_ERR("prepare_term_unreg: NULL message!!!");
		return -1;
	}
	if(req->first_line.type != SIP_REQUEST) {
		LM_ERR("prepare_term_unreg: message is not a request!!!");
		return -1;
	}

	if(parse_sip_msg_uri(req) < 0) {
		LM_ERR("prepare_term_unreg: could not parse request URI");
		return -1;
	}

	name.s.s = RDN_AS_NAME;
	name.s.len = strlen(name.s.s);
	if(!search_first_avp(AVP_VAL_STR | AVP_NAME_STR, name, &as_name, 0)) {
		LM_ERR("prepare_term_unreg: could not find AVP for AS Name");
		return -1;
	}

	name.s.s = RDN_SESSIONTYPE;
	name.s.len = strlen(name.s.s);
	if(!search_first_avp(AVP_VAL_STR | AVP_NAME_STR, name, &sess_type, 0)) {
		LM_ERR("prepare_term_unreg: could not find AVP for session_type");
		return -1;
	}

	name.s.s = RDN_HARDCODED_RURI;
	name.s.len = strlen(name.s.s);
	if(!search_first_avp(AVP_VAL_STR | AVP_NAME_STR, name, &hc_ruri, 0)) {
		LM_ERR("prepare_term_unreg: could not find AVP for hardcoded R-URI");
		return -1;
	}

	if(cscf_get_terminating_user(req, &impu)) {
		if(hc_ruri.s.len && (*hc_ruri.s.s) != '*') {
			LM_INFO("we dont use impu=.%.*s. but replace it by hardcoded RURI "
					".%.*s.",
					impu.len, impu.s, hc_ruri.s.len, hc_ruri.s.s);
			if(find_not_quoted(&(hc_ruri.s), '@'))
				sprintf(xml_string,
                  unreg_term_template.s /*xml_template*/,
                  hc_ruri.s.len,
                  hc_ruri.s.s,
                  as_name.s.len,
                  as_name.s.s /*,
                  sess_type.len,
                  sess_type.s */   );
			else {
				// %KTAVICT%-BEGIN devel_PR-0003370
				xml_string[0] = 0;
				// %KTAVICT%-END devel_PR-0003370
			}
		} else {
			sprintf(xml_string,
                unreg_term_template.s /*xml_template*/,
                impu.len,
                impu.s,
                as_name.s.len,
                as_name.s.s /*   ,
                sess_type.len,
                sess_type.s */   );
		}
		xml_data.len = strlen(xml_string);
		xml_data.s = xml_string;

		if(xml_data.s && xml_data.len > 0) {
			LM_DBG("Parsing user data string from XML config\n");
			// %KTAVICT% - BEGIN - improve_registrar_api
			s = ra.parse_user_data(xml_data);
			// %KTAVICT% - END - improve_registrar_api
			if(!s) {
				LM_ERR("Unable to parse user data XML string\n");
			} else {
				LM_INFO("successfully parse user data XML setting ref to 1 (we "
						"are referencing it) -> terminating service can be "
						"prepared");
				s->ref_count =
						1; //no need to lock as nobody else will be referencing this piece of memory just yet
				success = 1;
			}
		} else if(xml_data.s && xml_data.len == 0) {
			LM_INFO("hardcoded RURI doesnt contain \'@\', its an FQDN -> "
					"result value 2, no DB access");
			success = 2;
		} else
			LM_ERR("Could not copy xml_string to xml_data");
	}
	//here we update the contacts and also build the new contact header for the 200 OK reply
	// %KTAVICT% - BEGIN - improve_registrar_api
	if(success == 1
			&& ra.update_contacts(req, (udomain_t *)_t, &impu,
					   AVP_IMS_SAR_UNREGISTERED_USER, &s, &ccf1, &ccf2, &ecf1,
					   &ecf2, 0)
					   <= 0) {
		// %KTAVICT% - END - improve_registrar_api
		LM_ERR("Error processing data base for initial INVITE for RDN "
			   "Service\n");
		success = 0;
	}

	if(s)
		ul.unref_subscription(s);

	if(impu.s)
		shm_free(impu.s);

	if(success)
		return success;
	else
		return -1;
}

// common part for all analyse_*_ruri() functions
//
int analyse_ruri(
		struct sip_msg *_m, char *_t, char *_s, char *user_param, int mode)
{
	int success = -1;
	struct sip_msg *req;

	rdn_service_t *service = NULL;


	req = _m;
	if(!req) {
		LM_ERR("analyse_xxx_ruri: NULL message!!!");
		return -1;
	}
	if(req->first_line.type != SIP_REQUEST) {
		LM_ERR("analyse_xxx_ruri: not a request!!!");
		return -1;
	}

	if(parse_sip_msg_uri(req) < 0) {
		LM_ERR("analyse_xxx_ruri: could not parse request URI");
		return -1;
	}

	if(mode) {
		if(user_param) {
			int flix = lookup_userix(user_param);
			if(flix < 0) {
				LM_ERR("Internal Error (should not happen): cannot handle "
					   "user_param [%s]",
						user_param);
				return -1;
			}
			service = find_service_from_ruri2(&req->parsed_uri, user_param);
		} else {
			service = find_service_from_key(&req->parsed_uri.user);
		}
	} else {
		char buffer[100];
		str key = {0, 0};
		if(req->first_line.u.request.method.len + req->parsed_uri.host.len
				< 96) {
			key.s = buffer;
			key.len = req->first_line.u.request.method.len
					  + req->parsed_uri.host.len + 1; // '@' as separator
			memcpy(key.s, req->first_line.u.request.method.s,
					req->first_line.u.request.method.len);
			key.s[req->first_line.u.request.method.len] = '@';
			memcpy(key.s + req->first_line.u.request.method.len + 1,
					req->parsed_uri.host.s, req->parsed_uri.host.len);
			service = find_service_from_key(&key);
		} else {
			LM_ERR("Internal buffer too short (100) for method + '@' + domain "
				   "of RURI");
		}
	}

	if(!service) {
		LM_INFO("parse_xxx_ruri: no RDN service found");
		return -1;
	} else {
		success = 1;
		avp_name_t avp_name, avp_name2;
		avp_value_t avp_value;
		avp_name.s.s = RDN_SESSIONTYPE;
		avp_name.s.len = strlen(avp_name.s.s);
		avp_value.s = service->session_type;
		if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value) < 0) {
			LM_ERR("could not create AVP(sessiontype)");
			return -2;
		}
		if(service->scscf_flag) {
			avp_name.s.s = RDN_SCSCF_NAME;
			avp_name2.s.s = RDN_AS_NAME;
		} else {
			avp_name.s.s = RDN_AS_NAME;
			avp_name2.s.s = RDN_SCSCF_NAME;
		}
		avp_name.s.len = strlen(avp_name.s.s);
		avp_value.s = service->as_name;
		if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value) < 0) {
			LM_ERR("could not create AVP(as_name)");
			return -2;
		}
		avp_name2.s.len = strlen(avp_name2.s.s);
		avp_value.s.s = 0;
		avp_value.s.len = 0;
		if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name2, avp_value) < 0) {
			LM_ERR("could not create AVP(as_name)");
			return -2;
		} else {
			if(service->special_handling) {
				success =
						((*(service->special_handling->hardcoded_ruri.s) == '*')
								|| find_not_quoted(
										&(service->special_handling
														->hardcoded_ruri),
										'@'))
								? 1
								: 2;
				avp_name.s.s = RDN_MCX_DOMAIN;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.s = service->special_handling->mcx_domain;
				if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value)
						< 0) {
					LM_ERR("could not create AVP(mcx_domain)");
					return -2;
				}
				avp_name.s.s = RDN_IWF_DOMAIN;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.s = service->special_handling->iwf_domain;
				if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value)
						< 0) {
					LM_ERR("could not create AVP(iwf_domain)");
					return -2;
				}
				avp_name.s.s = RDN_HARDCODED_RURI;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.s = service->special_handling->hardcoded_ruri;
				if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value)
						< 0) {
					LM_ERR("could not create AVP(hardcoded_ruri)");
					return -2;
				}
				avp_name.s.s = RDN_DEL;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.n = service->special_handling->del;
				if(add_avp(AVP_NAME_STR, avp_name, avp_value) < 0) {
					LM_ERR("could not create AVP(del)");
					return -2;
				}
				avp_name.s.s = RDN_INSRT;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.s = service->special_handling->insrt;
				if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value)
						< 0) {
					LM_ERR("could not create AVP(insrt)");
					return -2;
				}
			} else {
				avp_name.s.s = RDN_MCX_DOMAIN;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.s.s = 0;
				avp_value.s.len = 0;
				if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value)
						< 0) {
					LM_ERR("could not create AVP(mcx_domain)");
					return -2;
				}
				avp_name.s.s = RDN_IWF_DOMAIN;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.s.s = 0;
				avp_value.s.len = 0;
				if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value)
						< 0) {
					LM_ERR("could not create AVP(iwf_domain)");
					return -2;
				}
				avp_name.s.s = RDN_HARDCODED_RURI;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.s.s = 0;
				avp_value.s.len = 0;
				if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value)
						< 0) {
					LM_ERR("could not create AVP(hardcoded_ruri)");
					return -2;
				}
				avp_name.s.s = RDN_DEL;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.n = 0;
				if(add_avp(AVP_NAME_STR, avp_name, avp_value) < 0) {
					LM_ERR("could not create AVP(del)");
					return -2;
				}
				avp_name.s.s = RDN_INSRT;
				avp_name.s.len = strlen(avp_name.s.s);
				avp_value.s.s = 0;
				avp_value.s.len = 0;
				if(add_avp(AVP_VAL_STR | AVP_NAME_STR, avp_name, avp_value)
						< 0) {
					LM_ERR("could not create AVP(insrt)");
					return -2;
				}
			}
			LM_INFO("Successful analyse_xxx_ruri()");
			return success;
		}
	}
}

int common_rdn_pvar_analyse(
		str *uri, char *user_param, int mode, rdn_analysis_result_t *result)
{
	rdn_service_t *service = NULL;

	if(!result) {
		LM_ERR("rdn_internal_analyse(): result pointer is NULL\n");
		return -1;
	}

	if(!uri || !uri->s || uri->len == 0) {
		LM_ERR("common_rdn_pvar_analyse(): invalid URI input\n");
		return -1;
	}

	struct sip_uri parsed_uri;
	if(parse_uri(uri->s, uri->len, &parsed_uri) < 0) {
		LM_ERR("common_rdn_pvar_analyse(): failed to parse URI: %.*s\n",
				uri->len, uri->s);
		return -1;
	}

	// Service lookup
	if(mode) {
		if(user_param) {
			int flix = lookup_userix(user_param);
			if(flix < 0) {
				LM_ERR("common_rdn_pvar_analyse(): unknown user_param [%s]",
						user_param);
				return -1;
			}
			service = find_service_from_ruri2(&parsed_uri, user_param);
		} else {
			service = find_service_from_key(&parsed_uri.user);
		}
	} else {
		char buffer[100];
		str key = {0, 0};
		if(parsed_uri.user.len + parsed_uri.host.len < 96) {
			key.s = buffer;
			key.len = parsed_uri.user.len + parsed_uri.host.len + 1;
			memcpy(key.s, parsed_uri.user.s, parsed_uri.user.len);
			key.s[parsed_uri.user.len] = '@';
			memcpy(key.s + parsed_uri.user.len + 1, parsed_uri.host.s,
					parsed_uri.host.len);
			service = find_service_from_key(&key);
		} else {
			LM_ERR("common_rdn_pvar_analyse(): buffer overflow risk for "
				   "method@domain key\n");
			return -1;
		}
	}

	if(!service) {
		LM_INFO("common_rdn_pvar_analyse(): no matching service for URI: "
				"%.*s\n",
				uri->len, uri->s);
		return -1;
	}

	// KORREKTE Zuweisungen:
	result->session_type = service->session_type;
	result->as_name = service->as_name;

	// "Leeren" Kamailio-String setzen:
	result->scscf_name.s = 0;
	result->scscf_name.len = 0;

	result->mcx_domain.s = 0;
	result->mcx_domain.len = 0;

	result->iwf_domain.s = 0;
	result->iwf_domain.len = 0;

	result->hardcoded_ruri.s = 0;
	result->hardcoded_ruri.len = 0;
	result->del = 0;
	result->insrt.s = 0;
	result->insrt.len = 0;

	if(service->special_handling) {
		result->mcx_domain = service->special_handling->mcx_domain;
		result->iwf_domain = service->special_handling->iwf_domain;
		result->hardcoded_ruri = service->special_handling->hardcoded_ruri;
		result->del = service->special_handling->del;
		result->insrt = service->special_handling->insrt;
	}

	LM_INFO("common_rdn_pvar_analyse(): service match successful for [%.*s]\n",
			uri->len, uri->s);
	return 1;
}

int analyse_e164_pvar(struct sip_msg *msg, char *pvar_input, char *unused)
{

	pv_value_t val;
	str uri;
	struct sip_uri parsed_uri;

	pv_spec_t *spec = (pv_spec_t *)pvar_input;

	if(pv_get_spec_value(msg, spec, &val) != 0 || !val.rs.s
			|| val.rs.len <= 0) {
		LM_ERR("analyse_e164_pvar: failed to get value\n");
		return -1;
	}

	uri = val.rs;
	if(parse_uri(uri.s, uri.len, &parsed_uri) < 0) {
		LM_ERR("analyse_e164_pvar: invalid SIP URI: %.*s\n", uri.len, uri.s);
		return -1;
	}

	rdn_analysis_result_t result;
	memset(&result, 0, sizeof(result));

	if(common_rdn_pvar_analyse(&uri, USER_PARAM_E164, 1, &result) < 0) {
		LM_INFO("analyse_e164_pvar: no service found for URI: %.*s\n", uri.len,
				uri.s);
		return -1;
	}

	// TEMP LOG
	LM_DBG("EL-DG: setze as_name='%.*s', scscf_name='%.*s', "
		   "sessiontype='%.*s', iwf_domain='%.*s', hardcoded_ruri='%.*s', "
		   "insrt='%.*s', mcx_domain='%.*s', del=%d\n",
			result.as_name.len, result.as_name.s ? result.as_name.s : "(null)",
			result.scscf_name.len,
			result.scscf_name.s ? result.scscf_name.s : "(null)",
			result.session_type.len,
			result.session_type.s ? result.session_type.s : "(null)",
			result.iwf_domain.len,
			result.iwf_domain.s ? result.iwf_domain.s : "(null)",
			result.hardcoded_ruri.len,
			result.hardcoded_ruri.s ? result.hardcoded_ruri.s : "(null)",
			result.insrt.len, result.insrt.s ? result.insrt.s : "(null)",
			result.mcx_domain.len,
			result.mcx_domain.s ? result.mcx_domain.s : "(null)", result.del);


	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.as_name;
	if(rdn_as_name_pv.setf(msg, &rdn_as_name_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.scscf_name;
	if(rdn_scscf_name_pv.setf(msg, &rdn_scscf_name_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.session_type;
	if(rdn_sessiontype_pv.setf(msg, &rdn_sessiontype_pv.pvp, (int)EQ_T, &val)
			< 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.iwf_domain;
	if(rdn_iwf_domain_pv.setf(msg, &rdn_iwf_domain_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.hardcoded_ruri;
	if(rdn_hardcoded_ruri_pv.setf(
			   msg, &rdn_hardcoded_ruri_pv.pvp, (int)EQ_T, &val)
			< 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.insrt;
	if(rdn_insrt_pv.setf(msg, &rdn_insrt_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;


	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	if(result.mcx_domain.s && result.mcx_domain.len > 0) {
		val.rs = result.mcx_domain;
	} else {
		val.rs.s = "";
		val.rs.len = 0;
	}
	if(rdn_mcx_domain_avp.setf(msg, &rdn_mcx_domain_avp.pvp, (int)EQ_T, &val)
			< 0)
		return -2;


	// Integer für del:
	pv_value_t ival;
	memset(&ival, 0, sizeof(ival));
	ival.flags = PV_VAL_INT | PV_TYPE_INT;
	ival.ri = result.del;
	if(rdn_del_pv.setf(msg, &rdn_del_pv.pvp, (int)EQ_T, &ival) < 0)
		return -2;

	return 1;
}


int analyse_eirene_pvar(struct sip_msg *msg, char *pvar_input, char *unused)
{
	pv_value_t val;
	str uri;
	struct sip_uri parsed_uri;

	pv_spec_t *spec = (pv_spec_t *)pvar_input;


	if(pv_get_spec_value(msg, spec, &val) != 0 || !val.rs.s
			|| val.rs.len <= 0) {
		LM_ERR("analyse_eirene_pvar: failed to get value\n");
		return -1;
	}

	uri = val.rs;
	if(parse_uri(uri.s, uri.len, &parsed_uri) < 0) {
		LM_INFO("analyse_eirene_pvar: invalid SIP URI: %.*s\n", uri.len, uri.s);
		return -1;
	}

	rdn_analysis_result_t result;
	memset(&result, 0, sizeof(result));

	if(common_rdn_pvar_analyse(&uri, USER_PARAM_EIRENE, 1, &result) < 0) {
		LM_INFO("analyse_eirene_pvar: no service found for URI: %.*s\n",
				uri.len, uri.s);
		return -1;
	}

	// TEMP LOG
	LM_DBG("EL-DG: setze as_name='%.*s', scscf_name='%.*s', "
		   "sessiontype='%.*s', iwf_domain='%.*s', hardcoded_ruri='%.*s', "
		   "insrt='%.*s', mcx_domain='%.*s', del=%d\n",
			result.as_name.len, result.as_name.s ? result.as_name.s : "(null)",
			result.scscf_name.len,
			result.scscf_name.s ? result.scscf_name.s : "(null)",
			result.session_type.len,
			result.session_type.s ? result.session_type.s : "(null)",
			result.iwf_domain.len,
			result.iwf_domain.s ? result.iwf_domain.s : "(null)",
			result.hardcoded_ruri.len,
			result.hardcoded_ruri.s ? result.hardcoded_ruri.s : "(null)",
			result.insrt.len, result.insrt.s ? result.insrt.s : "(null)",
			result.mcx_domain.len,
			result.mcx_domain.s ? result.mcx_domain.s : "(null)", result.del);

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.as_name;
	if(rdn_as_name_pv.setf(msg, &rdn_as_name_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.scscf_name;
	if(rdn_scscf_name_pv.setf(msg, &rdn_scscf_name_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.session_type;
	if(rdn_sessiontype_pv.setf(msg, &rdn_sessiontype_pv.pvp, (int)EQ_T, &val)
			< 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.iwf_domain;
	if(rdn_iwf_domain_pv.setf(msg, &rdn_iwf_domain_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.hardcoded_ruri;
	if(rdn_hardcoded_ruri_pv.setf(
			   msg, &rdn_hardcoded_ruri_pv.pvp, (int)EQ_T, &val)
			< 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.insrt;
	if(rdn_insrt_pv.setf(msg, &rdn_insrt_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	if(result.mcx_domain.s && result.mcx_domain.len > 0) {
		val.rs = result.mcx_domain;
	} else {
		val.rs.s = "";
		val.rs.len = 0;
	}
	if(rdn_mcx_domain_avp.setf(msg, &rdn_mcx_domain_avp.pvp, (int)EQ_T, &val)
			< 0)
		return -2;


	// Integer für del:
	pv_value_t ival;
	memset(&ival, 0, sizeof(ival));
	ival.flags = PV_VAL_INT | PV_TYPE_INT;
	ival.ri = result.del;
	if(rdn_del_pv.setf(msg, &rdn_del_pv.pvp, (int)EQ_T, &ival) < 0)
		return -2;

	return 1;
}


int analyse_dpsi_pvar(struct sip_msg *msg, char *pvar_input, char *unused)
{
	pv_value_t val;
	str uri;
	struct sip_uri parsed_uri;

	pv_spec_t *spec = (pv_spec_t *)pvar_input;

	if(pv_get_spec_value(msg, spec, &val) != 0 || !val.rs.s
			|| val.rs.len <= 0) {
		LM_ERR("analyse_dpsi_pvar: failed to get value\n");
		return -1;
	}

	uri = val.rs;
	if(parse_uri(uri.s, uri.len, &parsed_uri) < 0) {
		LM_INFO("analyse_dpsi_pvar: invalid SIP URI: %.*s\n", uri.len, uri.s);
		return -1;
	}

	rdn_analysis_result_t result;
	memset(&result, 0, sizeof(result));

	if(common_rdn_pvar_analyse(&uri, 0, 1, &result) < 0) {
		LM_INFO("analyse_dpsi_pvar: no service found for URI: %.*s\n", uri.len,
				uri.s);
		return -1;
	}

	// TEMP LOG
	LM_DBG("EL-DG: setze as_name='%.*s', scscf_name='%.*s', "
		   "sessiontype='%.*s', iwf_domain='%.*s', hardcoded_ruri='%.*s', "
		   "insrt='%.*s', mcx_domain='%.*s', del=%d\n",
			result.as_name.len, result.as_name.s ? result.as_name.s : "(null)",
			result.scscf_name.len,
			result.scscf_name.s ? result.scscf_name.s : "(null)",
			result.session_type.len,
			result.session_type.s ? result.session_type.s : "(null)",
			result.iwf_domain.len,
			result.iwf_domain.s ? result.iwf_domain.s : "(null)",
			result.hardcoded_ruri.len,
			result.hardcoded_ruri.s ? result.hardcoded_ruri.s : "(null)",
			result.insrt.len, result.insrt.s ? result.insrt.s : "(null)",
			result.mcx_domain.len,
			result.mcx_domain.s ? result.mcx_domain.s : "(null)", result.del);

	// Strings:
	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.as_name;
	if(rdn_as_name_pv.setf(msg, &rdn_as_name_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.scscf_name;
	if(rdn_scscf_name_pv.setf(msg, &rdn_scscf_name_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.session_type;
	if(rdn_sessiontype_pv.setf(msg, &rdn_sessiontype_pv.pvp, (int)EQ_T, &val)
			< 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.iwf_domain;
	if(rdn_iwf_domain_pv.setf(msg, &rdn_iwf_domain_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.hardcoded_ruri;
	if(rdn_hardcoded_ruri_pv.setf(
			   msg, &rdn_hardcoded_ruri_pv.pvp, (int)EQ_T, &val)
			< 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.insrt;
	if(rdn_insrt_pv.setf(msg, &rdn_insrt_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	if(result.mcx_domain.s && result.mcx_domain.len > 0) {
		val.rs = result.mcx_domain;
	} else {
		val.rs.s = "";
		val.rs.len = 0;
	}
	if(rdn_mcx_domain_avp.setf(msg, &rdn_mcx_domain_avp.pvp, (int)EQ_T, &val)
			< 0)
		return -2;


	// Integer für del:
	pv_value_t ival;
	memset(&ival, 0, sizeof(ival));
	ival.flags = PV_VAL_INT | PV_TYPE_INT;
	ival.ri = result.del;
	if(rdn_del_pv.setf(msg, &rdn_del_pv.pvp, (int)EQ_T, &ival) < 0)
		return -2;

	return 1;
}


int analyse_domain_pvar(struct sip_msg *msg, char *pvar_input, char *unused)
{
	pv_value_t val;
	str uri;
	struct sip_uri parsed_uri;

	pv_spec_t *spec = (pv_spec_t *)pvar_input;

	if(pv_get_spec_value(msg, spec, &val) != 0 || !val.rs.s
			|| val.rs.len <= 0) {
		LM_ERR("analyse_...: failed to get value\n");
		return -1;
	}

	uri = val.rs;
	if(parse_uri(uri.s, uri.len, &parsed_uri) < 0) {
		LM_INFO("analyse_domain_pvar: invalid SIP URI: %.*s\n", uri.len, uri.s);
		return -1;
	}

	rdn_analysis_result_t result;
	memset(&result, 0, sizeof(result));

	if(common_rdn_pvar_analyse(&uri, 0, 0, &result) < 0) {
		LM_INFO("analyse_domain_pvar: no service found for URI: %.*s\n",
				uri.len, uri.s);
		return -1;
	}

	// TEMP LOG
	LM_DBG("EL-DG: setze as_name='%.*s', scscf_name='%.*s', "
		   "sessiontype='%.*s', iwf_domain='%.*s', hardcoded_ruri='%.*s', "
		   "insrt='%.*s', mcx_domain='%.*s', del=%d\n",
			result.as_name.len, result.as_name.s ? result.as_name.s : "(null)",
			result.scscf_name.len,
			result.scscf_name.s ? result.scscf_name.s : "(null)",
			result.session_type.len,
			result.session_type.s ? result.session_type.s : "(null)",
			result.iwf_domain.len,
			result.iwf_domain.s ? result.iwf_domain.s : "(null)",
			result.hardcoded_ruri.len,
			result.hardcoded_ruri.s ? result.hardcoded_ruri.s : "(null)",
			result.insrt.len, result.insrt.s ? result.insrt.s : "(null)",
			result.mcx_domain.len,
			result.mcx_domain.s ? result.mcx_domain.s : "(null)", result.del);


	// Strings:
	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.as_name;
	if(rdn_as_name_pv.setf(msg, &rdn_as_name_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.scscf_name;
	if(rdn_scscf_name_pv.setf(msg, &rdn_scscf_name_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.session_type;
	if(rdn_sessiontype_pv.setf(msg, &rdn_sessiontype_pv.pvp, (int)EQ_T, &val)
			< 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.iwf_domain;
	if(rdn_iwf_domain_pv.setf(msg, &rdn_iwf_domain_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.hardcoded_ruri;
	if(rdn_hardcoded_ruri_pv.setf(
			   msg, &rdn_hardcoded_ruri_pv.pvp, (int)EQ_T, &val)
			< 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	val.rs = result.insrt;
	if(rdn_insrt_pv.setf(msg, &rdn_insrt_pv.pvp, (int)EQ_T, &val) < 0)
		return -2;

	memset(&val, 0, sizeof(val));
	val.flags = PV_VAL_STR;
	if(result.mcx_domain.s && result.mcx_domain.len > 0) {
		val.rs = result.mcx_domain;
	} else {
		val.rs.s = "";
		val.rs.len = 0;
	}
	if(rdn_mcx_domain_avp.setf(msg, &rdn_mcx_domain_avp.pvp, (int)EQ_T, &val)
			< 0)
		return -2;


	// Integer für del:
	pv_value_t ival;
	memset(&ival, 0, sizeof(ival));
	ival.flags = PV_VAL_INT | PV_TYPE_INT;
	ival.ri = result.del;
	if(rdn_del_pv.setf(msg, &rdn_del_pv.pvp, (int)EQ_T, &ival) < 0)
		return -2;

	return 1;
}

int analyse_eirene_ruri(struct sip_msg *_m, char *_t, char *_s)
{

	LM_INFO("analyse_eirene_ruri: called -> calling analyse_ruri()");

	return analyse_ruri(_m, _t, _s, USER_PARAM_EIRENE, 1);
}

int analyse_e164_ruri(struct sip_msg *_m, char *_t, char *_s)
{

	LM_INFO("analyse_e164_ruri: called -> calling analyse_ruri()");

	return analyse_ruri(_m, _t, _s, USER_PARAM_E164, 1);
}

int analyse_dpsi_ruri(struct sip_msg *_m, char *_t, char *_s)
{

	LM_INFO("analyse_dpsi_ruri: called -> calling analyse_ruri()");

	return analyse_ruri(_m, _t, _s, 0, 1);
}

int analyse_domain_ruri(struct sip_msg *_m, char *_t, char *_s)
{

	LM_INFO("analyse_domain_ruri: called -> calling analyse_ruri()");

	return analyse_ruri(_m, _t, _s, 0, 0);
}
