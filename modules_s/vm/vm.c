/*
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
 *            support introduced; snprintf removed;
 *
 */

#include "../../fifo_server.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../config.h"
#include "../tm/tm_load.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../parser/contact/parse_contact.h"
#include "../../db/db.h"

#include "vm_fifo.h"
#include "defs.h"
#include "vm.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

#define append_str(_dest,_src,_len) \
	do{\
		memcpy( (_dest) , (_src) , (_len) );\
		(_dest) += (_len) ;\
	}while(0);

#define IDBUF_LEN	128

#define SQL_SELECT     "SELECT "
#define SQL_SELECT_LEN 7

#define SQL_FROM       " FROM "
#define SQL_FROM_LEN   6

#define SQL_WHERE      " WHERE "
#define SQL_WHERE_LEN  7

#define SQL_AND        " AND "
#define SQL_AND_LEN    5

#define SQL_EQUAL      " = "
#define SQL_EQUAL_LEN  3

#define VM_FIFO_PARAMS 17

#define VM_INVITE      "invite"
#define VM_BYE         "bye"

#define ROUTE_BUFFER_MAX 512

static str empty_param={".",1};

static int write_to_vm_fifo(char *fifo, str *lines, int cnt );
static int init_tmb();
static int vm_action(struct sip_msg*, char* fifo, char*);
static int vm_mod_init(void);
static int vm_init_child(int rank);

struct tm_binds _tmb;

char* vm_db_url = "sql://ser:heslo@localhost/ser";    /* Database URL */
char* email_column = "email_address";
char* subscriber_table = "subscriber" ;

char* user_column = "username";

#ifdef MULTI_DOMAIN
char* domain_column = "domain";
#endif

#define EXTRA_DEBUG

db_con_t* db_handle = 0;

#define get_from(p_msg)      ((struct to_body*)(p_msg)->from->parsed)


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"vm", vm_action, 2, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",           STR_PARAM, &vm_db_url       },
	{"email_column",     STR_PARAM, &email_column    },
	{"subscriber_table", STR_PARAM, &subscriber_table},
	{"user_column",      STR_PARAM, &user_column     },
#ifdef MULTI_DOMAIN
	{"domain_column",    STR_PARAM, &domain_column   },
#endif
	{0, 0, 0}
};


struct module_exports exports = {
    "voicemail", 
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
        fprintf(stderr, "voicemail - initializing\n");

        if (register_fifo_cmd(fifo_vm_reply, "vm_reply", 0)<0) { 
  		LOG(L_CRIT, "cannot register fifo vm_reply\n"); 
  		return -1; 
        } 

	if (init_tmb()==-1) {
		LOG(L_ERR, "Error: vm_mod_init: cann't load tm\n");
		return -1;
	}

	if (bind_dbmod()) {
		LOG(L_ERR, "ERROR: vm_mod_init: unable to bind db\n");
		return -1;
	}
    
        return 0;
}

static int vm_init_child(int rank)
{
    LOG(L_INFO,"voicemail - initializing child %i\n",rank);

    assert(db_init);

    db_handle=db_init(vm_db_url);

    if(!db_handle) {
		LOG(L_ERR, "ERROR; vm_init_child: could not init db %s\n", 
						vm_db_url);
		return -1;
    }

    return 0;
}

#ifdef _OBSO
static int vm_extract_body(struct sip_msg *msg, str *body );
#endif

