/*
 * IMS IPSEC PCSCF module
 *
 * Copyright (C) 2018 Tsvetomir Dimitrov
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

#include "../../core/sr_module.h"
#include "../../modules/tm/tm_load.h"
#include "../ims_usrloc_pcscf/usrloc.h"

#include "cmd.h"
#include "spi_gen.h"


MODULE_VERSION

usrloc_api_t ul;						/**!< Structure containing pointers to usrloc functions*/
struct tm_binds tmb;					/**!< TM API structure */


str ipsec_listen_addr = str_init("127.0.0.1");
int ipsec_client_port =  5062;
int ipsec_server_port =  5063;
int spi_id_start = 100;
int spi_id_range = 1000;
int xfrm_user_selector = 143956232;

/*! \brief Module init & destroy function */
static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);
static int w_create(struct sip_msg* _m, char* _d, char* _cflags);
static int w_forward(struct sip_msg* _m, char* _d, char* _cflags);
static int w_destroy(struct sip_msg* _m, char* _d, char* _cflags);

/*! \brief Fixup functions */
static int domain_fixup(void** param, int param_no);
static int save_fixup2(void** param, int param_no);

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"ipsec_create",  (cmd_function)w_create,  1, save_fixup2, 0, ONREPLY_ROUTE },
	{"ipsec_forward", (cmd_function)w_forward, 1, save_fixup2, 0, REQUEST_ROUTE | ONREPLY_ROUTE },
	{"ipsec_destroy", (cmd_function)w_destroy, 1, save_fixup2, 0, REQUEST_ROUTE | ONREPLY_ROUTE },
	{0, 0, 0, 0, 0, 0}
};

/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"ipsec_listen_addr",   PARAM_STR, &ipsec_listen_addr   },
	{"ipsec_client_port",	INT_PARAM, &ipsec_client_port   },
	{"ipsec_server_port",	INT_PARAM, &ipsec_server_port   },
	{"ipsec_spi_id_start",	INT_PARAM, &spi_id_start},
	{"ipsec_spi_id_range",	INT_PARAM, &spi_id_range},
	{0, 0, 0}
};

/*! \brief
 * Module exports structure
 */
struct module_exports exports = {
	"ims_ipsec_pcscf",
	DEFAULT_DLFLAGS,/* dlopen flags */
	cmds,		/* exported functions */
	params,		/* exported params */
	0,		/*·exported·RPC·methods·*/
	0,		/* exported pseudo-variables */
	0,		/*·response·function·*/
	mod_init,	/* module initialization function */
	child_init,	/* Per-child init function */
	mod_destroy,	/* destroy function */
};


/*! \brief
 * Initialize parent
 */
static int mod_init(void) {
    char addr[128];
    if(ipsec_listen_addr.len > sizeof(addr)-1) {
        LM_ERR("Bad value for ipsec listen address: %.*s\n", ipsec_listen_addr.len, ipsec_listen_addr.s);
        return -1;
    }
    memset(addr, 0, sizeof(addr));
    memcpy(addr, ipsec_listen_addr.s, ipsec_listen_addr.len);

	bind_usrloc_t bind_usrloc;

	bind_usrloc = (bind_usrloc_t) find_export("ul_bind_ims_usrloc_pcscf", 1, 0);
	if (!bind_usrloc) {
		LM_ERR("can't bind ims_usrloc_pcscf\n");
		return -1;
	}

	if (bind_usrloc(&ul) < 0) {
		return -1;
	}
	LM_DBG("Successfully bound to PCSCF Usrloc module\n");

	/* load the TM API */
	if (load_tm_api(&tmb) != 0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}
	LM_DBG("Successfully bound to TM module\n");


    //add listen interfaces
    if(add_listen_iface(addr, NULL, ipsec_client_port, PROTO_TCP, 0) != 0) {
        LM_ERR("Error adding listen ipsec client interface\n");
        return -1;
    }

    if(add_listen_iface(addr, NULL, ipsec_server_port, PROTO_TCP, 0) != 0) {
        LM_ERR("Error adding listen ipsec server interface\n");
        return -1;
    }

    if(add_listen_iface(addr, NULL, ipsec_client_port, PROTO_UDP, 0) != 0) {
        LM_ERR("Error adding listen ipsec client interface\n");
        return -1;
    }

    if(add_listen_iface(addr, NULL, ipsec_server_port, PROTO_UDP, 0) != 0) {
        LM_ERR("Error adding listen ipsec server interface\n");
        return -1;
    }

    if(fix_all_socket_lists() != 0) {
        LM_ERR("Error calling fix_all_socket_lists() during module initialisation\n");
        return -1;
    }

    if(ipsec_cleanall() != 0) {
        LM_ERR("Error ipsec tunnels during for module initialisation\n");
        return -1;
	}

    int res = 0;
    if((res = init_spi_gen(spi_id_start, spi_id_start + spi_id_range)) != 0) {
        LM_ERR("Error initialising spi generator. Error: %d\n", res);
        return -1;
    }

	return 0;
}

static void mod_destroy(void)
{
    if(ipsec_cleanall() != 0) {
        LM_ERR("Error ipsec tunnels during for module cleanup\n");
	}

    if(destroy_spi_gen() != 0) {
        LM_ERR("Error destroying spi generator\n");
    }
}

static int child_init(int rank)
{
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
 * Wrapper to ipsec functions
 */
static int w_create(struct sip_msg* _m, char* _d, char* _cflags)
{
	return ipsec_create(_m, (udomain_t*)_d);
}

static int w_forward(struct sip_msg* _m, char* _d, char* _cflags)
{
	return ipsec_forward(_m, (udomain_t*)_d);
}

static int w_destroy(struct sip_msg* _m, char* _d, char* _cflags)
{
	return ipsec_destroy(_m, (udomain_t*)_d);
}
