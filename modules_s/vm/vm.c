/*
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *
 * History:
 * -------
 * 2003-03-11 New module interface (janakj)
 * 2003-03-16 flags export parameter added (janakj)
 * 2003-03-06 vm_{start|stop} changed to use a single fifo 
 *            function; new module parameters introduced;
 *            db now initialized only on start-up; MULTI_DOMAIN
 *            support introduced; snprintf removed; (rco)
 * 2003-12-14 vm_action adapted be called also from failure route; all allocs
 *            made by parser are freed on exit (bogdan)
 * 2003-12-14 in write_to_vm_fifo() "for"+"malloc"+"write" replaced by a single
 *            "writev" (bogdan)
 * 2004-04-07 Added Radius support (jih)
 * 2004-06-07 updated to the new db api (andrei)
 *
 */

#include "../../fifo_server.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../config.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_nameaddr.h"
#include "../../parser/parser_f.h"
#include "../../parser/contact/parse_contact.h"

#ifdef WITH_DB_SUPPORT
#include "../../db/db.h"
#endif

#ifdef WITH_RADIUS_SUPPORT
#include <radiusclient.h>
#include "../acc/dict.h"
#endif

#include "../tm/tm_load.h"
//#include "../tm/t_reply.h"

#include "vm_fifo.h"
#include "defs.h"
#include "vm.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/uio.h>

#define append_str(_dest,_src,_len) \
	do{\
		memcpy( (_dest) , (_src) , (_len) );\
		(_dest) += (_len) ;\
	}while(0);

#define VM_FIFO_PARAMS   21

#define IDBUF_LEN	 128
#define ROUTE_BUFFER_MAX 512
#define HDRS_BUFFER_MAX  512
#define CMD_BUFFER_MAX   128

MODULE_VERSION

static str empty_param = {".",1};

static int write_to_vm_fifo(char *fifo, int cnt );
static int init_tmb();
static int vm_action_req(struct sip_msg*, char* fifo, char *action);
static int vm_action_fld(struct sip_msg*, char* fifo, char *action);
static int vm_action(struct sip_msg* msg, char* fifo, char *action, int mode);
static int vmt_action(struct sip_msg*, char* fifo, char*);
static int vm_mod_init(void);
static int vm_init_child(int rank);

struct tm_binds _tmb;

#ifndef WITH_DB_SUPPORT
#define MAX_EMAIL_SIZE  64
char email_buf[MAX_EMAIL_SIZE];
#endif

char language[2];

#ifdef WITH_LDAP_SUPPORT
typedef int (*ldap_get_ui_t)(str*, str*);
ldap_get_ui_t ldap_get_ui = NULL;
#endif

#ifdef WITH_RADIUS_SUPPORT
static char *radius_config = "/usr/local/etc/radiusclient/radiusclient.conf";
static int service_type = -1;
void *rh;
struct attr attrs[A_MAX];
struct val vals[V_MAX];
#endif

#ifdef WITH_DB_SUPPORT
static char* vm_db_url = 0;                     /* Database URL */
char* email_column = "email_address";
char* subscriber_table = "subscriber" ;

char* user_column = "username";
char* domain_column = "domain";

static db_con_t* db_handle = 0;
static db_func_t vm_dbf;
#endif

int use_domain = 0;

/* #define EXTRA_DEBUG */

static str       lines_eol[2*VM_FIFO_PARAMS];
static str       eol={"\n",1};

#define get_from(p_msg)      ((struct to_body*)(p_msg)->from->parsed)
#define eol_line(_i_)        ( lines_eol[2*(_i_)] )

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"vm", vm_action_req, 2, 0, REQUEST_ROUTE },
	{"vm", vm_action_fld, 2, 0, FAILURE_ROUTE },
	{"vmt", vmt_action, 2, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
#ifdef WITH_DB_SUPPORT
	{"db_url",           STR_PARAM, &vm_db_url       },
	{"email_column",     STR_PARAM, &email_column    },
	{"subscriber_table", STR_PARAM, &subscriber_table},
	{"user_column",      STR_PARAM, &user_column     },
	{"domain_column",    STR_PARAM, &domain_column   },
#endif
#ifdef WITH_RADIUS_SUPPORT
	{"radius_config",    STR_PARAM, &radius_config   },
	{"service_type",     INT_PARAM, &service_type    },
#endif
	{"use_domain",       INT_PARAM, &use_domain      },
	{0, 0, 0}
};


