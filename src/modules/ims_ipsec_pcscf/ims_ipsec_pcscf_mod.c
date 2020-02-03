/*
 * IMS IPSEC PCSCF module
 *
 * Copyright (C) 2018 Tsvetomir Dimitrov
 * Copyright (C) 2019 Aleksandar Yosifov
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
#include "port_gen.h"


MODULE_VERSION

usrloc_api_t ul;						/**!< Structure containing pointers to usrloc functions*/
struct tm_binds tmb;					/**!< TM API structure */


str ipsec_listen_addr = STR_NULL;
str ipsec_listen_addr6 = STR_NULL;
int ipsec_client_port =  5062;
int ipsec_server_port =  5063;
int ipsec_reuse_server_port = 1;
int ipsec_max_connections = 2;
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
static int free_uint_fixup(void** param, int param_no);

extern int bind_ipsec_pcscf(usrloc_api_t* api);

int init_flag = 0;

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"ipsec_create",  (cmd_function)w_create,  1, save_fixup2, 0, ONREPLY_ROUTE },
	{"ipsec_forward", (cmd_function)w_forward, 1, save_fixup2, 0, REQUEST_ROUTE | ONREPLY_ROUTE },
	{"ipsec_forward", (cmd_function)w_forward, 2, save_fixup2, free_uint_fixup, REQUEST_ROUTE | ONREPLY_ROUTE },
	{"ipsec_destroy", (cmd_function)w_destroy, 1, save_fixup2, 0, REQUEST_ROUTE | ONREPLY_ROUTE },
	{"bind_ims_ipsec_pcscf", (cmd_function)bind_ipsec_pcscf, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"ipsec_listen_addr",		PARAM_STR, &ipsec_listen_addr		},
	{"ipsec_listen_addr6",  	PARAM_STR, &ipsec_listen_addr6		},
	{"ipsec_client_port",		INT_PARAM, &ipsec_client_port		},
	{"ipsec_server_port",		INT_PARAM, &ipsec_server_port		},
	{"ipsec_reuse_server_port",	INT_PARAM, &ipsec_reuse_server_port	},
	{"ipsec_max_connections",	INT_PARAM, &ipsec_max_connections	},
	{"ipsec_spi_id_start",		INT_PARAM, &spi_id_start			},
	{"ipsec_spi_id_range",		INT_PARAM, &spi_id_range			},
	{0, 0, 0}
};

/*! \brief
 * Module exports structure
 */
struct module_exports exports = {
	"ims_ipsec_pcscf",
	DEFAULT_DLFLAGS,/* dlopen flags */
	cmds,			/* exported functions */
	params,			/* exported params */
	0,				/*·exported·RPC·methods·*/
	0,				/* exported pseudo-variables */
	0,				/*·response·function·*/
	mod_init,		/* module initialization function */
	child_init,		/* Per-child init function */
	mod_destroy,	/* destroy function */
};


static void ipsec_print_all_socket_lists()
{
	struct socket_info *si;
	struct socket_info** list;
	struct addr_info* ai;
	unsigned short proto;

	LM_INFO("Listening on:\n");

	proto=PROTO_UDP;
	do{
		list=get_sock_info_list(proto);
		for(si=list?*list:0; si; si=si->next){
			char buf[1024];
			int cnt=0;

			memset(buf, 0, sizeof(buf));

			if(si->addr_info_lst){
				sprintf(buf, "%s: (%s", get_valid_proto_name(proto), si->address_str.s);
				cnt = strlen(buf);

				for(ai=si->addr_info_lst; ai; ai=ai->next){
					sprintf(buf + cnt, ", %s", ai->address_str.s);
					cnt = strlen(buf);
				}

				if(si->port_no_str.s){
					sprintf(buf + cnt, "):%s%s%s", si->port_no_str.s, si->flags & SI_IS_MCAST ? " mcast" : "", si->flags & SI_IS_MHOMED? " mhomed" : "");
				}else{
					sprintf(buf + cnt, "):%u%s%s", si->port_no, si->flags & SI_IS_MCAST ? " mcast" : "", si->flags & SI_IS_MHOMED? " mhomed" : "");
				}
				cnt = strlen(buf);
			}else{
				sprintf(buf, "%s: %s", get_valid_proto_name(proto), si->name.s);
				cnt = strlen(buf);

				if(!(si->flags & SI_IS_IP)){
					if(si->address_str.s){
						sprintf(buf + cnt, " [%s]", si->address_str.s);
						cnt = strlen(buf);
					}
				}

				if(si->port_no_str.s){
					sprintf(buf + cnt, ":%s%s%s", si->port_no_str.s, si->flags & SI_IS_MCAST ? " mcast" : "", si->flags & SI_IS_MHOMED? " mhomed" : "");
				}else{
					sprintf(buf + cnt, ":%u%s%s", si->port_no, si->flags & SI_IS_MCAST ? " mcast" : "", si->flags & SI_IS_MHOMED? " mhomed" : "");
				}
				cnt = strlen(buf);

				if(si->useinfo.name.s){
					printf(buf + cnt, " advertise %s:%d", si->useinfo.name.s, si->useinfo.port_no);
					cnt = strlen(buf);
				}
			}

			LM_INFO("%s\n", buf);
		}
	}while((proto=next_proto(proto)));
}

