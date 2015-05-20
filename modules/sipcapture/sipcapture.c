/* 
 * sipcapture module - helper module to capture sip messages
 *
 * Copyright (C) 2011-2014 Alexandr Dubovikov (QSC AG) (alexandr.dubovikov@gmail.com)
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

/*! \file
 * sipcapture module - helper module to capture sip messages
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h> 
#include <netdb.h>
#include <arpa/inet.h>

/* BPF structure */
#ifdef __OS_linux
#include <linux/filter.h>
#endif

#ifndef __USE_BSD
#define __USE_BSD  /* on linux use bsd version of iphdr (more portable) */
#endif /* __USE_BSD */
#include <netinet/ip.h>
#define __FAVOR_BSD /* on linux use bsd version of udphdr (more portable) */
#include <netinet/udp.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../events.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lib/kmi/mi.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_ppi_pai.h"
#include "../../pvar.h"
#include "../../str.h"
#include "../../onsend.h"
#include "../../resolve.h"
#include "../../receive.h"
#include "../../mod_fix.h"
#include "sipcapture.h"
#include "hash_mode.h"
#include "hep.h"

#ifdef STATISTICS
#include "../../lib/kcore/statistics.h"
#endif


MODULE_VERSION


#define ETHHDR 14 /* sizeof of ethhdr structure */

#define EMPTY_STR(val) val.s=""; val.len=0;

#define TABLE_LEN 256

#define NR_KEYS 37
#define RTCP_NR_KEYS 12

/*multiple table mode*/
enum e_mt_mode{
	mode_random = 1,
	mode_hash,
	mode_round_robin,
	mode_error
};


typedef struct _capture_mode_data {
	unsigned int id;
	str name;
	str db_url;
	db1_con_t *db_con;
	db_func_t db_funcs;
	str * table_names;
	unsigned int no_tables;
	enum e_mt_mode mtmode;
	enum hash_source hash_source;
	unsigned int rr_idx;
	stat_var* sipcapture_req;
	stat_var* sipcapture_rpl;
	struct _capture_mode_data * next;
}_capture_mode_data_t;

_capture_mode_data_t * capture_modes_root = NULL;
_capture_mode_data_t * capture_def = NULL;

/* module function prototypes */
static int mod_init(void);
static int sipcapture_init_rpc(void);
static int child_init(int rank);
static void destroy(void);
static int sipcapture_fixup(void** param, int param_no);
static int sip_capture(struct sip_msg *msg, str *dtable,  _capture_mode_data_t *cm_data);

static int w_sip_capture(struct sip_msg* _m, char* _table, _capture_mode_data_t * _cm_data, char* s2);
int init_rawsock_children(void);
int extract_host_port(void);
int raw_capture_socket(struct ip_addr* ip, str* iface, int port_start, int port_end, int proto);
int raw_capture_rcv_loop(int rsock, int port1, int port2, int ipip);



static struct mi_root* sip_capture_mi(struct mi_root* cmd, void* param );

static str db_url		= str_init(DEFAULT_DB_URL);
static str table_name		= str_init("sip_capture");
static str hash_source		= str_init("call_id");
static str mt_mode		= str_init("rand");
static str date_column		= str_init("date");
static str micro_ts_column 	= str_init("micro_ts");
static str method_column 	= str_init("method"); 	
static str reply_reason_column 	= str_init("reply_reason");        
static str correlation_column 	= str_init("correlation_id");
static str ruri_column 		= str_init("ruri");     	
static str ruri_user_column 	= str_init("ruri_user");  
static str from_user_column 	= str_init("from_user");  
static str from_tag_column 	= str_init("from_tag");   
static str to_user_column 	= str_init("to_user");
static str to_tag_column 	= str_init("to_tag");   
static str pid_user_column 	= str_init("pid_user");
static str contact_user_column 	= str_init("contact_user");
static str auth_user_column 	= str_init("auth_user");  
static str callid_column 	= str_init("callid");
static str callid_aleg_column 	= str_init("callid_aleg");
static str via_1_column 	= str_init("via_1");      
static str via_1_branch_column 	= str_init("via_1_branch"); 
static str cseq_column		= str_init("cseq");     
static str diversion_column 	= str_init("diversion_user"); 
static str reason_column 	= str_init("reason");        
static str content_type_column 	= str_init("content_type");  
static str authorization_column = str_init("auth"); 
static str user_agent_column 	= str_init("user_agent");
static str source_ip_column 	= str_init("source_ip");  
static str source_port_column 	= str_init("source_port");	
static str dest_ip_column	= str_init("destination_ip");
static str dest_port_column 	= str_init("destination_port");		
static str contact_ip_column 	= str_init("contact_ip"); 
static str contact_port_column 	= str_init("contact_port");
static str orig_ip_column 	= str_init("originator_ip");      
static str orig_port_column 	= str_init("originator_port");    
static str rtp_stat_column 	= str_init("rtp_stat");    
static str proto_column 	= str_init("proto"); 
static str family_column 	= str_init("family"); 
static str type_column 		= str_init("type");  
static str node_column 		= str_init("node");  
static str msg_column 		= str_init("msg");   
static str capture_node 	= str_init("homer01");     	
static str star_contact		= str_init("*");
static str callid_aleg_header   = str_init("X-CID");

int raw_sock_desc = -1; /* raw socket used for ip packets */
unsigned int raw_sock_children = 1;
int capture_on   = 0;
int ipip_capture_on   = 0;
int moni_capture_on   = 0;
int moni_port_start = 0;
int moni_port_end   = 0;
int *capture_on_flag = NULL;
int db_insert_mode = 0;
int promisc_on = 0;
int bpf_on = 0;
int hep_capture_on   = 0;
int insert_retries = 0;
int insert_retry_timeout = 60;
int hep_offset = 0;
str raw_socket_listen = { 0, 0 };
str raw_interface = { 0, 0 };
char *authkey = NULL, *correlation_id = NULL;

struct ifreq ifr; 	/* interface structure */

#ifdef __OS_linux
/* Linux socket filter */
/* tcpdump -s 0 udp and portrange 5060-5090 -dd */
static struct sock_filter BPF_code[] = { { 0x28, 0, 0, 0x0000000c }, { 0x15, 0, 7, 0x000086dd },
        { 0x30, 0, 0, 0x00000014 },   { 0x15, 0, 18, 0x00000011 }, { 0x28, 0, 0, 0x00000036 },
        { 0x35, 0, 1, 0x000013c4 },   { 0x25, 0, 14, 0x000013e2 }, { 0x28, 0, 0, 0x00000038 },
        { 0x35, 11, 13, 0x000013c4 }, { 0x15, 0, 12, 0x00000800 }, { 0x30, 0, 0, 0x00000017 },
        { 0x15, 0, 10, 0x00000011 },  { 0x28, 0, 0, 0x00000014 },  { 0x45, 8, 0, 0x00001fff },
        { 0xb1, 0, 0, 0x0000000e },   { 0x48, 0, 0, 0x0000000e },  { 0x35, 0, 1, 0x000013c4 },
        { 0x25, 0, 3, 0x000013e2 },   { 0x48, 0, 0, 0x00000010 },  { 0x35, 0, 2, 0x000013c4 },
        { 0x25, 1, 0, 0x000013e2 },   { 0x6, 0, 0, 0x0000ffff },   { 0x6, 0, 0, 0x00000000 },
};
#endif

//db1_con_t *db_con = NULL; 		/*!< database connection */
//db_func_t db_funcs;      		/*!< Database functions */

//str* table_names = NULL;

unsigned int no_tables = 0;

enum e_mt_mode mtmode = mode_random ;
enum hash_source source = hs_error;

//unsigned int rr_idx = 0;

struct hep_timehdr* heptime;