static int vm_get_user_info( str* user,   /*[in]*/
			     str* host,   /*[in]*/
                             str* email   /*[out]*/)
{
        db_res_t*       email_res=0;
    
#ifdef _OBSO
	char*           s=0;
        str             email_query;

	    email_query.len = SQL_SELECT_LEN
		+ strlen(email_column)
		+ SQL_FROM_LEN
		+ strlen(subscriber_table)
		+ SQL_WHERE_LEN
		+ strlen(user_column)
		+ SQL_EQUAL_LEN
		+ user->len + 2/* strlen("''") */
#ifdef MULTI_DOMAIN
		+ SQL_AND_LEN
		+ strlen(domain_column)
		+ SQL_EQUAL_LEN
		+ host->len + 2/* strlen("''") */
#endif
		;
	    
	    email_query.s = malloc(email_query.len+1);
	    if(!email_query.s){
		LOG(L_ERR,"ERROR: %s: not enough memory\n",
		    exports.name);
		goto error;
	    }
	    s = email_query.s;
	    append_str(s,SQL_SELECT,SQL_SELECT_LEN);
	    append_str(s,email_column,strlen(email_column));
	    append_str(s,SQL_FROM,SQL_FROM_LEN);
	    append_str(s,subscriber_table,strlen(subscriber_table));
	    append_str(s,SQL_WHERE,SQL_WHERE_LEN);
	    append_str(s,user_column,strlen(user_column));
	    append_str(s,SQL_EQUAL,SQL_EQUAL_LEN);
	    *s = '\''; s++;
	    append_str(s,user->s,user->len);
	    *s = '\''; s++;
#ifdef MULTI_DOMAIN
	    append_str(s,SQL_AND,SQL_AND_LEN);
	    append_str(s,domain_column,strlen(domain_column));
	    append_str(s,SQL_EQUAL,SQL_EQUAL_LEN);
	    *s = '\''; s++;
	    append_str(s,msg->parsed_uri.host.s,msg->parsed_uri.host.len);
	    *s = '\''; s++;
#endif
	    *s = '\0';
	    
	    
	    (*db_raw_query)(db_handle,email_query.s,&email_res);
	    free(email_query.s);
#else
	    db_key_t keys[2];
	    db_val_t vals[2];
	    db_key_t cols[1];

	    keys[0] = user_column;
	    cols[0] = email_column;
	    VAL_TYPE(&(vals[0])) = DB_STR;
	    VAL_NULL(&(vals[0])) = 0;
	    VAL_STR(&(vals[0]))  = *user;

#ifdef MULTI_DOMAIN
	    keys[1] = domain_column;
	    VAL_TYPE(&vals[1]) = DB_STR;
	    VAL_NULL(&vals[1]) = 0;
	    VAL_STR(&vals[1])  = *host;
#endif

	    db_use_table(db_handle,subscriber_table);
	    if ((*db_query)(db_handle, keys, 0, vals, cols, 
#ifdef MULTI_DOMAIN
			    2, 
#else
			    1,
#endif
			    1, 0, &email_res))
	    {

		LOG(L_ERR,"ERROR: vm: db_query() failed.");
		goto error;
	    }
#endif
	    
	    if( (!email_res) || (email_res->n != 1) ){
	    
		if(email_res)
		    (*db_free_query)(db_handle,email_res);
		
		LOG( L_ERR,"ERROR: %s: no email for user '%.*s'",
		     exports.name,
		     user->len,user->s);
		goto error;
	    }
	    
	    email->s = strdup(VAL_STRING(&(email_res->rows[0].values[0])));
	    email->len = strlen(email->s);

#ifdef MULTI_DOMAIN
	    domain->s = strdup(VAL_STRING(&(email_res->rows[0].values[1])));
	    domain->s = strlen(domain->s);
#endif	    

	    return 0;
error:
	    return -1;
}