struct module_exports exports = {
    "vm", 
    cmds,         /* Exported commands */
    params,       /* Exported parameters */
    vm_mod_init,  /* module initialization function */
    0,            /* response function*/
    0,            /* destroy function */
    0,            /* oncancel function */
    vm_init_child /* per-child init function */
};


static int vm_mod_init(void)
{
	int i;

	fprintf(stderr, "voicemail - initializing\n");

	if (register_fifo_cmd(fifo_vm_reply, "vm_reply", 0)<0) {
		LOG(L_CRIT, "cannot register fifo vm_reply\n");
		return -1;
	}

	if (init_tmb()==-1) {
		LOG(L_ERR, "Error: vm_mod_init: cann't load tm\n");
		return -1;
	}

#ifdef WITH_LDAP_SUPPORT
        ldap_get_ui = (ldap_get_ui_t)find_export("ldap_get_uinfo", 0, 0);

        if (!ldap_get_ui)
        {
                LOG(L_ERR, "ERROR: vm_mod_init: This module requires auth_ldap module\n");
                return -1;
        }
#endif

#ifdef WITH_DB_SUPPORT
	/* init database support only if needed */
	if (vm_db_url && bind_dbmod(vm_db_url, &vm_dbf)) {
		LOG(L_ERR, "ERROR: vm_mod_init: unable to bind db\n");
		return -1;
	}
#endif

#ifdef WITH_RADIUS_SUPPORT
	memset(attrs, 0, sizeof(attrs));
	memset(attrs, 0, sizeof(vals));
	attrs[A_SERVICE_TYPE].n			= "Service-Type";
	attrs[A_USER_NAME].n	                = "User-Name";
	attrs[A_VM_EMAIL].n			= "VM-Email";
	attrs[A_VM_LANGUAGE].n			= "VM-Language";
	vals[V_VM_INFO].n			= "VM-Info";

	/* open log */
	rc_openlog("ser");
	/* read config */
	if ((rh = rc_read_config(radius_config)) == NULL) {
		LOG(L_ERR, "ERROR: acc: error opening radius config file: %s\n", 
			radius_config );
		return -1;
	}
	/* read dictionary */
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary"))!=0) {
		LOG(L_ERR, "ERROR: acc: error reading radius dictionary\n");
		return -1;
	}

	INIT_AV(rh, attrs, vals, "vm", -1, -1);

	if (service_type != -1)
		vals[V_VM_INFO].v = service_type;
#endif

	/* init the line_eol table */
	for(i=0;i<VM_FIFO_PARAMS;i++) {
		lines_eol[2*i].s = 0;
		lines_eol[2*i].len = 0;
		lines_eol[2*i+1] = eol;
	}

	return 0;
}

static int vm_init_child(int rank)
{
	LOG(L_INFO,"voicemail - initializing child %i\n",rank);

#ifdef WITH_DB_SUPPORT
	if (vm_db_url) {
		assert(vm_dbf.init);
		db_handle=vm_dbf.init(vm_db_url);

		if(!db_handle) {
			LOG(L_ERR, "ERROR; vm_init_child: could not init db %s\n", 
						vm_db_url);
			return -1;
		}
	}
#endif
	return 0;
}

#ifdef _OBSO
static int vm_extract_body(struct sip_msg *msg, str *body );
#endif

