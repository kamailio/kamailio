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
 *            support introduced; sprintf removed;
 *
 */

#include "../../fifo_server.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../config.h"
#include "../tm/tm_load.h"
#include "../../parser/parse_from.h"
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

#define VM_FIFO_PARAMS 14

#define VM_INVITE      "invite"
#define VM_BYE         "bye"

static str empty_str={0,0};

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

        if (register_fifo_cmd(fifo_uac_dlg, "vm_uac_dlg", 0)<0) { 
  		LOG(L_CRIT, "cannot register fifo vm_uac_dlg\n"); 
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

    if( !db_init && bind_dbmod() ){
		LOG(L_CRIT, "cannot bind db_mod\n");
		return -1;
    }

    assert(db_init);

    db_handle=db_init(vm_db_url);

    if(!db_handle) {
		LOG(L_ERR, "ERROR; vm_init_child: could not init db %s\n", 
						vm_db_url);
		return -1;
    }

    return 0;
}

static int vm_extract_body(struct sip_msg *msg, str *body );

static int vm_action(struct sip_msg* msg, char* vm_fifo, char* action)
{
        str             body;
        unsigned int    hash_index;
        unsigned int    label;
        contact_body_t* cb=0;
        str*            str_uri=0;
        str             email_query;
        str             email;
        db_res_t*       email_res=0;
	contact_t*      c=0;
	str             lines[VM_FIFO_PARAMS];
	char            id_buf[IDBUF_LEN];
	int             int_buflen, l;
	char*           i2s;
	char*           s;

	/* parse all -- we will need every header field for a UAS */
	if (parse_headers(msg, HDR_EOH, 0)==-1) {
		LOG(L_ERR, "ERROR; vm_start: parse_headers failed\n");
		goto error;
	}

	/* find index and hash; (the transaction can be safely used due 
	 * to refcounting till script completes)
	 */
        if( (*_tmb.t_get_trans_ident)(msg,&hash_index,&label) == -1 ) {
		LOG(L_ERR,"ERROR: vm_start: t_get_trans_ident failed\n");
		goto error;
        }

        if(parse_from_header(msg) == -1){
		LOG(L_ERR,"ERROR: %s : vm_start: "
				"while parsing <From:> header\n",exports.name);
		goto error;
        }

	if (parse_sip_msg_uri(msg)<0) {
  		LOG(L_ERR,"ERROR: %s : vm_start: uri has not been parsed\n",
				exports.name);
  		goto error;
        }

        if(msg->contact){

		if(parse_contact(msg->contact) == -1){
	    	LOG(L_ERR,"ERROR: %s : vm_start: "
						"while parsing <Contact:> header\n",exports.name);
	    	goto error;
		}
	
#ifdef EXTRA_DEBUG
		DBG("DEBUG: vm_start: ******* contacts: *******\n");
#endif
		cb = msg->contact->parsed;

		if(cb && (c=cb->contacts)) {
		    str_uri = &c->uri;
#ifdef EXTRA_DEBUG
		    print_contacts(c);
		    for(; c; c=c->next)
			DBG("DEBUG:           %.*s\n",c->uri.len,c->uri.s);
#endif
		}
#ifdef EXTRA_DEBUG
		DBG("DEBUG: vm_start: **** end of contacts ****\n");
#endif
        }

	/* str_uri is taken from caller's contact or from is missing
	 * for backwards compatibility with pre-3261 */
        if(!str_uri || !str_uri->len)
	    str_uri = &(get_from(msg)->uri);
	
	if(msg->route)
	    DBG("DEBUG: vm_start: route:%.*s\n",
		msg->route->body.len,msg->route->body.s);


	email.s = 0; email.len = 0;
	body.s = 0; body.len = 0;

	if(!strcmp(action,VM_INVITE)){

	    if(vm_extract_body(msg,&body)==-1) {
		LOG(L_ERR, "ERROR: vm_start: extract_body failed\n");
		goto error;
	    }

	    email_query.len = SQL_SELECT_LEN
		+ strlen(email_column)
		+ SQL_FROM_LEN
		+ strlen(subscriber_table)
		+ SQL_WHERE_LEN
		+ strlen(user_column)
		+ SQL_EQUAL_LEN
		+ msg->parsed_uri.user.len + 2/* strlen("''") */
#ifdef MULTI_DOMAIN
		+ SQL_AND_LEN
		+ strlen(domain_column)
		+ SQL_EQUAL_LEN
		+ msg->parsed_uri.host.len + 2/* strlen("''") */
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
	    append_str(s,msg->parsed_uri.user.s,msg->parsed_uri.user.len);
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
	    
	    if( (!email_res) || (email_res->n != 1) ){
	    
		if(email_res)
		    (*db_free_query)(db_handle,email_res);
		
		LOG( L_ERR,"ERROR: %s: no email for user '%.*s'",
		     exports.name,
		     msg->parsed_uri.user.len,msg->parsed_uri.user.s);
		goto error;
	    }
	    
	    email.s = strdup(VAL_STRING(&(email_res->rows[0].values[0])));
	    email.len = strlen(email.s);
	}

	lines[0].s=action; lines[0].len=strlen(action); 
	lines[1]=msg->parsed_uri.user;		/* user from r-uri */
	lines[2]=email;			        /* email address from db */
	lines[3].s=ip_addr2a(&msg->rcv.dst_ip);	/* dst ip */
	lines[3].len=strlen(lines[3].s);
	lines[4]=*str_uri;			/* contact (from if c absent) */
	lines[5]=get_from(msg)->body;		/* from */
	lines[6]=msg->to->body;			/* to */
	lines[7]=msg->callid->body;		/* callid */
	lines[8]=get_from(msg)->tag_value;	/* from tag */
	lines[9]=get_to(msg)->tag_value;	/* to tag */
	lines[10]=get_cseq(msg)->number;	/* cseq number */

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
	lines[11].s=id_buf;lines[11].len=int_buflen;

	lines[12]=msg->route ? msg->route->body : empty_str;
	lines[13].s=body.s; lines[13].len=body.len;

	if ( write_to_vm_fifo(vm_fifo, &lines[0], 
			     body.len ? VM_FIFO_PARAMS : VM_FIFO_PARAMS-1)
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

#ifdef _OBSO
static int vm_stop(struct sip_msg* msg, char* vm_fifo, char* str2)
{

    int  is_local;
	str lines[VM_FIFO_PARAMS];

	/* parse all -- we will need every header field for a UAS */
	if (parse_headers(msg, HDR_EOH, 0)==-1) {
		LOG(L_ERR, "ERROR; vm_stop: parse_headers failed\n");
		goto error;
	}

	/* REVIEW/jiri: what is this good for? incoming requests  should
	 * never match transactions which were created localy, should
	 * they?  --- just look it up and ignore "is_local" */

	if( (is_local = (*_tmb.t_is_local)(msg)) == -1 ) { 
	        LOG(L_ERR,"ERROR: vm_start: t_is_local failed\n"); 
	        goto error; 
	} 
    
	if(parse_from_header(msg) == -1){
		LOG(L_ERR,"ERROR: %s : vm_stop: while parsing <From> header\n",
						exports.name);
		goto error;
	}

	lines[0].s="bye";lines[0].len=3;
	lines[1]=msg->parsed_uri.user;			/* user from r-uri */
	lines[2]=empty_str;						/* email */
	lines[3].s=ip_addr2a(&msg->rcv.dst_ip);	/* dst ip */
	lines[3].len=strlen(lines[3].s);
	lines[4]=get_from(msg)->uri;			/* from uri */
	lines[5]=get_from(msg)->body;			/* from */
	lines[6]=msg->to->body;					/* to */
	lines[7]=msg->callid->body;				/* callid */
	lines[8]=get_from(msg)->tag_value;		/* from-tag (optional) */
	lines[9]=get_to(msg)->tag_value;		/* to-tag (optional) */
	lines[10]=get_cseq(msg)->number;		/* cseq number */
	lines[11]=empty_str;					/* no hash:index */
	lines[12]=msg->route ? msg->route->body : empty_str;	/* route */

    if (write_to_vm_fifo(vm_fifo, &lines[0], 13 )==-1) {
		LOG(L_ERR, "ERROR: vm_stop: write_to_fifo failed\n");
		goto error;
	}

	return 1;

 error:
    return -1; /* !OK */

}
#endif

static int im_get_body_len( struct sip_msg* msg)
{
	int x,err;
	str foo;

	if (!msg->content_length)
	{
		LOG(L_ERR,"ERROR: im_get_body_len: Content-Length header absent!\n");
		goto error;
	}
	/* if header is present, trim to get only the string containing numbers */
	trim_len( foo.len , foo.s , msg->content_length->body );
	/* convert from string to number */
	x = str2s( foo.s,foo.len,&err);
	if (err){
		LOG(L_ERR, "ERROR: im_get_body_len:"
			" unable to parse the Content_Length number !\n");
		goto error;
	}
	return x;
error:
	return -1;
}

static int vm_extract_body(struct sip_msg *msg, str *body )
{
	int len;
	int offset;

	if ( parse_headers(msg,HDR_EOH, 0)==-1 )
	{
		LOG(L_ERR,"ERROR: vm_extract_body: unable to parse all headers!\n");
		goto error;
	}

	/* get the lenght from Content-Lenght header */
	if ( (len = im_get_body_len(msg))<0 )
	{
		LOG(L_ERR,"ERROR: vm_extract_body: cannot get body length\n");
		goto error;
	}

	if ( strncmp(CRLF,msg->unparsed,CRLF_LEN)==0 )
		offset = CRLF_LEN;
	else if (*(msg->unparsed)=='\n' || *(msg->unparsed)=='\r' )
		offset = 1;
	else{
		LOG(L_ERR,"ERROR: vm_extract_body: unable to detect the beginning"
			" of message body!\n ");
		goto error;
	}

	body->s = msg->unparsed + offset;
	body->len = len;

#ifdef _VM_EXTRA_DBG
	DBG("DEBUG:vm_extract_body:=|%.*s|\n",body->len,body->s);
#endif

	return 1;
error:
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