static int vm_action(struct sip_msg* msg, char* vm_fifo, char* action)
{
        str             body;
        unsigned int    hash_index;
        unsigned int    label;
        contact_body_t* cb=0;
        str             str_uri;
        str             email;
	str             domain;
	contact_t*      c=0;
	str             lines[VM_FIFO_PARAMS];
	char            id_buf[IDBUF_LEN];
	int             int_buflen, l;
	char*           i2s;
	char*           s;
	rr_t*           record_route;
	char            fproxy_lr;
	char            route_buffer[ROUTE_BUFFER_MAX];
	str             route;
	struct hdr_field* rr_hdr;
	param_hooks_t     hooks;
	str               tmp_str;

	if(msg->first_line.type != SIP_REQUEST){
	    LOG(L_ERR, "ERROR: vm() has been passed something else as a SIP request\n");
	    goto error;
	}

	/* parse all -- we will need every header field for a UAS */
	if (parse_headers(msg, HDR_EOH, 0)==-1) {
		LOG(L_ERR, "ERROR: vm: parse_headers failed\n");
		goto error;
	}

	/* find index and hash; (the transaction can be safely used due 
	 * to refcounting till script completes)
	 */
        if( (*_tmb.t_get_trans_ident)(msg,&hash_index,&label) == -1 ) {
		LOG(L_ERR,"ERROR: vm: t_get_trans_ident failed\n");
		goto error;
        }

        if(parse_from_header(msg) == -1){
		LOG(L_ERR,"ERROR: %s : vm: "
				"while parsing <From:> header\n",exports.name);
		goto error;
        }

	if (parse_sip_msg_uri(msg)<0) {
  		LOG(L_ERR,"ERROR: %s : vm: uri has not been parsed\n",
				exports.name);
  		goto error;
        }

	str_uri.s = 0; str_uri.len = 0;
        if(msg->contact){

		if(parse_contact(msg->contact) == -1){
		    LOG(L_ERR,"ERROR: %s : vm: "
			"while parsing <Contact:> header\n",exports.name);
		    goto error;
		}
	
#ifdef EXTRA_DEBUG
		DBG("DEBUG: vm: ******* contacts: *******\n");
#endif
		cb = msg->contact->parsed;

		if(cb && (c=cb->contacts)) {
		    str_uri = c->uri;
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

	/* str_uri is taken from caller's contact or from is missing
	 * for backwards compatibility with pre-3261 */
        if(!str_uri.len)
	    str_uri = get_from(msg)->uri;

	//if(parse_nameaddr(str* _s, name_addr_t* _a)){
	//    LOG(L_ERR,"ERROR: parse_nameaddr failed\n");
	//}

	route.s = route_buffer; route.len = 0;
	s = route_buffer;

	rr_hdr = msg->record_route;
	if(rr_hdr && (!rr_hdr->parsed && parse_rr(rr_hdr))){
	    LOG(L_ERR,"ERROR: vm: while parsing 'Record-Route:' header\n");
	    goto error;
	}
	    
	record_route = rr_hdr ? rr_hdr->parsed : 0;
	fproxy_lr = 0;

	if(rr_hdr && record_route){

#define copy_route(s,len,rs,rlen) \
   do {\
     if(rlen+len+3 >= ROUTE_BUFFER_MAX){\
       LOG(L_ERR,"vm: buffer overflow while copying new route");\
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

	  if(rr_hdr->body.len){
	      
	    /* Parse all parameters */
	    tmp_str = record_route->nameaddr.uri;
	    if (parse_params(&tmp_str, CLASS_RR, &hooks, &record_route->params) < 0) {
	      LOG(L_ERR, "vm: Error while parsing record route uri params\n");
	      goto error;
	    }
	    
	    //for(route_param=record_route->params; route_param; route_param=route_param->next)
	    fproxy_lr = (hooks.rr.lr != 0);//|= (route_param->type == P_LR);
	    
	    DBG("record_route->nameaddr.uri: %.*s\n",record_route->nameaddr.uri.len,record_route->nameaddr.uri.s);
	    if(fproxy_lr){
	      copy_route(s,route.len,record_route->nameaddr.uri.s,record_route->nameaddr.uri.len);
	    }
	  }
	  rr_hdr = rr_hdr->next;

	  for(;;rr_hdr = rr_hdr->next){

	    if(rr_hdr && (rr_hdr->type == HDR_RECORDROUTE) && rr_hdr->body.len){
	      
	      if(!rr_hdr->parsed && parse_rr(rr_hdr)){
		LOG(L_ERR,"ERROR: %s : vm: "
		  "while parsing <Record-route:> header\n",exports.name);
		goto error;
	      }
	      
	      for(record_route = rr_hdr->parsed; record_route; record_route = record_route->next){
		DBG("record_route->nameaddr.uri: %.*s\n",record_route->nameaddr.uri.len,record_route->nameaddr.uri.s);
		copy_route(s,route.len,record_route->nameaddr.uri.s,record_route->nameaddr.uri.len);
	      }
	    } 

	    if(rr_hdr == msg->last_header)
	      break;
	  }

	  if(!fproxy_lr){
	    copy_route(s,route.len,str_uri.s,str_uri.len);
	    str_uri = ((rr_t*)msg->record_route->parsed)->nameaddr.uri;
	  }
	}

	DBG("vm: calculated route: %.*s\n",route.len,route.s);
	DBG("vm: next r-uri: %.*s\n",str_uri.len,str_uri.s);
	
	body = empty_param;
	email = empty_param;
	domain = empty_param;

	if(!strcmp(action,VM_INVITE)){

	    if( (body.s = get_body(msg)) == 0 ){
		LOG(L_ERR, "ERROR: vm: get_body failed\n");
		goto error;
	    }

	    body.len = strlen(body.s);
	    
	    if(vm_get_user_info(&msg->parsed_uri.user,&msg->parsed_uri.host,&email) < 0){
		LOG(L_ERR, "ERROR: vm: vm_get_user_info failed\n");
		goto error;
	    }
	    domain = msg->parsed_uri.host;
	}

	lines[0].s=action; lines[0].len=strlen(action);

	lines[1]=msg->parsed_uri.user;		/* user from r-uri */
	lines[2]=email;			        /* email address from db */
	lines[3]=domain;                        /* domain */
	/*  lines[4].s=ip_addr2a(&msg->rcv.dst_ip); */
	/*  lines[4].len=strlen(lines[4].s); */
	lines[4]=msg->rcv.bind_address->address_str; /* dst ip */
	lines[5]=msg->rcv.bind_address->port_no_str; /* port */
	lines[6]=msg->first_line.u.request.uri;      /* r_uri ('Contact:' for next requests) */
	/*  lines[6]=empty_param; */
	lines[7]=str_uri.len?str_uri:empty_param; /* r_uri for subsequent requests */
	lines[8]=get_from(msg)->body;		/* from */
	lines[9]=msg->to->body;			/* to */
	lines[10]=msg->callid->body;		/* callid */
	lines[11]=get_from(msg)->tag_value;	/* from tag */
	lines[12]=get_to(msg)->tag_value;	/* to tag */
	lines[13]=get_cseq(msg)->number;	/* cseq number */

	i2s=int2str(hash_index, &l);		/* hash:label */
	if (l+1>=IDBUF_LEN) {
		LOG(L_ERR, "ERROR: vm_start: too big hash\n");
		goto error;
	}
	memcpy(id_buf, i2s, l);id_buf[l]=':';int_buflen=l+1;
	i2s=int2str(label, &l);
	if (l+1+int_buflen>=IDBUF_LEN) {
		LOG(L_ERR, "ERROR: vm_start: too big label\n");
		goto error;
	}
	memcpy(id_buf+int_buflen, i2s, l);int_buflen+=l;
	lines[14].s=id_buf;lines[14].len=int_buflen;

	lines[15]=route.len ? route : empty_param;
	lines[16].s=body.s; lines[16].len=body.len;

	if ( write_to_vm_fifo(vm_fifo, &lines[0],VM_FIFO_PARAMS)
	     ==-1 ) {

	    LOG(L_ERR, "ERROR: vm_start: write_to_fifo failed\n");
	    goto error;
	}

	/* make sure that if voicemail does not initiate a reply
	 * timely, a SIP timeout will be sent out */
	if( (*_tmb.t_addblind)() == -1 ) {
		LOG(L_ERR, "ERROR: vm_start: add_blind failed\n");
		goto error;
	}
	return 1;

 error:
	/* 0 would lead to immediate script exit -- -1 returns
		with 'false' to script processing */
	return -1;
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

static int write_to_vm_fifo(char *fifo, str *lines, int cnt )
{
    int   fd_fifo;
	char *buf, *p;
	int len;
	int i;


	/* contruct buffer first */
	len=0;
	for (i=0; i<cnt; i++) len+=lines[i].len+1;
	buf=pkg_malloc(len+1);
	if (!buf) {
		LOG(L_ERR, "ERROR: write_to_vm_fifo: no mem\n");
		return -1;
	}
	p=buf;
	for (i=0; i<cnt; i++ ) {
		memcpy(p, lines[i].s, lines[i].len);
		p+=lines[i].len;
		*p='\n';
		p++;
	}
		

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
    DBG("vm: write_to_vm_fifo: <%.*s>\n",len,buf);
    if (write(fd_fifo, buf,len)==-1) {
		LOG(L_ERR, "ERROR: write_to_vm_fifo: write failed: %s\n",
					strerror(errno));
	}
    close(fd_fifo);

    DBG("DEBUG: write_to_vm_fifo: write completed\n");

	pkg_free(buf);
    return 1; /* OK */

error:
	pkg_free(buf);
	return -1;
}