/*! \brief
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"sip_capture", (cmd_function)w_sip_capture, 0, 0, 0, ANY_ROUTE},
	{"sip_capture", (cmd_function)w_sip_capture, 1, sipcapture_fixup, 0, ANY_ROUTE },	                         
	{"sip_capture", (cmd_function)w_sip_capture, 2, sipcapture_fixup, 0, ANY_ROUTE },
	{0, 0, 0, 0, 0, 0}
};


int capture_mode_param(modparam_t type, void *val);

/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",			PARAM_STR, &db_url            },
	{"table_name",       		PARAM_STR, &table_name	},
	{"hash_source",				PARAM_STR, &hash_source	},
	{"mt_mode",					PARAM_STR, &mt_mode	},
	{"date_column",        		PARAM_STR, &date_column       },
	{"micro_ts_column",     	PARAM_STR, &micro_ts_column	},
	{"method_column",      		PARAM_STR, &method_column 	},
        {"correlation_column",     	PARAM_STR, &correlation_column.s },
	{"reply_reason_column",		PARAM_STR, &reply_reason_column	},
	{"ruri_column",      		PARAM_STR, &ruri_column     	},
	{"ruri_user_column",      	PARAM_STR, &ruri_user_column  },
	{"from_user_column",      	PARAM_STR, &from_user_column  },
	{"from_tag_column",        	PARAM_STR, &from_tag_column   },
	{"to_user_column",     		PARAM_STR, &to_user_column	},
	{"to_tag_column",        	PARAM_STR, &to_tag_column	},
	{"pid_user_column",   		PARAM_STR, &pid_user_column	},
	{"contact_user_column",        	PARAM_STR, &contact_user_column	},
	{"auth_user_column",     	PARAM_STR, &auth_user_column  },
	{"callid_column",      		PARAM_STR, &callid_column},
	{"callid_aleg_column",      	PARAM_STR, &callid_aleg_column},
	{"via_1_column",		PARAM_STR, &via_1_column      },
	{"via_1_branch_column",        	PARAM_STR, &via_1_branch_column },
	{"cseq_column",     		PARAM_STR, &cseq_column     },
	{"diversion_column",      	PARAM_STR, &diversion_column },
	{"reason_column",		PARAM_STR, &reason_column        },
	{"content_type_column",        	PARAM_STR, &content_type_column  },
	{"authorization_column",     	PARAM_STR, &authorization_column },
	{"user_agent_column",      	PARAM_STR, &user_agent_column	},
	{"source_ip_column",		PARAM_STR, &source_ip_column  },
	{"source_port_column",		PARAM_STR, &source_port_column},
	{"destination_ip_column",	PARAM_STR, &dest_ip_column	},
	{"destination_port_column",	PARAM_STR, &dest_port_column	},
	{"contact_ip_column",		PARAM_STR, &contact_ip_column },
	{"contact_port_column",		PARAM_STR, &contact_port_column},
	{"originator_ip_column",	PARAM_STR, &orig_ip_column    },
	{"originator_port_column",	PARAM_STR, &orig_port_column  },
	{"proto_column",		PARAM_STR, &proto_column },
	{"family_column",		PARAM_STR, &family_column },
	{"rtp_stat_column",		PARAM_STR, &rtp_stat_column },
	{"type_column",			PARAM_STR, &type_column  },
	{"node_column",			PARAM_STR, &node_column  },
	{"msg_column",			PARAM_STR, &msg_column   },
	{"capture_on",           	INT_PARAM, &capture_on          },
	{"capture_node",     		PARAM_STR, &capture_node     	},
        {"raw_sock_children",  		INT_PARAM, &raw_sock_children   },	
        {"hep_capture_on",  		INT_PARAM, &hep_capture_on   },	
	{"raw_socket_listen",     	PARAM_STR, &raw_socket_listen   },
        {"raw_ipip_capture_on",  	INT_PARAM, &ipip_capture_on  },	
        {"raw_moni_capture_on",  	INT_PARAM, &moni_capture_on  },	
        {"db_insert_mode",  		INT_PARAM, &db_insert_mode  },	
	{"raw_interface",     		PARAM_STR, &raw_interface   },
        {"promiscious_on",  		INT_PARAM, &promisc_on   },		
        {"raw_moni_bpf_on",  		INT_PARAM, &bpf_on   },		
        {"callid_aleg_header",          PARAM_STR, &callid_aleg_header},
        {"capture_mode",		PARAM_STRING|USE_FUNC_PARAM, (void *)capture_mode_param},
    {"insert_retries",   	INT_PARAM, &insert_retries },
    {"insert_retry_timeout",INT_PARAM, &insert_retry_timeout },
		{0, 0, 0}
};



/*! \brief
 * MI commands
 */
static mi_export_t mi_cmds[] = {
	{ "sip_capture", sip_capture_mi,   0,  0,  0 },
	{ 0, 0, 0, 0, 0}
};


#ifdef STATISTICS
/*stat_var* sipcapture_req;
stat_var* sipcapture_rpl;

stat_export_t sipcapture_stats[] = {
	{"captured_requests" ,  0,  &sipcapture_req  },
	{"captured_replies"  ,  0,  &sipcapture_rpl  },
	{0,0,0}
};
*/
stat_export_t *sipcapture_stats = NULL;
#endif

/*! \brief module exports */
struct module_exports exports = {
	"sipcapture", 
	DEFAULT_DLFLAGS, /*!< dlopen flags */
	cmds,       /*!< Exported functions */
	params,     /*!< Exported parameters */
#ifdef STATISTICS
//	sipcapture_stats,  /*!< exported statistics */
	0,
#else
	0,          /*!< exported statistics */
#endif
	mi_cmds,    /*!< exported MI functions */
	0,          /*!< exported pseudo-variables */
	0,          /*!< extra processes */
	mod_init,   /*!< module initialization function */
	0,          /*!< response function */
	destroy,    /*!< destroy function */
	child_init  /*!< child initialization function */
};


/* returns number of tables if successful
 * <0 if failed
 */
int parse_table_names (str table_name, str ** table_names){

	char *p = NULL;
	unsigned int no_tables;
	char * table_name_cpy;
	unsigned int i;

	/*parse and save table names*/
	no_tables = 1;
	i = 0;

	str * names;

	table_name_cpy = (char *) pkg_malloc(sizeof(char) * table_name.len + 1 );
	if (table_name_cpy == NULL){
		LM_ERR("no more pkg memory left\n");
		return -1;
	}
	memcpy (table_name_cpy, table_name.s, table_name.len);
	table_name_cpy[table_name.len] = '\0';

	p = table_name_cpy;

	while (*p)
	{
		if (*p== '|')
		{
			no_tables++;
		}
		p++;
	}

	names = (str*)pkg_malloc(sizeof(str) * no_tables);
	if(names == NULL) {
		LM_ERR("no more pkg memory left\n");
		return -1;
	}
	p = strtok (table_name_cpy,"| \t");
	while (p != NULL)
	{
		LM_INFO ("INFO: table name:%s\n",p);
		names[i].len = strlen (p);
		names[i].s =  (char *)pkg_malloc(sizeof(char) *names[i].len);
		memcpy(names[i].s, p, names[i].len);
		i++;
		p = strtok (NULL, "| \t");
	}

	pkg_free(table_name_cpy);

	*table_names = names;

	return no_tables;

}

/* checks for some missing fields*/
int check_capture_mode ( _capture_mode_data_t * n) {


	if (!n->db_url.s || !n->db_url.len){
		LM_ERR("db_url not set\n");
		goto error;
	}

	if (!n->mtmode ){
		LM_ERR("mt_mode not set\n");
		goto error;
	}
	else if (!n->no_tables || !n->table_names){
		LM_ERR("table names not set\n");
		goto error;
	}
	return 0;

	error:
	LM_ERR("parsing capture_mode: not all needed parameters are set. Please check again\n");
	return -1;
}

int capture_mode_set_params (_capture_mode_data_t * n, str * params){


	param_t * params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	db_func_t db_funcs;

	str s;
	LM_DBG("to tokenize: [%.*s]\n", params->len, params->s);
	if ( n == NULL || params == NULL)
		return -1;
	s = *params;

	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0)
		return -1;
	for (pit = params_list; pit; pit=pit->next)
	{
		LM_DBG("parameter is [%.*s]\n",pit->name.len, pit->name.s );
		LM_DBG("parameter value is [%.*s]\n", pit->body.len, pit->body.s);
		if (pit->name.len == 6 && strncmp (pit->name.s, "db_url", pit->name.len)==0){

			n->db_url.len =pit->body.len;
			n->db_url.s = (char*)pkg_malloc(sizeof(char) * n->db_url.len);
			if (!n->db_url.s){
				LM_ERR("no more pkg memory\n");
				goto error;
			}
			memcpy(n->db_url.s, pit->body.s,n->db_url.len );

			if (db_bind_mod(&n->db_url, &db_funcs)){

				LM_ERR("parsing capture_mode: could not bind db funcs for url:[%.*s]\n", n->db_url.len, n->db_url.s);
				goto error;
			}
			n->db_funcs = db_funcs;

			if (!DB_CAPABILITY(n->db_funcs, DB_CAP_INSERT))
			{
				LM_ERR("parsing capture_mode: database modules does not provide all functions needed"
						" by module\n");
					goto error;
			}

		}

		else if (pit->name.len == 10 && strncmp (pit->name.s, "table_name", pit->name.len)==0){
			if ((int)(n->no_tables = parse_table_names(pit->body, &n->table_names))<0){
				LM_ERR("parsing capture_mode: table name parsing failed\n");
				goto error;
			}

		}
		else if (pit->name.len == 7 && strncmp (pit->name.s, "mt_mode", pit->name.len)==0){

			if (pit->body.len == 4 && strncmp(pit->body.s, "rand",pit->body.len ) ==0)
			{
				n->mtmode  = mode_random;
			}
			else if (pit->body.len == 11 && strncmp(pit->body.s, "round_robin",pit->body.len ) ==0)
			{
				n->mtmode = mode_round_robin;
			}
			else if (pit->body.len == 4 && strncmp(pit->body.s, "hash", pit->body.len) ==0)
			{
				n->mtmode = mode_hash;
			}
			else {
				LM_ERR("parsing capture_mode: capture mode not recognized: [%.*s]\n", pit->body.len, pit->body.s);
				goto error;

			}
		}
		else if (pit->name.len == 11 && strncmp (pit->name.s, "hash_source", pit->name.len)==0){
			if ( (n->hash_source = get_hash_source (pit->body.s))  == hs_error)
			{
				LM_ERR("parsing capture_mode: hash source unrecognized: [%.*s]\n", pit->body.len, pit->body.s);
				goto error;
			}
		}


	}
	if (n->mtmode == mode_hash && ( n->hash_source == 0 || n->hash_source == hs_error )){
		LM_WARN("Hash mode set, but no hash source provided for [%.*s]. Will consider hashing by call id.\n", n->name.len, n->name.s);
		n->hash_source = hs_call_id;
	}

	if ( check_capture_mode(n)){
		goto error;
	}

	return 0;