static int vm_get_user_info( str* user,   /*[in]*/
			     str* host,   /*[in]*/
			     str* email,   /*[out]*/
			     char* language   /*[out]*/)
{
#ifdef  WITH_LDAP_SUPPORT
        email->s = email_buf;
        email->len = MAX_EMAIL_SIZE;
        return ldap_get_ui(user, email);
#endif

#ifdef WITH_DB_SUPPORT
	db_res_t* email_res=0;
	db_key_t keys[2];
	db_val_t vals[2];
	db_key_t cols[1];

	keys[0] = user_column;
	cols[0] = email_column;
	VAL_TYPE(&(vals[0])) = DB_STR;
	VAL_NULL(&(vals[0])) = 0;
	VAL_STR(&(vals[0]))  = *user;
	    
	keys[1] = domain_column;
	VAL_TYPE(&vals[1]) = DB_STR;
	VAL_NULL(&vals[1]) = 0;
	VAL_STR(&vals[1])  = *host;

	vm_dbf.use_table(db_handle, subscriber_table);
	if (vm_dbf.query(db_handle, keys, 0, vals, cols, (use_domain ? 2 : 1),
				1, 0, &email_res))
	{
		LOG(L_ERR,"ERROR: vm: db_query() failed.\n");
		goto error;
	}

	if( email_res && (email_res->n == 1) ){
		email->s = strdup(VAL_STRING(&(email_res->rows[0].values[0])));
		email->len = strlen(email->s);
	}

	if(email_res)
		vm_dbf.free_query(db_handle, email_res);

	return 0;
error:
	return -1;
#endif

#ifdef WITH_RADIUS_SUPPORT
	static char msg[4096];
	VALUE_PAIR *send, *received, *vp;
	UINT4 service;
	char *user_name, *at;

	send = received = 0;

        email->s = email_buf;
        email->len = MAX_EMAIL_SIZE;

	user_name = (char*)pkg_malloc(user->len + host->len + 2);
	if (!user_name) {
		LOG(L_ERR, "vm(): No memory left\n");
		return -1;
	}

	at = user_name;
	memcpy(at, user->s, user->len);
	at += user->len;
	*at = '@';
	at++;
	memcpy(at, host->s, host->len);
	at += host->len;
	*at = '\0';

	if (!rc_avpair_add(rh, &send, attrs[A_USER_NAME].v, user_name, -1, 0)) {
		LOG(L_ERR, "vm(): Error adding User-Name\n");
		rc_avpair_free(send);
		pkg_free(user_name);
	 	return -1;
	}

	service = vals[V_VM_INFO].v;
	if (!rc_avpair_add(rh, &send, attrs[A_SERVICE_TYPE].v, &service, -1, 0)) {
		LOG(L_ERR, "vm(): Error adding service type\n");
		rc_avpair_free(send);
		pkg_free(user_name);
	 	return -1;
	}
	
	if (rc_auth(rh, 0, send, &received, msg) == OK_RC) {
		rc_avpair_free(send);
		pkg_free(user_name);
		if ((vp = rc_avpair_get(received, attrs[A_VM_EMAIL].v, 0))) {
			if (vp->lvalue > MAX_EMAIL_SIZE) {
				LOG(L_ERR, "vm(): email address too large\n");
				rc_avpair_free(received);
				return -1;
			}
			strncpy(email->s, vp->strvalue, vp->lvalue);
			email->len = vp->lvalue;
		} else {
			LOG(L_ERR, "vm(): email address missing\n");
			rc_avpair_free(received);
			return -1;
		}
		if ((vp = rc_avpair_get(received, attrs[A_VM_LANGUAGE].v, 0))) {
			if (vp->lvalue != 2) {
				LOG(L_ERR, "vm(): invalid language code\n");
				rc_avpair_free(received);
				return -1;
			}
			strncpy(language, vp->strvalue, 2);
		}
		rc_avpair_free(received);
		return 0;
	} else {
		rc_avpair_free(send);
		pkg_free(user_name);
		rc_avpair_free(received);
		return -1;
	}
#endif
}


static int vmt_action(struct sip_msg* msg, char* fifo, char* action) 
{
#ifdef UNDER_CONSTRUCTION
	int status;

    status=(*_tmb.t_newtran)(msg);
	if (status<=0) /* retransmission or error; return; */
			return status;
	/* send a provisional reply back */
    status=(*_tmb.t_reply)(msg, 100, "trying media server for you");
	/* contact SEMS */
	status=vm_action(msg, fifo, action);
	if (status<=0) {
    	status=(*_tmb.t_reply)(msg, 500, "media server unavailable");
		return status;
	}
#endif
	/* success */
	return 1;
}