static int ipsec_add_listen_ifaces()
{
	char addr4[128];
	char addr6[128];
	int i;

	for(i = 0; i < ipsec_max_connections; ++i){
		if(ipsec_listen_addr.len) {
			if(ipsec_listen_addr.len > sizeof(addr4)-1) {
				LM_ERR("Bad value for ipsec listen address IPv4: %.*s\n", ipsec_listen_addr.len, ipsec_listen_addr.s);
				return -1;
			}

			memset(addr4, 0, sizeof(addr4));
			memcpy(addr4, ipsec_listen_addr.s, ipsec_listen_addr.len);

			//add listen interfaces for IPv4
			if(add_listen_iface(addr4, NULL, ipsec_client_port + i, PROTO_TCP, 0) != 0) {
				LM_ERR("Error adding listen ipsec client TCP interface for IPv4\n");
				return -1;
			}

			if(add_listen_iface(addr4, NULL, ipsec_server_port + i, PROTO_TCP, 0) != 0) {
				LM_ERR("Error adding listen ipsec server TCP interface for IPv4\n");
				return -1;
			}

			if(add_listen_iface(addr4, NULL, ipsec_client_port + i, PROTO_UDP, 0) != 0) {
				LM_ERR("Error adding listen ipsec client UDP interface for IPv4\n");
				return -1;
			}

			if(add_listen_iface(addr4, NULL, ipsec_server_port + i, PROTO_UDP, 0) != 0) {
				LM_ERR("Error adding listen ipsec server UDP interface for IPv4\n");
				return -1;
			}
		}

		if(ipsec_listen_addr6.len) {
			if(ipsec_listen_addr6.len > sizeof(addr6)-1) {
				LM_ERR("Bad value for ipsec listen address IPv6: %.*s\n", ipsec_listen_addr6.len, ipsec_listen_addr6.s);
				return -1;
			}

			memset(addr6, 0, sizeof(addr6));
			memcpy(addr6, ipsec_listen_addr6.s, ipsec_listen_addr6.len);

			//add listen interfaces for IPv6
			if(add_listen_iface(addr6, NULL, ipsec_client_port + i, PROTO_TCP, 0) != 0) {
				LM_ERR("Error adding listen ipsec client TCP interface for IPv6\n");
				return -1;
			}

			if(add_listen_iface(addr6, NULL, ipsec_server_port + i, PROTO_TCP, 0) != 0) {
				LM_ERR("Error adding listen ipsec server TCP interface for IPv6\n");
				return -1;
			}

			if(add_listen_iface(addr6, NULL, ipsec_client_port + i, PROTO_UDP, 0) != 0) {
				LM_ERR("Error adding listen ipsec client UDP interface for IPv6\n");
				return -1;
			}

			if(add_listen_iface(addr6, NULL, ipsec_server_port + i, PROTO_UDP, 0) != 0) {
				LM_ERR("Error adding listen ipsec server UDP interface for IPv6\n");
				return -1;
			}
		}
	}

	if(fix_all_socket_lists() != 0) {
		LM_ERR("Error calling fix_all_socket_lists()\n");
		return -1;
	}

	ipsec_print_all_socket_lists();

	return 0;
}

/*! \brief
 * Initialize parent
 */
static int mod_init(void) {
	bind_usrloc_t bind_usrloc;

	bind_usrloc = (bind_usrloc_t) find_export("ul_bind_ims_usrloc_pcscf", 1, 0);
	if (!bind_usrloc) {
		LM_ERR("can't bind ims_usrloc_pcscf\n");
		return -1;
	}

	if (bind_usrloc(&ul) < 0) {
		return -1;
	}
	LM_INFO("Successfully bound to PCSCF Usrloc module\n");

	/* load the TM API */
	if (load_tm_api(&tmb) != 0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}
	LM_INFO("Successfully bound to TM module\n");

	if(ipsec_add_listen_ifaces() != 0){
		return -1;
	}

	if(ipsec_cleanall() != 0) {
		LM_ERR("Error ipsec tunnels during for module initialisation\n");
		return -1;
	}

	int res = 0;
	if((res = init_spi_gen(spi_id_start, spi_id_range)) != 0) {
		LM_ERR("Error initialising spi generator. Error: %d\n", res);
		return -1;
	}

	if((res = init_port_gen(ipsec_server_port, ipsec_client_port, ipsec_max_connections)) != 0) {
		LM_ERR("Error initialising port generator. Error: %d\n", res);
		return -1;
	}

	init_flag = 1;

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

	if(destroy_port_gen() != 0){
		LM_ERR("Error destroying port generator\n");
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

static int unit_fixup(void** param, int param_no)
{
	str s;
	unsigned int* num;

	if(*param){
		num = (unsigned int*)pkg_malloc(sizeof(unsigned int));
		*num = 0;

		s.s = *param;
		s.len = strlen(s.s);

		if (likely(str2int(&s, num) == 0)) {
			*param = (void*)(long)num;
		}else{
			LM_ERR("failed to convert to int\n");
			pkg_free(num);
			return E_UNSPEC;
		}
	}else{
		return E_UNSPEC;
	}

	return 0;
}

static int free_uint_fixup(void** param, int param_no)
{
	if(*param && param_no == 2){
		pkg_free(*param);
		*param = 0;
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
	}else if(param_no == 2){
		return unit_fixup(param, param_no);
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
	if(_cflags){
		return ipsec_forward(_m, (udomain_t*)_d, ((int)(*_cflags)));
	}
	return ipsec_forward(_m, (udomain_t*)_d, 0);
}

static int w_destroy(struct sip_msg* _m, char* _d, char* _cflags)
{
	return ipsec_destroy(_m, (udomain_t*)_d);
}