error:
	if (n->db_url.s){
		pkg_free(n->db_url.s);
	}
	return -1;





}

void * capture_mode_init(str *name, str * params) {

	_capture_mode_data_t * n = NULL;
	unsigned int id;

	if (!name || name->len == 0){
		LM_ERR("capture_mode name is empty\n");
		goto error;
	}
	if (!params || params->len == 0){
		LM_ERR("capture_mode params are empty\n");
		goto error;
	}
	id = core_case_hash(name, 0, 0);
	n = (_capture_mode_data_t *) pkg_malloc(sizeof(_capture_mode_data_t));
	if (!n){
		LM_ERR("no more pkg memory\n");
		goto error;
	}
	memset (n, 0,sizeof(_capture_mode_data_t) );
	n->id = id;
	n->name.len = name->len;
	n->name.s = (char *)pkg_malloc(sizeof(char) * n->name.len);
	if (!n->name.s){
		LM_ERR("no more pkg memory\n");
		goto error;
	}
	memcpy(n->name.s, name->s, n->name.len);
	n->table_names = (str *)pkg_malloc(sizeof(str));
	if (!n->table_names){
		LM_ERR("no more pkg memory\n");
		goto error;
	}



	if (capture_mode_set_params (n, params)<0){
		LM_ERR("capture mode parsing failed\n");
		goto error;
	}

	n->next = capture_modes_root;
	capture_modes_root = n;
	return n;

error:
	if (n->name.s){
		pkg_free(n->name.s);
	}
	if (n->table_names){
		pkg_free(n->table_names);
	}
	if (n){
		pkg_free(n);
	}
	return 0;

}