#define copy_route(s,len,rs,rlen) \
   do {\
     if(rlen+len+3 >= ROUTE_BUFFER_MAX){\
       LOG(L_ERR,"vm: buffer overflow while copying new route\n");\
       goto error;\
     }\
     if(len){\
       append_str(s,",",1);len++;\
     }\
     append_str(s,"<",1);len++;\
     append_str(s,rs,rlen);\
     len += rlen; \
     append_str(s,">",1);len++;\
   } while(0);


#define VM_FROM_PARSED     (1<<1)
#define VM_CONTACT_PARSED  (1<<2)
#define VM_RR_PARSED       (1<<3)
#define VM_RRTMP_PARSED    (1<<4)

#define VM_REQUEST   0
#define VM_ONFAILURE 1


static int vm_action_req(struct sip_msg *msg, char* fifo, char *action)
{
	return vm_action( msg, fifo, action, VM_REQUEST);
}


static int vm_action_fld(struct sip_msg *msg, char* fifo, char *action)
{
	return vm_action( msg, fifo, action, VM_ONFAILURE);
}


static int vm_action(struct sip_msg* msg, char* vm_fifo, char* action,int mode)
{
    str             body;
    unsigned int    hash_index;
    unsigned int    label;
    contact_body_t* cb=0;
    name_addr_t     na;
    str             str_uri;
    str             email;
    str             domain;
    contact_t*      c=0;
    char            id_buf[IDBUF_LEN];
    int             int_buflen, l;
    char*           i2s;
    char*           s;
    rr_t*           record_route;
    char            fproxy_lr;
    char            route_buffer[ROUTE_BUFFER_MAX];
    str             route;
    str             next_hop;
    char            hdrs_buf[HDRS_BUFFER_MAX];
    str             hdrs;
    int             msg_flags_size;
    char            cmd_buf[CMD_BUFFER_MAX];
    struct hdr_field* p_hdr;
    param_hooks_t     hooks;
	int             parse_flags;
	int             ret;
	str             tmp_s;

	ret = -1;

	DBG("DEBUG:vm:vm_action: mode is %d\n",mode);

	if(msg->first_line.type != SIP_REQUEST){
		LOG(L_ERR, "ERROR: vm() has been passed something "
			"else as a SIP request\n");
		goto error;
	}

	parse_flags = 0;

	/* parse all -- we will need every header field for a UAS
	 * avoid parsing if in FAILURE_ROUTE - all hdr have already been parsed */
	if ( mode==VM_REQUEST && parse_headers(msg, HDR_EOH, 0)==-1) {
		LOG(L_ERR, "ERROR: vm: parse_headers failed\n");
		goto error;
	}

	/* find index and hash; (the transaction can be safely used due 
	 * to refcounting till script completes) */
	if( (*_tmb.t_get_trans_ident)(msg,&hash_index,&label) == -1 ) {
		LOG(L_ERR,"ERROR: vm: t_get_trans_ident failed\n");
		goto error;
	}

	/* if in FAILURE_MODE, we have to remember if the from will be parsed by us
	 * or was already parsed - if it's parsed by us, free it at the end */
	if (msg->from->parsed==0) {
		if (mode==VM_ONFAILURE )
			parse_flags |= VM_FROM_PARSED;
		if(parse_from_header(msg) == -1){
			LOG(L_ERR,"ERROR: %s : vm: "
				"while parsing <From:> header\n",exports.name);
			goto error;
		}
	}

	/* parse the RURI (doesn't make any malloc) */
	msg->parsed_uri_ok = 0; /* force parsing */
	if (parse_sip_msg_uri(msg)<0) {
		LOG(L_ERR,"ERROR: %s : vm: uri has not been parsed\n",
			exports.name);
		goto error1;
	}

	/* parse contact header */
	str_uri.s = 0;
	str_uri.len = 0;
	if(msg->contact) {
		if (msg->contact->parsed==0) {
			if (mode==VM_ONFAILURE)
				parse_flags |= VM_CONTACT_PARSED;
			if( parse_contact(msg->contact) == -1) {
				LOG(L_ERR,"ERROR: %s : vm: "
					"while parsing <Contact:> header\n",exports.name);
				goto error1;
			}
		}
#ifdef EXTRA_DEBUG
		DBG("DEBUG: vm:msg->contact->parsed ******* contacts: *******\n");
#endif
		cb = (contact_body_t*)msg->contact->parsed;
		if(cb && (c=cb->contacts)) {
			str_uri = c->uri;
			if (find_not_quoted(&str_uri,'<')) {
				parse_nameaddr(&str_uri,&na);
				str_uri = na.uri;
			}
#ifdef EXTRA_DEBUG
			/*print_contacts(c);*/
			for(; c; c=c->next)
				DBG("DEBUG:           %.*s\n",c->uri.len,c->uri.s);
#endif
		}
#ifdef EXTRA_DEBUG
		DBG("DEBUG: vm: **** end of contacts ****\n");
#endif
	}

	/* str_uri is taken from caller's contact or from header
	 * for backwards compatibility with pre-3261 (from is already parsed)*/
	if(!str_uri.len || !str_uri.s)
		str_uri = get_from(msg)->uri;

	/* parse Record-Route headers */
	route.s = s = route_buffer; route.len = 0;
	fproxy_lr = 0;
	next_hop = empty_param;

	p_hdr = msg->record_route;
	if(p_hdr) {
		if (p_hdr->parsed==0) {
			if (mode==VM_ONFAILURE)
				parse_flags |= VM_RR_PARSED;
			if ( parse_rr(p_hdr) ) {
				LOG(L_ERR,"ERROR: vm: while parsing 'Record-Route:' header\n");
				goto error2;
			}
		}
		record_route = (rr_t*)p_hdr->parsed;
	} else {
		record_route = 0;
	}

	if( record_route ) {
		if ( (tmp_s.s=find_not_quoted(&record_route->nameaddr.uri,';'))!=0 &&
		tmp_s.s+1!=record_route->nameaddr.uri.s+
		record_route->nameaddr.uri.len) {
			/* Parse all parameters */
			tmp_s.len = record_route->nameaddr.uri.len - (tmp_s.s-
				record_route->nameaddr.uri.s);
			if (parse_params( &tmp_s, CLASS_URI, &hooks, 
			&record_route->params) < 0) {
				LOG(L_ERR,"vm: Error while parsing record route uri params\n");
				goto error3;
			}
			fproxy_lr = (hooks.uri.lr != 0);
			DBG("record_route->nameaddr.uri: %.*s\n",
				record_route->nameaddr.uri.len,record_route->nameaddr.uri.s);
			if(fproxy_lr){
				DBG("vm: first proxy has loose routing.\n");
				copy_route(s,route.len,record_route->nameaddr.uri.s,
					record_route->nameaddr.uri.len);
			}
		}
		for(p_hdr = p_hdr->next;p_hdr;p_hdr = p_hdr->next) {
			/* filter out non-RR hdr and empty hdrs */
			if( (p_hdr->type!=HDR_RECORDROUTE) || p_hdr->body.len==0)
				continue;

			if(p_hdr->parsed==0) {
				/* if we are in failure route and we have to parse,
				 * remember to free before exiting */
				if (mode==VM_ONFAILURE)
					parse_flags |= VM_RRTMP_PARSED;
				if ( parse_rr(p_hdr) ){
					LOG(L_ERR,"ERROR: %s : vm: "
						"while parsing <Record-route:> header\n",exports.name);
					goto error3;
				}
			}
			for(record_route=p_hdr->parsed; record_route;
			record_route=record_route->next){
				DBG("record_route->nameaddr.uri: %.*s\n",
					record_route->nameaddr.uri.len,
					record_route->nameaddr.uri.s);
				copy_route(s,route.len,record_route->nameaddr.uri.s,
					record_route->nameaddr.uri.len);
			}
			if (parse_flags&VM_RRTMP_PARSED)
				free_rr( ((rr_t**)&p_hdr->parsed) );
		}

		if(!fproxy_lr){
			copy_route(s,route.len,str_uri.s,str_uri.len);
			str_uri = ((rr_t*)msg->record_route->parsed)->nameaddr.uri;
		} else {
			next_hop = ((rr_t*)msg->record_route->parsed)->nameaddr.uri;
		}
	}

	DBG("vm: calculated route: %.*s\n",route.len,route.len ? route.s : "");
	DBG("vm: next r-uri: %.*s\n",str_uri.len,str_uri.len ? str_uri.s : "");


	/* parse body */
	body = empty_param;
	email = empty_param;
	domain = empty_param;
	language[0] = language[1] = '\0';

	if( REQ_LINE(msg).method_value==METHOD_INVITE ) {

		if( (body.s = get_body(msg)) == 0 ){
			LOG(L_ERR, "ERROR: vm: get_body failed\n");
			goto error3;
		}

		/*body.len = strlen(body.s); (by bogdan) */
		body.len = msg->len - (body.s - msg->buf);
#ifdef WITH_DB_SUPPORT
		if(vm_db_url && vm_get_user_info(&msg->parsed_uri.user,
#else
		if(vm_get_user_info(&msg->parsed_uri.user,
#endif
		   &msg->parsed_uri.host, &email, &(language[0])) < 0) {
			LOG(L_ERR, "ERROR: vm: vm_get_user_info failed\n");
			goto error3;
		}
		domain = msg->parsed_uri.host;
	}

	/* additional headers */
	hdrs.s=hdrs_buf; hdrs.len=0;
	s = hdrs_buf;

	if(hdrs.len+12+sizeof(flag_t)+1 >= HDRS_BUFFER_MAX){
		LOG(L_ERR,"vm: buffer overflow while copying optional header\n");
		goto error3;
	}
	append_str(s,"P-MsgFlags: ",12); hdrs.len += 12;
	msg_flags_size = sizeof(flag_t);
	int2reverse_hex(&s, &msg_flags_size, (int)msg->msg_flags);
	hdrs.len += sizeof(flag_t) - msg_flags_size;
	append_str(s,"\n",1); hdrs.len++;

	for(p_hdr = msg->headers;p_hdr;p_hdr = p_hdr->next) {
		if( !(p_hdr->type&HDR_OTHER) )
			continue;

		if(hdrs.len+p_hdr->name.len+p_hdr->body.len+4 >= HDRS_BUFFER_MAX){
			LOG(L_ERR,"vm: buffer overflow while copying optional header\n");
			goto error3;
		}
		append_str(s,p_hdr->name.s,p_hdr->name.len);
		hdrs.len += p_hdr->name.len;
		append_str(s,": ",2); hdrs.len+=2;
		append_str(s,p_hdr->body.s,p_hdr->body.len);
		hdrs.len += p_hdr->body.len;
		if(*(s-1) != '\n'){
			append_str(s,"\n",1);
			hdrs.len++;
		}
	}

	append_str(s,".",1);
	hdrs.len++;

	eol_line(0).s=VM_FIFO_VERSION;
	eol_line(0).len=strlen(VM_FIFO_VERSION);

	eol_line(1).s = s = cmd_buf;
	if(strlen(action)+12 >= CMD_BUFFER_MAX){
		LOG(L_ERR,"vm: buffer overflow while copying command name\n");
		goto error3;
	}
	append_str(s,"sip_request.",12);
	append_str(s,action,strlen(action));
	eol_line(1).len = s-eol_line(1).s;

	eol_line(2)=REQ_LINE(msg).method;     /* method type */
	eol_line(3)=msg->parsed_uri.user;     /* user from r-uri */
	eol_line(4)=email;                    /* email address from db */
	eol_line(5)=domain;                   /* domain */

	eol_line(6)=msg->rcv.bind_address->address_str; /* dst ip */

	eol_line(7)=msg->rcv.dst_port==SIP_PORT ?
			empty_param : msg->rcv.bind_address->port_no_str; /* port */

	/* r_uri ('Contact:' for next requests) */
	eol_line(8)=msg->first_line.u.request.uri;

	/* r_uri for subsequent requests */
	eol_line(9)=str_uri.len?str_uri:empty_param;

	eol_line(10)=get_from(msg)->body;		/* from */
	eol_line(11)=msg->to->body;			/* to */
	eol_line(12)=msg->callid->body;		/* callid */
	eol_line(13)=get_from(msg)->tag_value;	/* from tag */
	eol_line(14)=get_to(msg)->tag_value;	/* to tag */
	eol_line(15)=get_cseq(msg)->number;	/* cseq number */

	i2s=int2str(hash_index, &l);		/* hash:label */
	if (l+1>=IDBUF_LEN) {
		LOG(L_ERR, "ERROR: vm_start: too big hash\n");
		goto error3;
	}
	memcpy(id_buf, i2s, l);
	id_buf[l]=':';int_buflen=l+1;
	i2s=int2str(label, &l);
	if (l+1+int_buflen>=IDBUF_LEN) {
		LOG(L_ERR, "ERROR: vm_start: too big label\n");
		goto error3;
	}
	memcpy(id_buf+int_buflen, i2s, l);int_buflen+=l;
	eol_line(16).s=id_buf;eol_line(16).len=int_buflen;

	eol_line(17) = route.len ? route : empty_param;
	eol_line(18) = next_hop;
	eol_line(19) = hdrs;
	eol_line(20) = body;

	if ( write_to_vm_fifo(vm_fifo, VM_FIFO_PARAMS)==-1 ) {
		LOG(L_ERR, "ERROR: vm_start: write_to_fifo failed\n");
		goto error3;
	}

	/* make sure that if voicemail does not initiate a reply
	 * timely, a SIP timeout will be sent out */
	if( (*_tmb.t_addblind)() == -1 ) {
		LOG(L_ERR, "ERROR: vm_start: add_blind failed\n");
		goto error3;
	}

	/* success */
	ret = 1;

error3:
	if (parse_flags&VM_RR_PARSED)
		free_rr( ((rr_t**)&msg->record_route->parsed) );
error2:
	if (parse_flags&VM_CONTACT_PARSED)
		free_contact( ((contact_body_t**)&msg->contact->parsed) );
error1:
	if (parse_flags&VM_FROM_PARSED) {
	}
error:
	/* 0 would lead to immediate script exit -- -1 returns
	 * with 'false' to script processing */
	return ret;
}




static int init_tmb()
{
    load_tm_f _load_tm;

    if(!(_load_tm=(load_tm_f)find_export("load_tm",NO_SCRIPT, 0)) ){
	LOG(L_ERR,"ERROR: vm_start: could not find export `load_tm'\n");
	return -1;
    }
    if ( ((*_load_tm)(&_tmb)) == -1 ){
	LOG(L_ERR,"ERROR: vm_start: load_tm failed\n");
	return -1;
    }
    return 0;
}



static int write_to_vm_fifo(char *fifo, int cnt )
{
	int   fd_fifo;

	/* open FIFO file stream */
	if((fd_fifo = open(fifo,O_WRONLY | O_NONBLOCK)) == -1){
		switch(errno){
			case ENXIO:
				LOG(L_ERR,"ERROR: %s: ans_machine deamon is not running !\n",
								exports.name);
			default:
				LOG(L_ERR,"ERROR: %s: %s\n",exports.name,strerror(errno));
		}
		goto error;
	}

	/* write now (unbuffered straight-down write) */
repeat:
	if (writev(fd_fifo, (struct iovec*)lines_eol, 2*cnt)<0) {
		if (errno!=EINTR) {
			LOG(L_ERR, "ERROR: write_to_vm_fifo: writev failed: %s\n",
				strerror(errno));
			close(fd_fifo);
			goto error;
		} else {
			goto repeat;
		}
	}
	close(fd_fifo);

	DBG("DEBUG: write_to_vm_fifo: write completed\n");
	return 1; /* OK */

error:
	return -1;
}