/*parse name=>param1=>val1;param2=>val2;..*/
int capture_mode_param(modparam_t type, void *val){


	str name;
	str in;
	str tok;
	char * p;

	in.s = val;
	in.len = strlen(in.s);
	p = in.s;

	while(p<in.s+in.len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	if(p>in.s+in.len || *p=='\0')
		goto error;
	name.s = p;
	while(p < in.s + in.len)
	{
		if(*p=='=' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
			break;
		p++;
	}

	if(p>in.s+in.len || *p=='\0')
		goto error;
	name.len = p - name.s;
	if(*p!='=')
	{
		while(p<in.s+in.len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in.s+in.len || *p=='\0' || *p!='=')
			goto error;
	}
	p++;
	if(*p!='>')
		goto error;
	p++;
	while(p<in.s+in.len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	tok.s = p;
	tok.len = in.len + (int)(in.s - p);

	LM_DBG("capture_mode name: [%.*s] data: [%.*s]\n", name.len, name.s, tok.len, tok.s);
	if (!capture_mode_init(&name, &tok)){
		return -1;
	}
	return 0;

	error:
		LM_ERR("invalid parameter [%.*s] at [%d]\n", in.len, in.s,
				(int)(p-in.s));
		return -1;
}






/*! \brief Initialize sipcapture module */
static int mod_init(void) {


	struct ip_addr *ip = NULL;
	char * def_params = NULL;

#ifdef STATISTICS
	int cnt = 0;
	int i = 0;
	char * stat_name = NULL;
	_capture_mode_data_t * c = NULL;
	int def;
#endif

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
	if(sipcapture_init_rpc()!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/*Check the table name - if table_name is empty and no capture modes are defined, then error*/
	if(!table_name.len && capture_modes_root == NULL) {
		LM_ERR("ERROR: sipcapture: mod_init: table_name is not defined or empty\n");
		return -1;
	}


	/*create a default capture mode using the default parameters*/
	def_params = (char *) pkg_malloc(snprintf(NULL, 0, "db_url=%s;table_name=%s;mt_mode=%s;hash_source=%s",db_url.s, table_name.s, mt_mode.s,hash_source.s) + 1);
	sprintf(def_params, "db_url=%s;table_name=%s;mt_mode=%s;hash_source=%s",db_url.s, table_name.s, mt_mode.s,hash_source.s);

	str def_name,  def_par;
	def_name.s= strdup("default");
	def_name.len = 7;
	def_par.s = def_params;
	def_par.len = strlen (def_params);

	LM_DBG("def_params is: %s\n", def_params);


	if ((capture_def =capture_mode_init(&def_name, &def_par)) == NULL){
		LM_WARN("Default capture mode configuration failed. Suppose sip_capture calls will use other defined capture modes.\n");
	}

	pkg_free(def_params);


#ifdef STATISTICS

	c = capture_modes_root;
	while (c){
		cnt++;
		c=c->next;
	}
	/*requests and replies for each mode + 1 zero-filled stat_export */
	stat_export_t *stats = (stat_export_t *) shm_malloc(sizeof(stat_export_t) * cnt * 2 + 1 );

	c = capture_modes_root;

	while (c){
		/*for the default capture_mode, don't add it's name to the stat name*/
		def = (capture_def && c == capture_def)?1:0;
		stat_name = (char *)shm_malloc(sizeof (char) * (snprintf(NULL, 0 , (def)?"captured_requests%.*s":"captured_requests[%.*s]", (def)?0:c->name.len, (def)?"":c->name.s) + 1));
		sprintf(stat_name, (def)?"captured_requests%.*s":"captured_requests[%.*s]", (def)?0:c->name.len, (def)?"":c->name.s);
		stats[i].name = stat_name;
		stats[i].flags = 0;
		stats[i].stat_pointer = &c->sipcapture_req;
		i++;
		stat_name = (char *)shm_malloc(sizeof (char) * (snprintf(NULL, 0 , (def)?"captured_replies%.*s":"captured_replies[%.*s]", (def)?0:c->name.len, (def)?"":c->name.s) + 1));
		sprintf(stat_name, (def)?"captured_replies%.*s":"captured_replies[%.*s]", (def)?0:c->name.len, (def)?"":c->name.s);
		stats[i].name = stat_name;
		stats[i].flags = 0;
		stats[i].stat_pointer = &c->sipcapture_rpl;
		i++;
		c=c->next;
	}
	stats[i].name = 0;
	stats[i].flags = 0;
	stats[i].stat_pointer = 0;

	sipcapture_stats = stats;

	/* register statistics */
	if (register_module_stats(exports.name, sipcapture_stats)!=0)
	{
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	srand(time(NULL));


	if(db_insert_mode) {
                LM_INFO("INFO: sipcapture: mod_init: you have enabled INSERT DELAYED \
                                Make sure your DB can support it\n");
        }

	capture_on_flag = (int*)shm_malloc(sizeof(int));
	if(capture_on_flag==NULL) {
		LM_ERR("no more shm memory left\n");
		return -1;
	}
	
	*capture_on_flag = capture_on;
	
	/* register DGRAM event */
	if(sr_event_register_cb(SREV_NET_DGRAM_IN, hep_msg_received) < 0) {
		LM_ERR("failed to register SREV_NET_DGRAM_IN event\n");
		return -1;		                	
	}

	if(ipip_capture_on && moni_capture_on) {
		LM_ERR("only one RAW mode is supported. Please disable ipip_capture_on or moni_capture_on\n");
		return -1;		                		
	}
	
	if ((insert_retries <0) || ( insert_retries > 500)) {
		LM_ERR("insert_retries should be a value between 0 and 500\n");
		return -1;
	}

	if  (( 0 == insert_retries) && (insert_retry_timeout != 0)){
		LM_ERR("insert_retry_timeout has no meaning when insert_retries is not set\n");
	}

	if ((insert_retry_timeout <0) || ( insert_retry_timeout > 300)) {
		LM_ERR("insert_retry_timeout should be a value between 0 and 300\n");
		return -1;
	}

	/* raw processes for IPIP encapsulation */
	if (ipip_capture_on || moni_capture_on) {
		register_procs(raw_sock_children);
		                		
		if(extract_host_port() && (((ip=str2ip(&raw_socket_listen)) == NULL)
		               && ((ip=str2ip6(&raw_socket_listen)) == NULL)
		         )) 
		{		
			LM_ERR("sipcapture mod_init: bad RAW IP: %.*s\n", raw_socket_listen.len, raw_socket_listen.s); 
			return -1;
		}		
			
        	if(moni_capture_on && !moni_port_start) {
	        	LM_ERR("ERROR:sipcapture:mod_init: Please define port/portrange in 'raw_socket_listen', before \
	        	                        activate monitoring capture\n");
        		return -1;		                		
                }			
			
		raw_sock_desc = raw_capture_socket(raw_socket_listen.len ? ip : 0, raw_interface.len ? &raw_interface : 0, 
		                                moni_port_start, moni_port_end , ipip_capture_on ? IPPROTO_IPIP : htons(0x0800));						         
						
		if(raw_sock_desc < 0) {
			LM_ERR("could not initialize raw udp socket:"
                                         " %s (%d)\n", strerror(errno), errno);
	                if (errno == EPERM)
        	        	LM_ERR("could not initialize raw socket on startup"
                	        	" due to inadequate permissions, please"
                        	        " restart as root or with CAP_NET_RAW\n");
                                
			return -1;		
		}

		if(promisc_on && raw_interface.len) {

			 memset(&ifr, 0, sizeof(ifr));
			 memcpy(ifr.ifr_name, raw_interface.s, raw_interface.len);


#ifdef __OS_linux			 			 
			 if(ioctl(raw_sock_desc, SIOCGIFFLAGS, &ifr) < 0) {
				LM_ERR("could not get flags from interface [%.*s]:"
                                         " %s (%d)\n", raw_interface.len, raw_interface.s, strerror(errno), errno);			 			 				 
				goto error;
			 }
			 
	                 ifr.ifr_flags |= IFF_PROMISC; 
	                 
	                 if (ioctl(raw_sock_desc, SIOCSIFFLAGS, &ifr) < 0) {
	                 	LM_ERR("could not set PROMISC flag to interface [%.*s]:"
                                         " %s (%d)\n", raw_interface.len, raw_interface.s, strerror(errno), errno);			 			 				 
				goto error;	                 
	                 }
#endif
	                 
		}		
	}

	return 0;
#ifdef __OS_linux			 			 
error:
	if(raw_sock_desc) close(raw_sock_desc);
	return -1;	
#endif
}

static int sipcapture_fixup(void** param, int param_no)
{

		_capture_mode_data_t *con;

		str val;
		unsigned int id;

        if (param_no == 1 ) {
                return fixup_var_pve_str_12(param, 1);
        }
        if (param_no == 2 ){

			val.s = (char *)*param;
			val.len = strlen((char *)*param);


			con = capture_modes_root;
			id = core_case_hash (&val, 0 , 0);
			while (con){
				if (id == con->id && con->name.len == val.len
						&& strncmp(con->name.s, val.s, val.len) == 0){
					*param = (void *)con;
					LM_DBG("found capture mode :[%.*s]\n",con->name.len, con->name.s);
					return 0;
				}
				con = con->next;
			}

			LM_ERR("no capture mode found\n");
			return -1;

        }
        
        return 0;
} 
   
static int w_sip_capture(struct sip_msg* _m, char* _table, _capture_mode_data_t * cm_data, char* s2)
{
        str table = {0};
        
        if(_table!=NULL && (get_str_fparam(&table, _m, (fparam_t*)_table) < 0))
        {
                LM_ERR("invalid table parameter [%s] [%s]\n", _table, table.s);
                return -1;
        }

        return sip_capture(_m, (table.len>0)?&table:NULL, cm_data );
}


int extract_host_port(void)
{
	if(raw_socket_listen.len) {
		char *p1,*p2;
		p1 = raw_socket_listen.s;
			
		if( (p1 = strrchr(p1, ':')) != 0 ) {
			 *p1 = '\0';
			 p1++;			 
			 p2=p1;
			 if((p2 = strrchr(p2, '-')) != 0 ) {
			 	p2++;
			 	moni_port_end = atoi(p2);
			 	p1[strlen(p1)-strlen(p2)-1]='\0';
			 }
			 moni_port_start = atoi(p1);
			 raw_socket_listen.len = strlen(raw_socket_listen.s);
		}									                                        									
		return 1;
	}
	return 0;
}


static int child_init(int rank)
{

	_capture_mode_data_t * c;

	if (rank == PROC_MAIN && (ipip_capture_on || moni_capture_on)) {
                if (init_rawsock_children() < 0) return -1;
        }

	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */


	c = capture_modes_root;

	while (c){
		if (!c->db_url.s || !c->db_url.len ){
			LM_ERR("DB URL not set for capture mode:[%.*s]\n", c->name.len, c->name.s);
			return -1;
		}
		c->db_con = c->db_funcs.init(&c->db_url);
		if (!c->db_con)
		{
			LM_ERR("unable to connect to database [%.*s] from capture_mode param.\n", c->db_url.len, c->db_url.s);
			return -1;
		}
	    if (c->mtmode ==mode_round_robin && rank > 0)
	    {
			c->rr_idx = rank % c->no_tables;
	    }
		c = c->next;
	}


	heptime = (struct hep_timehdr*)pkg_malloc(sizeof(struct hep_timehdr));
        if(heptime==NULL) {
                LM_ERR("no more pkg memory left\n");
                return -1;
        }
        
	heptime->tv_sec = 0;

	return 0;
}

/*
 * RAW IPIP || Monitoring listeners
 */
int init_rawsock_children(void)
{
        int i;
        pid_t pid;

        for(i = 0; i < raw_sock_children; i++) {
                pid = fork_process(PROC_UNIXSOCK,"homer raw socket", 1);
                if (pid < 0) {
                        ERR("Unable to fork: %s\n", strerror(errno));
                        return -1;
                } else if (pid == 0) { /* child */
			raw_capture_rcv_loop(raw_sock_desc, moni_port_start, moni_port_end, moni_capture_on ? 0 : 1);
                }
                /* Parent */
        }

        DBG("Raw IPIP socket server successfully initialized\n");
        return 1;
}


static void destroy(void)
{
	//if (capture_def->db_con!=NULL)
	//	capture_def->db_funcs.close(capture_def->db_con);

	/*free content from the linked list*/
	_capture_mode_data_t * c;
	_capture_mode_data_t * c0;

	c = capture_modes_root;

	while (c){
		c0 = c->next;
		if (c->name.s){
			pkg_free(c->name.s);
		}
		if (c->db_url.s){
			pkg_free(c->db_url.s);
		}
		if (c->db_con){
			c->db_funcs.close(c->db_con);
		}
		if (c->table_names){
			pkg_free(c->table_names);
		}

		pkg_free(c);
		c = c0;
	}

	if (capture_on_flag)
		shm_free(capture_on_flag);
		
        if(heptime) pkg_free(heptime);

	if(raw_sock_desc > 0) {
		 if(promisc_on && raw_interface.len) {
#ifdef __OS_linux
                         ifr.ifr_flags &= ~(IFF_PROMISC);

                         if (ioctl(raw_sock_desc, SIOCSIFFLAGS, &ifr) < 0) {
                                LM_ERR("destroy: could not remove PROMISC flag from interface [%.*s]:"
                                         " %s (%d)\n", raw_interface.len, raw_interface.s, strerror(errno), errno);
                         }
#endif                        
                }                		
		close(raw_sock_desc);
	}


//	if (table_names){
//		pkg_free(table_names);
//	}
}

static int sip_capture_prepare(sip_msg_t *msg)
{
        /* We need parse all headers */
        if (parse_headers(msg, HDR_CALLID_F|HDR_EOH_F, 0) != 0) {
                LM_ERR("cannot parse headers\n");
                return 0;
        }

        return 0;
}

static int sip_capture_store(struct _sipcapture_object *sco, str *dtable, _capture_mode_data_t * cm_data)
{
	db_key_t db_keys[NR_KEYS];
	db_val_t db_vals[NR_KEYS];

	str tmp, corrtmp;
	int ii = 0;
	int ret = 0;
	int counter = 0;
	db_insert_f insert;
	time_t retry_failed_time = 0;

	str *table = NULL;
	_capture_mode_data_t *c = NULL;

	c = (cm_data)? cm_data:capture_def;
	if (!c){
		LM_ERR("no connection mode available to store data\n");
		return -1;
	}

	if(sco==NULL)
	{
		LM_DBG("invalid parameter\n");
		return -1;
	}
	
	if(correlation_id) {
	         corrtmp.s = correlation_id;
	         corrtmp.len = strlen(correlation_id);        
        }
	
	db_keys[0] = &date_column;
	db_vals[0].type = DB1_DATETIME;
	db_vals[0].nul = 0;
	db_vals[0].val.time_val = time(NULL);
	
	db_keys[1] = &micro_ts_column;
        db_vals[1].type = DB1_BIGINT;
        db_vals[1].nul = 0;
        db_vals[1].val.ll_val = sco->tmstamp;
	
	db_keys[2] = &method_column;
	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;
	db_vals[2].val.str_val = sco->method;
	
	db_keys[3] = &reply_reason_column;
	db_vals[3].type = DB1_STR;
	db_vals[3].nul = 0;
	db_vals[3].val.str_val = sco->reply_reason;
	
	db_keys[4] = &ruri_column;
	db_vals[4].type = DB1_STR;
	db_vals[4].nul = 0;
	db_vals[4].val.str_val = sco->ruri;
	
	db_keys[5] = &ruri_user_column;
	db_vals[5].type = DB1_STR;
	db_vals[5].nul = 0;
	db_vals[5].val.str_val = sco->ruri_user;
	
	db_keys[6] = &from_user_column;
	db_vals[6].type = DB1_STR;
	db_vals[6].nul = 0;
	db_vals[6].val.str_val = sco->from_user;
	
	db_keys[7] = &from_tag_column;
	db_vals[7].type = DB1_STR;
	db_vals[7].nul = 0;
	db_vals[7].val.str_val = sco->from_tag;

	db_keys[8] = &to_user_column;
	db_vals[8].type = DB1_STR;
	db_vals[8].nul = 0;
	db_vals[8].val.str_val = sco->to_user;

	db_keys[9] = &to_tag_column;
	db_vals[9].type = DB1_STR;
	db_vals[9].nul = 0;
	db_vals[9].val.str_val = sco->to_tag;
	
	db_keys[10] = &pid_user_column;
	db_vals[10].type = DB1_STR;
	db_vals[10].nul = 0;
	db_vals[10].val.str_val = sco->pid_user;

	db_keys[11] = &contact_user_column;
	db_vals[11].type = DB1_STR;
	db_vals[11].nul = 0;
	db_vals[11].val.str_val = sco->contact_user;	

	db_keys[12] = &auth_user_column;
	db_vals[12].type = DB1_STR;
	db_vals[12].nul = 0;
	db_vals[12].val.str_val = sco->auth_user;
	
	db_keys[13] = &callid_column;
	db_vals[13].type = DB1_STR;
	db_vals[13].nul = 0;
	db_vals[13].val.str_val = sco->callid;

	db_keys[14] = &callid_aleg_column;
	db_vals[14].type = DB1_STR;
	db_vals[14].nul = 0;
	db_vals[14].val.str_val = sco->callid_aleg;
	
	db_keys[15] = &via_1_column;
	db_vals[15].type = DB1_STR;
	db_vals[15].nul = 0;
	db_vals[15].val.str_val = sco->via_1;
	
	db_keys[16] = &via_1_branch_column;
	db_vals[16].type = DB1_STR;
	db_vals[16].nul = 0;
	db_vals[16].val.str_val = sco->via_1_branch;

	db_keys[17] = &cseq_column;
	db_vals[17].type = DB1_STR;
	db_vals[17].nul = 0;
	db_vals[17].val.str_val = sco->cseq;	
	
	db_keys[18] = &reason_column;
	db_vals[18].type = DB1_STR;
	db_vals[18].nul = 0;
	db_vals[18].val.str_val = sco->reason;
	
	db_keys[19] = &content_type_column;
	db_vals[19].type = DB1_STR;
	db_vals[19].nul = 0;
	db_vals[19].val.str_val = sco->content_type;

	db_keys[20] = &authorization_column;
	db_vals[20].type = DB1_STR;
	db_vals[20].nul = 0;
	db_vals[20].val.str_val = sco->authorization;

	db_keys[21] = &user_agent_column;
	db_vals[21].type = DB1_STR;
	db_vals[21].nul = 0;
	db_vals[21].val.str_val = sco->user_agent;
	
	db_keys[22] = &source_ip_column;
	db_vals[22].type = DB1_STR;
	db_vals[22].nul = 0;
	db_vals[22].val.str_val = sco->source_ip;
	
	db_keys[23] = &source_port_column;
        db_vals[23].type = DB1_INT;
        db_vals[23].nul = 0;
        db_vals[23].val.int_val = sco->source_port;
        
	db_keys[24] = &dest_ip_column;
	db_vals[24].type = DB1_STR;
	db_vals[24].nul = 0;
	db_vals[24].val.str_val = sco->destination_ip;
	
	db_keys[25] = &dest_port_column;
        db_vals[25].type = DB1_INT;
        db_vals[25].nul = 0;
        db_vals[25].val.int_val = sco->destination_port;        
        
	db_keys[26] = &contact_ip_column;
	db_vals[26].type = DB1_STR;
	db_vals[26].nul = 0;
	db_vals[26].val.str_val = sco->contact_ip;
	
	db_keys[27] = &contact_port_column;
        db_vals[27].type = DB1_INT;
        db_vals[27].nul = 0;
        db_vals[27].val.int_val = sco->contact_port;
        
	db_keys[28] = &orig_ip_column;
	db_vals[28].type = DB1_STR;
	db_vals[28].nul = 0;
	db_vals[28].val.str_val = sco->originator_ip;
	
	db_keys[29] = &orig_port_column;			
        db_vals[29].type = DB1_INT;
        db_vals[29].nul = 0;
        db_vals[29].val.int_val = sco->originator_port;        
        
        db_keys[30] = &proto_column;			
        db_vals[30].type = DB1_INT;
        db_vals[30].nul = 0;
        db_vals[30].val.int_val = sco->proto;        

        db_keys[31] = &family_column;			
        db_vals[31].type = DB1_INT;
        db_vals[31].nul = 0;
        db_vals[31].val.int_val = sco->family;        
        
        db_keys[32] = &rtp_stat_column;			
        db_vals[32].type = DB1_STR;
        db_vals[32].nul = 0;
        db_vals[32].val.str_val = sco->rtp_stat;                
        
        db_keys[33] = &type_column;			
        db_vals[33].type = DB1_INT;
        db_vals[33].nul = 0;
        db_vals[33].val.int_val = sco->type;                

	db_keys[34] = &node_column;
	db_vals[34].type = DB1_STR;
	db_vals[34].nul = 0;
	db_vals[34].val.str_val = sco->node;

	db_keys[35] = &correlation_column;
	db_vals[35].type = DB1_STR;
	db_vals[35].nul = 0;
	db_vals[35].val.str_val = (correlation_id) ? corrtmp : sco->callid;	
	
	db_keys[36] = &msg_column;
	db_vals[36].type = DB1_BLOB;
	db_vals[36].nul = 0;

	/*we don't have empty spaces now */
	tmp.s = sco->msg.s;
	tmp.len = sco->msg.len;

	db_vals[36].val.blob_val = tmp;

	if (dtable){
		table = dtable;
	}

	else if (c->no_tables > 0 ){

		if ( c->mtmode == mode_hash ){
			ii = hash_func ( sco, c->hash_source , c->no_tables);
			if (ii < 0){
				LM_ERR("hashing failed\n");
				return -1;
			}
			LM_DBG ("hash idx is:%d\n", ii);
		}
		else if (c->mtmode == mode_random )
		{
			ii = rand() % c->no_tables;
			LM_DBG("rand idx is:%d\n", ii);
		}
		else if (c->mtmode == mode_round_robin)
		{
			ii = c->rr_idx;
			c->rr_idx = (c->rr_idx +1) % c->no_tables;
			LM_DBG("round robin idx is:%d\n", ii);
		}
		table = &c->table_names[ii];
	}


	/* check dynamic table */
	LM_DBG("insert into homer table: [%.*s]\n", table->len, table->s);
	c->db_funcs.use_table(c->db_con, table);

	LM_DBG("storing info...\n");

	if (db_insert_mode == 1 && c->db_funcs.insert_delayed != NULL)
		insert = c->db_funcs.insert_delayed;
	else
		insert = c->db_funcs.insert;
	ret = insert(c->db_con, db_keys, db_vals, NR_KEYS);

	if (ret < 0) {
		LM_DBG("failed to insert into database(first attempt)\n");
		if (insert_retries != 0) {
			counter = 0;
			while ((ret = insert(c->db_con, db_keys, db_vals, NR_KEYS)) < 0) {
				counter++;
				if (1 == counter) //first failed retry
					retry_failed_time = time(NULL);

				if ((counter > insert_retries) || (time(NULL)
						- retry_failed_time > insert_retry_timeout)) {
					LM_ERR("failed to insert into database(second attempt)\n");
					break;
				}
			}
		}
	}
	if (ret < 0)
		goto error;
#ifdef STATISTICS
	update_stat(sco->stat, 1);
#endif	

	return 1;
	error: return -1;
}

static int sip_capture(struct sip_msg *msg, str *_table, _capture_mode_data_t * cm_data)
{
	struct _sipcapture_object sco;
	struct sip_uri from, to, contact;
	struct hdr_field *hook1 = NULL;	 
	hdr_field_t *tmphdr[4];       
	contact_body_t*  cb=0;	        	        
	char buf_ip[IP_ADDR_MAX_STR_SIZE+12];
	char *port_str = NULL, *tmp = NULL;
	struct timeval tvb;
        struct timezone tz;
        char tmp_node[100];
        char rtpinfo[256];
        unsigned int len = 0;
	                                          
	LM_DBG("CAPTURE DEBUG...\n");

	gettimeofday( &tvb, &tz );
	        

	if(msg==NULL) {
		LM_DBG("nothing to capture\n");
		return -1;
	}
	memset(&sco, 0, sizeof(struct _sipcapture_object));


	if(capture_on_flag==NULL || *capture_on_flag==0) {
		LM_DBG("capture off...\n");
		return -1;
	}
	
	if(sip_capture_prepare(msg)<0) return -1;

	if(msg->first_line.type == SIP_REQUEST) {

		if (parse_sip_msg_uri(msg)<0) return -1;
	
		sco.method = msg->first_line.u.request.method;
		EMPTY_STR(sco.reply_reason);
		
		sco.ruri = msg->first_line.u.request.uri;
		sco.ruri_user = msg->parsed_uri.user;		
	}
	else if(msg->first_line.type == SIP_REPLY) {
		sco.method = msg->first_line.u.reply.status;
		sco.reply_reason = msg->first_line.u.reply.reason;

		EMPTY_STR(sco.ruri);
		EMPTY_STR(sco.ruri_user);		
	}
	else {		
		LM_ERR("unknown type [%i]\n", msg->first_line.type);
		EMPTY_STR(sco.method);
		EMPTY_STR(sco.reply_reason);
		EMPTY_STR(sco.ruri);
		EMPTY_STR(sco.ruri_user);
	}

	if(heptime && heptime->tv_sec != 0) {
               sco.tmstamp = (unsigned long long)heptime->tv_sec*1000000+heptime->tv_usec; /* micro ts */
               snprintf(tmp_node, 100, "%.*s:%i", capture_node.len, capture_node.s, heptime->captid);
               sco.node.s = tmp_node;
               sco.node.len = strlen(tmp_node);
        }
        else {
               sco.tmstamp = (unsigned long long)tvb.tv_sec*1000000+tvb.tv_usec; /* micro ts */
               sco.node = capture_node;
        }
	
	/* Parse FROM */
        if(msg->from) {

              if (parse_from_header(msg)!=0){
                   LOG(L_ERR, "ERROR: eval_elem: bad or missing" " From: header\n");
                   return -1;
              }

              if (parse_uri(get_from(msg)->uri.s, get_from(msg)->uri.len, &from)<0){
                   LOG(L_ERR, "ERROR: do_action: bad from dropping"" packet\n");
                   return -1;
              }
              
              sco.from_user = from.user;
              sco.from_tag = get_from(msg)->tag_value;              
        }
        else {
		EMPTY_STR(sco.from_user);
		EMPTY_STR(sco.from_tag);
        }

        /* Parse TO */
        if(msg->to) {

              if (parse_uri(get_to(msg)->uri.s, get_to(msg)->uri.len, &to)<0){
                    LOG(L_ERR, "ERROR: do_action: bad to dropping"" packet\n");
                    return -1;
              }
        
              sco.to_user = to.user;
              if(get_to(msg)->tag_value.len) 
              		sco.to_tag = get_to(msg)->tag_value;              
              else { EMPTY_STR(sco.to_tag); }
        }
        else {        
        	EMPTY_STR(sco.to_user);
        	EMPTY_STR(sco.to_tag);
        }

	/* Call-id */
	if(msg->callid) sco.callid = msg->callid->body;
	else { EMPTY_STR(sco.callid); }

	/* P-Asserted-Id */
	if((parse_pai_header(msg) == 0) && (msg->pai) && (msg->pai->parsed)) {
		to_body_t *pai = get_pai(msg)->id; /* This returns the first entry */
		if ((pai->parsed_uri.user.s == NULL) &&
			(parse_uri(pai->uri.s, pai->uri.len, &pai->parsed_uri) < 0)){
			LM_DBG("DEBUG: do_action: bad pai: method:[%.*s] CID: [%.*s]\n", sco.method.len, sco.method.s, sco.callid.len, sco.callid.s);
		}
		else {
			LM_DBG("PARSE PAI: (%.*s)\n", pai->uri.len, pai->uri.s);
			sco.pid_user = pai->parsed_uri.user;
		}
	}
	else if((parse_ppi_header(msg) == 0) && (msg->ppi) && (msg->ppi->parsed)) {
		to_body_t *ppi = get_ppi(msg)->id; /* This returns the first entry */
		if ((ppi->parsed_uri.user.s == NULL) &&
			(parse_uri(ppi->uri.s, ppi->uri.len, &ppi->parsed_uri) < 0)){
			LM_DBG("DEBUG: do_action: bad ppi: method:[%.*s] CID: [%.*s]\n", sco.method.len, sco.method.s, sco.callid.len, sco.callid.s);
		}
		else {
			LM_DBG("PARSE PPI: (%.*s)\n", ppi->uri.len, ppi->uri.s);
			sco.pid_user = ppi->parsed_uri.user;
		}
	}
	else { EMPTY_STR(sco.pid_user); }
	
	/* Auth headers */
        if(msg->proxy_auth != NULL) hook1 = msg->proxy_auth;
        else if(msg->authorization != NULL) hook1 = msg->authorization;

        if(hook1) {
               if(parse_credentials(hook1) == 0)  sco.auth_user = ((auth_body_t*)(hook1->parsed))->digest.username.user;               
               else { EMPTY_STR(sco.auth_user); }
        }
        else { EMPTY_STR(sco.auth_user);}

	if(msg->contact) {

              if (msg->contact->parsed == 0 && parse_contact(msg->contact) == -1) {
                     LOG(L_ERR,"assemble_msg: error while parsing <Contact:> header\n");
                     return -1;
              }

              cb = (contact_body_t*)msg->contact->parsed;

              if(cb) {
            	    if (cb->contacts) {
			if(parse_uri( cb->contacts->uri.s, cb->contacts->uri.len, &contact)<0){
                		LOG(L_ERR, "ERROR: do_action: bad contact dropping"" packet\n");
                 	    	return -1;
                  	}
              	    } else {
              		if(cb->star){ /* in the case Contact is "*" */
			    memset(&contact, 0, sizeof(contact));
			    contact.user.s =  star_contact.s;
			    contact.user.len = star_contact.len;
			} else {
			    LOG(L_NOTICE,"Invalid contact\n");
			    memset(&contact, 0, sizeof(contact));
			}
		    }
	    }
        }

	/* callid_aleg - default is X-CID but configurable via modul params */
        if((tmphdr[0] = get_hdr_by_name(msg, callid_aleg_header.s, callid_aleg_header.len)) != NULL) {	
		sco.callid_aleg = tmphdr[0]->body;
        }
	else { EMPTY_STR(sco.callid_aleg);}
		
	/* VIA 1 */
	sco.via_1 = msg->h_via1->body;

	/* Via branch */
	if(msg->via1->branch) sco.via_1_branch = msg->via1->branch->value;
	else { EMPTY_STR(sco.via_1_branch); }
	
	/* CSEQ */	
	if(msg->cseq) sco.cseq = msg->cseq->body;
	else { EMPTY_STR(sco.cseq); }
	
	/* Reason */	
	if((tmphdr[1] = get_hdr_by_name(msg,"Reason", 6)) != NULL) {
		sco.reason =  tmphdr[1]->body;
	}	                         	
	else { EMPTY_STR(sco.reason); }

	/* Diversion */	
	if(msg->diversion) sco.diversion = msg->diversion->body;
	else { EMPTY_STR(sco.diversion);}
	
	/* Content-type */	
	if(msg->content_type) sco.content_type = msg->content_type->body;
	else { EMPTY_STR(sco.content_type);}
	
	/* User-Agent */	
	if(msg->user_agent) sco.user_agent = msg->user_agent->body;
	else { EMPTY_STR(sco.user_agent);}

	/* Contact */	
	if(msg->contact && cb) {
		sco.contact_ip = contact.host;
		str2int(&contact.port, (unsigned int*)&sco.contact_port);
		sco.contact_user = contact.user;
	}
	else {
		EMPTY_STR(sco.contact_ip);	
		sco.contact_port = 0;
		EMPTY_STR(sco.contact_user);
	}
	
	/* X-OIP */	
	if((tmphdr[2] = get_hdr_by_name(msg,"X-OIP", 5)) != NULL) {
		sco.originator_ip = tmphdr[2]->body;
		/* Originator port. Should be parsed from XOIP header as ":" param */
		tmp = strchr(tmphdr[2]->body.s, ':');
	        if (tmp) {
			*tmp = '\0';
	                port_str = tmp + 1;
			sco.originator_port = strtol(port_str, NULL, 10);
		}
		else sco.originator_port = 0;		
	}
	else {
		EMPTY_STR(sco.originator_ip);
		sco.originator_port = 0;
	}	
	
	/* X-RTP-Stat */	
	if((tmphdr[3] = get_hdr_by_name(msg,"X-RTP-Stat", 10)) != NULL) {
		sco.rtp_stat =  tmphdr[3]->body;
	}	                         
	/* P-RTP-Stat */	
	else if((tmphdr[3] = get_hdr_by_name(msg,"P-RTP-Stat", 10)) != NULL) {
		sco.rtp_stat =  tmphdr[3]->body;
	}	                         	
	/* X-Siemens-RTP-stats */	
	else if((tmphdr[3] = get_hdr_by_name(msg,"X-Siemens-RTP-stats", 19)) != NULL) {
		sco.rtp_stat =  tmphdr[3]->body;
	}	                         
	/* X-NG-RTP-STATS */	
	else if((tmphdr[3] = get_hdr_by_name(msg,"X-NG-RTP-STATS", 14)) != NULL) {
		sco.rtp_stat =  tmphdr[3]->body;
	}	                         	
	/* RTP-RxStat */
        else if((tmphdr[3] = get_hdr_by_name(msg,"RTP-RxStat", 10)) != NULL) {
                if(tmphdr[3]->body.len > 250) tmphdr[3]->body.len = 250;

                memcpy(&rtpinfo, tmphdr[3]->body.s, tmphdr[3]->body.len);
                len = tmphdr[3]->body.len;
                if((tmphdr[3] = get_hdr_by_name(msg,"RTP-TxStat", 10)) != NULL) {
                        memcpy(&rtpinfo[len], ", ", 2);
                        if((len + 2 + tmphdr[3]->body.len) > 256) tmphdr[3]->body.len = 256 - (len+2);
                        memcpy(&rtpinfo[len+2], tmphdr[3]->body.s, tmphdr[3]->body.len);
                }
                sco.rtp_stat.s =  rtpinfo;
                sco.rtp_stat.len =  strlen(rtpinfo);
        }


	else { EMPTY_STR(sco.rtp_stat); }	
	
		
	/* PROTO TYPE */
	sco.proto = msg->rcv.proto;
	
	/* FAMILY TYPE */
	sco.family = msg->rcv.src_ip.af;
	
	/* MESSAGE TYPE */
	sco.type = msg->first_line.type;
	
	/* MSG */	
	sco.msg.s = msg->buf;
	sco.msg.len = msg->len;	        
	//EMPTY_STR(sco.msg);
                 
	/* IP source and destination */
	
	strcpy(buf_ip, ip_addr2a(&msg->rcv.src_ip));
	sco.source_ip.s = buf_ip;
	sco.source_ip.len = strlen(buf_ip);
        sco.source_port = msg->rcv.src_port;	

        /*source ip*/
	sco.destination_ip.s = ip_addr2a(&msg->rcv.dst_ip);
	sco.destination_ip.len = strlen(sco.destination_ip.s);
	sco.destination_port = msg->rcv.dst_port;
	
        
        LM_DBG("src_ip: [%.*s]\n", sco.source_ip.len, sco.source_ip.s);
        LM_DBG("dst_ip: [%.*s]\n", sco.destination_ip.len, sco.destination_ip.s);
                 
        LM_DBG("dst_port: [%d]\n", sco.destination_port);
        LM_DBG("src_port: [%d]\n", sco.source_port);
        
#ifdef STATISTICS
	if(msg->first_line.type==SIP_REPLY) {
		sco.stat = (cm_data)?cm_data->sipcapture_rpl:capture_def->sipcapture_rpl;
	} else {
		sco.stat = (cm_data)?cm_data->sipcapture_req:capture_def->sipcapture_req;
	}
#endif
	//LM_DBG("DONE");
	return sip_capture_store(&sco, _table, cm_data);
}

#define capture_is_off(_msg) \
	(capture_on_flag==NULL || *capture_on_flag==0)


/*! \brief
 * MI Sip_capture command
 *
 * MI command format:
 * name: sip_capture
 * attribute: name=none, value=[on|off]
 */
static struct mi_root* sip_capture_mi(struct mi_root* cmd_tree, void* param )
{
	struct mi_node* node;
	
	struct mi_node *rpl; 
	struct mi_root *rpl_tree ; 

	node = cmd_tree->node.kids;
	if(node == NULL) {
		rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
		if (rpl_tree == 0)
			return 0;
		rpl = &rpl_tree->node;

		if (*capture_on_flag == 0 ) {
			node = add_mi_node_child(rpl,0,0,0,MI_SSTR("off"));
		} else if (*capture_on_flag == 1) {
			node = add_mi_node_child(rpl,0,0,0,MI_SSTR("on"));
		}
		return rpl_tree ;
	}
	if(capture_on_flag==NULL)
		return init_mi_tree( 500, MI_SSTR(MI_INTERNAL_ERR));

	if ( node->value.len==2 && (node->value.s[0]=='o'
				|| node->value.s[0]=='O') &&
			(node->value.s[1]=='n'|| node->value.s[1]=='N')) {
		*capture_on_flag = 1;
		return init_mi_tree( 200, MI_SSTR(MI_OK));
	} else if ( node->value.len==3 && (node->value.s[0]=='o'
				|| node->value.s[0]=='O')
			&& (node->value.s[1]=='f'|| node->value.s[1]=='F')
			&& (node->value.s[2]=='f'|| node->value.s[2]=='F')) {
		*capture_on_flag = 0;
		return init_mi_tree( 200, MI_SSTR(MI_OK));
	} else {
		return init_mi_tree( 400, MI_SSTR(MI_BAD_PARM));
	}
}

/* Local raw socket */
int raw_capture_socket(struct ip_addr* ip, str* iface, int port_start, int port_end, int proto)
{

	int sock = -1;	
	union sockaddr_union su;

#ifdef __OS_linux
	struct sock_fprog pf;
	char short_ifname[sizeof(int)];
	int ifname_len;
	char* ifname;
#endif 
 	//0x0003 - all packets
 	if(proto == IPPROTO_IPIP) {
        	sock = socket(PF_INET, SOCK_RAW, proto);
        }
#ifdef __OS_linux
 	else if(proto == htons(0x800)) {
        	sock = socket(PF_PACKET, SOCK_RAW, proto);
        }
#endif
        else {
                ERR("raw_capture_socket: LSF currently supported only on linux\n");
                goto error;                        
        }
                
	if (sock==-1)
		goto error;

#ifdef __OS_linux

	/* set socket options */
	if (iface && iface->s){

		/* workaround for linux bug: arg to setsockopt must have at least
		 * sizeof(int) size or EINVAL would be returned */
		if (iface->len<sizeof(int)){
			memcpy(short_ifname, iface->s, iface->len);
			short_ifname[iface->len]=0; /* make sure it's zero term */
			ifname_len=sizeof(short_ifname);
			ifname=short_ifname;
		}else{
			ifname_len=iface->len;
			ifname=iface->s;
		}
		if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifname, ifname_len) <0){
				ERR("raw_socket: could not bind to %.*s: %s [%d]\n",
							iface->len, ZSW(iface->s), strerror(errno), errno);
				goto error;
		}
	}

	if(bpf_on) {

		memset(&pf, 0, sizeof(pf));
	        pf.len = sizeof(BPF_code) / sizeof(BPF_code[0]);
        	pf.filter = (struct sock_filter *) BPF_code;

                if(!port_end) port_end = port_start;
                
        	/* Start PORT */
        	BPF_code[5]  = (struct sock_filter)BPF_JUMP(0x35, port_start, 0, 1);
        	BPF_code[8] = (struct  sock_filter)BPF_JUMP(0x35, port_start, 11, 13);
        	BPF_code[16] = (struct sock_filter)BPF_JUMP(0x35, port_start, 0, 1);
        	BPF_code[19] = (struct sock_filter)BPF_JUMP(0x35, port_start, 0, 2);
        	/* Stop PORT */
        	BPF_code[6]  = (struct sock_filter)BPF_JUMP(0x25, port_end, 0, 14);
        	BPF_code[17] = (struct sock_filter)BPF_JUMP(0x25, port_end, 0, 3);	
        	BPF_code[20] = (struct sock_filter)BPF_JUMP(0x25, port_end, 1, 0);			                                                
	
        	/* Attach the filter to the socket */
        	if(setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &pf, sizeof(pf)) < 0 ) {
                        ERR(" setsockopt filter: [%s] [%d]\n", strerror(errno), errno);
                }		
        }
#endif

        if (ip && proto == IPPROTO_IPIP){
                init_su(&su, ip, 0);
                if (bind(sock, &su.s, sockaddru_len(su))==-1){
                        ERR("raw_capture_socket: bind(%s) failed: %s [%d]\n",
                                ip_addr2a(ip), strerror(errno), errno);
                        goto error;
                }
        }

	return sock;
	
error:
	if (sock!=-1) close(sock);
	return -1;		
               		
}

/* Local raw receive loop */
int raw_capture_rcv_loop(int rsock, int port1, int port2, int ipip) {


	static char buf [BUF_SIZE+1];
	union sockaddr_union from;
	union sockaddr_union to;
        struct receive_info ri;
	int len;
	struct ip *iph;
        struct udphdr *udph;
        char* udph_start;
        unsigned short udp_len;
	int offset = 0; 
	char* end;
	unsigned short dst_port;
	unsigned short src_port;
	struct ip_addr dst_ip, src_ip;
	struct socket_info* si = 0;
	int tmp_len;
	

	for(;;){

		len = recvfrom(rsock, buf, BUF_SIZE, 0x20, 0, 0);

		if (len<0){
                        if (len==-1){
                                LOG(L_ERR, "ERROR: raw_moni_rcv_loop:recvfrom: %s [%d]\n",
                                                strerror(errno), errno);
                                if ((errno==EINTR)||(errno==EWOULDBLOCK))
                                        continue;
                        }else{
                                DBG("raw_moni_rcv_loop: recvfrom error: %d\n", len);
                                continue;
                        }
                }

		end=buf+len;
		
		offset =  ipip ? sizeof(struct ip) : ETHHDR;
		
		if (unlikely(len<(sizeof(struct ip)+sizeof(struct udphdr) + offset))) {
			DBG("received small packet: %d. Ignore it\n",len);
                	continue;
        	}
		
		iph = (struct ip*) (buf + offset);				

		offset+=iph->ip_hl*4;

		udph_start = buf+offset;
		
		udph = (struct udphdr*) udph_start;
		offset +=sizeof(struct udphdr);

        	if (unlikely((buf+offset)>end)){
                	continue;	                
        	}

		udp_len=ntohs(udph->uh_ulen);
	        if (unlikely((udph_start+udp_len)!=end)){
        	        if ((udph_start+udp_len)>end){
				continue;
        	        }else{
                	        DBG("udp length too small: %d/%d\n", (int)udp_len, (int)(end-udph_start));
	                        continue;
        	        }
	        }
        									
		/* cut off the offset */
	        len -= offset;

		if (len<MIN_UDP_PACKET){
                        DBG("raw_udp4_rcv_loop: probing packet received from\n");
                        continue;
                }

                /* fill dst_port && src_port */
                dst_port=ntohs(udph->uh_dport);
                src_port=ntohs(udph->uh_sport);
                                              
                /* if the message has not alpha */
                if(!isalnum((buf+offset)[0])) {
                        DBG("not alpha and not digit... skiping...\n");
                        continue;
                }
                                                        

                DBG("PORT: [%d] and [%d]\n", port1, port2);
                
		if((!port1 && !port2) || (src_port >= port1 && src_port <= port2) 
		        || (dst_port >= port1 && dst_port <= port2) 
		        || (!port2 && (src_port == port1 || dst_port == port1))) {
		        
        		/*FIL IPs*/
        		dst_ip.af=AF_INET;
        	        dst_ip.len=4;
                	dst_ip.u.addr32[0]=iph->ip_dst.s_addr;

	                /* fill dst_port */
        	        ip_addr2su(&to, &dst_ip, dst_port);

                	/* fill src_port */
                        src_ip.af=AF_INET;
         	        src_ip.len=4;
                        src_ip.u.addr32[0]=iph->ip_src.s_addr;
                        ip_addr2su(&from, &src_ip, src_port);
        	        su_setport(&from, src_port);
	
        		ri.src_su=from;
                        su2ip_addr(&ri.src_ip, &from);
                        ri.src_port=src_port;
                        su2ip_addr(&ri.dst_ip, &to);
                        ri.dst_port=dst_port;
                        ri.proto=PROTO_UDP;                                

                        /* a little bit memory */                
                        si=(struct socket_info*) pkg_malloc(sizeof(struct socket_info));
                        if (si==0) {                                
                                LOG(L_ERR, "ERROR: new_sock_info: memory allocation error\n");
                                return 0;
                        }
                        
                        memset(si, 0, sizeof(struct socket_info));                
                        si->address = ri.dst_ip; 
                        si->socket=-1;

	                /* set port & proto */
                	si->port_no = dst_port;
	                si->proto=PROTO_UDP;
                	si->flags=0;
	                si->addr_info_lst=0;
	                
        	        si->port_no_str.s = int2str(si->port_no, &tmp_len);
        	        si->port_no_str.len = tmp_len;
	        
        	        si->address_str.s = ip_addr2a(&si->address);;
                        si->address_str.len = strlen(si->address_str.s);	                        
	        
        	        si->name.len = si->address_str.len;
	                si->name.s = si->address_str.s;

     	                ri.bind_address=si;		        		        
		        

     	                /* and now recieve message */
        		receive_msg(buf+offset, len, &ri);		                          
	        	if(si) pkg_free(si);                         
                }                                
	}

	return 0;
}

static void sipcapture_rpc_status (rpc_t* rpc, void* c) {
	str status = {0, 0};

	if (rpc->scan(c, "S", &status) < 1) {
		rpc->fault(c, 500, "Not enough parameters (on, off or check)");
		return;
	}

	if(capture_on_flag==NULL) {
		rpc->fault(c, 500, "Internal error");
		return;
	}

	if (strncasecmp(status.s, "on", strlen("on")) == 0) {
		*capture_on_flag = 1;
		rpc->rpl_printf(c, "Enabled");
		return;
	}
	if (strncasecmp(status.s, "off", strlen("off")) == 0) {
		*capture_on_flag = 0;
		rpc->rpl_printf(c, "Disabled");
		return;
	}
	if (strncasecmp(status.s, "check", strlen("check")) == 0) {
		rpc->rpl_printf(c, *capture_on_flag ? "Enabled" : "Disabled");
		return;
	} 
	rpc->fault(c, 500, "Bad parameter (on, off or check)");
	return;
}

static const char* sipcapture_status_doc[2] = {
        "Get status or turn on/off sipcapture.",
        0
};

rpc_export_t sipcapture_rpc[] = {
	{"sipcapture.status", sipcapture_rpc_status, sipcapture_status_doc, 0},
	{0, 0, 0, 0}
};

static int sipcapture_init_rpc(void)
{
	if (rpc_register_array(sipcapture_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}



/* for rtcp and logging */
int receive_logging_json_msg(char * buf, unsigned int len, struct hep_generic_recv *hg, char *log_table) {


	db_key_t db_keys[RTCP_NR_KEYS];
	db_val_t db_vals[RTCP_NR_KEYS];
        struct _sipcapture_object sco;
	char ipstr_dst[INET6_ADDRSTRLEN], ipstr_src[INET6_ADDRSTRLEN];
	char tmp_node[100];
        struct timeval tvb;
        struct timezone tz;    
        time_t epoch_time_as_time_t;
        
	str tmp, corrtmp, table;
	_capture_mode_data_t *c = NULL;

	c = capture_def;
	if (!c){
		LM_ERR("no connection mode available to store data\n");
		return -1;
	}

	memset(&sco, 0, sizeof(struct _sipcapture_object));
	gettimeofday( &tvb, &tz );

        /* PROTO TYPE */
        if(hg->ip_proto->data == IPPROTO_TCP) sco.proto=PROTO_TCP;
        else if(hg->ip_proto->data == IPPROTO_UDP) sco.proto=PROTO_UDP;
        /* FAMILY TYPE */
        sco.family = hg->ip_family->data;

        /* IP source and destination */

	if ( hg->ip_family->data == AF_INET6 ) {
        	inet_ntop(AF_INET6, &(hg->hep_dst_ip6->data), ipstr_dst, INET6_ADDRSTRLEN);	       
        	inet_ntop(AF_INET6, &(hg->hep_src_ip6->data), ipstr_src, INET6_ADDRSTRLEN);	       
        }
	else if ( hg->ip_family->data == AF_INET ) {
        	inet_ntop(AF_INET, &(hg->hep_src_ip4->data), ipstr_src, INET_ADDRSTRLEN);
        	inet_ntop(AF_INET, &(hg->hep_dst_ip4->data), ipstr_dst, INET_ADDRSTRLEN);
	}


        /*source ip*/
        sco.source_ip.s = ipstr_src;
        sco.source_ip.len = strlen(ipstr_src);
        sco.source_port = hg->src_port->data;        
        
        sco.destination_ip.s = ipstr_dst;
        sco.destination_ip.len = strlen(ipstr_dst);
        sco.destination_port = hg->dst_port->data;

	if(heptime && heptime->tv_sec != 0) {
               sco.tmstamp = (unsigned long long)heptime->tv_sec*1000000+heptime->tv_usec; /* micro ts */
               snprintf(tmp_node, 100, "%.*s:%i", capture_node.len, capture_node.s, heptime->captid);
               sco.node.s = tmp_node;
               sco.node.len = strlen(tmp_node);
               epoch_time_as_time_t = heptime->tv_sec;;
        }
        else {
               sco.tmstamp = (unsigned long long)tvb.tv_sec*1000000+tvb.tv_usec; /* micro ts */
               sco.node = capture_node;
               epoch_time_as_time_t = tvb.tv_sec;
        }

        if(correlation_id) {
                corrtmp.s = correlation_id;
                corrtmp.len = strlen(correlation_id);
                if(!strncmp(log_table, "rtcp_capture",12)) corrtmp.len--;
        }

	db_keys[0] = &date_column;
	db_vals[0].type = DB1_DATETIME;
	db_vals[0].nul = 0;
	db_vals[0].val.time_val = epoch_time_as_time_t;
	
	db_keys[1] = &micro_ts_column;
        db_vals[1].type = DB1_BIGINT;
        db_vals[1].nul = 0;
        db_vals[1].val.ll_val = sco.tmstamp;
	
	db_keys[2] = &correlation_column;
	db_vals[2].type = DB1_STR;
	db_vals[2].nul = 0;
	db_vals[2].val.str_val = corrtmp;
	
	db_keys[3] = &source_ip_column;
	db_vals[3].type = DB1_STR;
	db_vals[3].nul = 0;
	db_vals[3].val.str_val = sco.source_ip;
	
	db_keys[4] = &source_port_column;
        db_vals[4].type = DB1_INT;
        db_vals[4].nul = 0;
        db_vals[4].val.int_val = sco.source_port;
        
	db_keys[5] = &dest_ip_column;
	db_vals[5].type = DB1_STR;
	db_vals[5].nul = 0;
	db_vals[5].val.str_val = sco.destination_ip;
	
	db_keys[6] = &dest_port_column;
        db_vals[6].type = DB1_INT;
        db_vals[6].nul = 0;
        db_vals[6].val.int_val = sco.destination_port;        
        
        db_keys[7] = &proto_column;			
        db_vals[7].type = DB1_INT;
        db_vals[7].nul = 0;
        db_vals[7].val.int_val = sco.proto;        

        db_keys[8] = &family_column;			
        db_vals[8].type = DB1_INT;
        db_vals[8].nul = 0;
        db_vals[8].val.int_val = sco.family;        
        
        db_keys[9] = &type_column;			
        db_vals[9].type = DB1_INT;
        db_vals[9].nul = 0;
        db_vals[9].val.int_val = sco.type;                

	db_keys[10] = &node_column;
	db_vals[10].type = DB1_STR;
	db_vals[10].nul = 0;
	db_vals[10].val.str_val = sco.node;
	
	db_keys[11] = &msg_column;
	db_vals[11].type = DB1_BLOB;
	db_vals[11].nul = 0;
		
	tmp.s = buf;
	tmp.len = len;
	
	db_vals[11].val.blob_val = tmp;

	table.s = log_table;
	table.len = strlen(log_table);

	c->db_funcs.use_table(c->db_con, &table);

	if(db_insert_mode==1 && c->db_funcs.insert_delayed!=NULL) {
                if (c->db_funcs.insert_delayed(c->db_con, db_keys, db_vals, RTCP_NR_KEYS) < 0) {
                	LM_ERR("failed to insert delayed into database\n");
                        goto error;
                }
        } else if (c->db_funcs.insert(c->db_con, db_keys, db_vals, RTCP_NR_KEYS) < 0) {
		LM_ERR("failed to insert into database\n");
                goto error;               
	}
                
	
	return 1;
error:
	return -1;
}


