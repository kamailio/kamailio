/*
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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

static int write_to_vm_fifo(void* buf, unsigned int len);
static int init_tmb();
static int vm_start(struct sip_msg*, char*, char*);
static int vm_stop(struct sip_msg*, char*, char*);
static int vm_mod_init(void);
#if 1
static int vm_init_child(int rank);
#endif

static load_tm_f _load_tm=0;
static struct tm_binds _tmb;

char* vm_db_url = "sql://ser:heslo@localhost/ser";    /* Database URL */
db_con_t* db_handle = 0;

#define get_from(p_msg)      ((struct to_body*)(p_msg)->from->parsed)

struct module_exports exports = {
    "voicemail", 
    (char*[]){"vm_start","vm_stop"},
    (cmd_function[]){vm_start,vm_stop},
    (int[]){0,0},
    (fixup_function[]){0,0},
    2, /* number of functions*/
    
    (char*[]) {
	"db_url"
    },
    (modparam_t[]) {
	STR_PARAM
    },
    (void*[]) {
	&vm_db_url
    },
    1,
    
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

    if( !(db_handle = (*db_init)(vm_db_url)) ){
	LOG(L_CRIT, "could not init db\n");
	return -1;
    }

    return (*db_use_table)(db_handle,"subscriber");
}

static int vm_extract_body(struct sip_msg *msg, str *body );

static int vm_start(struct sip_msg* msg, char* str1, char* str2)
{
    str    body;
    char  cmd[2048];
    unsigned int hash_index;
    unsigned int label;
    contact_body_t* cb;
    str*  str_uri=0;
    char query_buf[256];
    char email_buf[256];
    db_res_t* email_res=0;
	contact_t* c;

    db_handle = db_init(vm_db_url);

    if(init_tmb())
	goto error;

    if( (*_tmb.t_get_trans_ident)(msg,&hash_index,&label) == -1 ) {
	LOG(L_ERR,"ERROR: vm_start: t_get_trans_ident failed\n");
	goto error;
    }

    if(vm_extract_body(msg,&body)==-1)
	goto error;

    if(parse_from_header(msg) == -1){
	LOG(L_ERR,"ERROR: %s : vm_start: while parsing <From:> header\n",exports.name);
	goto error;
    }

    if(!msg->parsed_uri_ok){
  	LOG(L_ERR,"ERROR: %s : vm_start: uri has not been parsed\n",exports.name);
  	goto error;
    }

    if(msg->contact){

	if(parse_contact(msg->contact) == -1){
	    LOG(L_ERR,"ERROR: %s : vm_start: while parsing <Contact:> header\n",exports.name);
	    goto error;
	}
	
	DBG("DEBUG: vm_start: ******* contacts: *******\n");
	cb = msg->contact->parsed;

	if(cb) {
	    print_contacts(cb->contacts);
	    c=cb->contacts;
	    str_uri = &c->uri;
	    
	    for(; c; c=c->next)
		DBG("DEBUG:           %.*s\n",c->uri.len,c->uri.s);
	}
	DBG("DEBUG: vm_start: **** end of contacts ****\n");
    }

    if(!str_uri || !str_uri->len)
	str_uri = &(get_from(msg)->uri);
	
    if(msg->route)
	DBG("DEBUG: vm_start: route:%.*s\n",msg->route->body.len,msg->route->body.s);

    if( snprintf( query_buf,256,
  		  "SELECT email_address FROM subscriber WHERE user = '%.*s'",
  		  msg->parsed_uri.user.len,msg->parsed_uri.user.s ) < 0 )
    {
  	LOG(L_ERR,"ERROR: %s: snprintf failed\n",exports.name);
  	return -1;
    } 

    (*db_raw_query)(db_handle,query_buf,&email_res);
    if( (!email_res) || (email_res->n != 1) ){
  	LOG( L_ERR,"ERROR: %s: no email for user '%.*s'",
  	     exports.name,
  	     msg->parsed_uri.user.len,msg->parsed_uri.user.s);
  	return -1;
    }

    strcpy(email_buf,VAL_STRING(&(email_res->rows[0].values[0])));
    (*db_free_query)(db_handle,email_res);

    if ( snprintf(cmd,2048,
		  "invite\n"
		  "%.*s\n"  // user
		  "%s\n"    // email
		  "%s\n"    // dst ip
		  "%.*s\n"  // from uri
		  "%.*s\n"  // from
		  "%.*s\n"  // to
		  "%.*s\n"  // call-id
		  "%.*s\n"  // from-tag [optional]
		  "%.*s\n"  // to-tag [optional]
		  "%.*s\n"  // cseq
		  "%u:%u\n" // hash_index label [optional]
		  "%.*s\n"  // route [optional]
		  "%.*s\n", // body [optional]
		  msg->parsed_uri.user.len,msg->parsed_uri.user.s,
		  email_buf,
		  ip_addr2a(&msg->rcv.dst_ip),
		  str_uri->len,str_uri->s, // from-uri   
		  get_from(msg)->body.len,get_from(msg)->body.s, // from
		  msg->to->body.len,msg->to->body.s,
		  msg->callid->body.len,msg->callid->body.s,
		  get_from(msg)->tag_value.len,get_from(msg)->tag_value.s,
		  get_to(msg)->tag_value.len,get_to(msg)->tag_value.s,
		  get_cseq(msg)->number.len,get_cseq(msg)->number.s,
		  hash_index,label,
		  msg->route ? msg->route->body.len : 0, msg->route ? msg->route->body.s : "",// route
		  body.len,body.s ) < 0 ) 
    {
	LOG(L_ERR,"ERROR: %s: snprintf failed\n",exports.name);
	goto error;
    }
#ifdef _VM_EXTRA_DBG
    DBG("DEBUG: vm_start: sending `%s'\n", cmd);
#endif
    return write_to_vm_fifo(cmd,strlen(cmd));

 error:
    return 0; // !OK
}

static int vm_stop(struct sip_msg* msg, char* str1, char* str2)
{
    char cmd[1024];
    char srcip[64];
    char dstip[64];
    int  is_local;
    int  err;
    
    if(init_tmb())
	goto error;

    if( (is_local = (*_tmb.t_is_local)(msg)) == -1 ) {
	LOG(L_ERR,"ERROR: vm_start: t_is_local failed\n");
	goto error;
    }
    
    if(is_local)
	return (*_tmb.t_relay)(msg, (char*)0, (char*)0);

    if(parse_from_header(msg) == -1){
	LOG(L_ERR,"ERROR: %s : vm_stop: while parsing <From> header\n",exports.name);
	goto error;
    }

    strcpy(srcip,ip_addr2a(&msg->rcv.src_ip));
    strcpy(dstip,ip_addr2a(&msg->rcv.dst_ip));

    if ( snprintf( cmd,1024,
		   "bye\n"
		   "%.*s\n"  /* user */
		   "\n"      /* email */
		   "%s\n"    /* dst_ip */
		   "%.*s\n"  /* from uri*/
		   "%.*s\n"  /* from */
		   "%.*s\n"  /* to */
		   "%.*s\n"  /* call-id */
		   "%.*s\n"  /* from-tag [optional] */
		   "%.*s\n"  /* to-tag [optional]*/
		   "%.*s\n"  /* cseq */
		   "\n"      /* hash_index:label [optinal] */
		   "\n",     /* route [optional]*/
		   msg->parsed_uri.user.len,msg->parsed_uri.user.s,
		   ip_addr2a(&msg->rcv.dst_ip),
		   get_from(msg)->uri.len,get_from(msg)->uri.s,
		   msg->from->body.len,msg->from->body.s,
		   msg->to->body.len,msg->to->body.s,
		   msg->callid->body.len,msg->callid->body.s,
		   get_from(msg)->tag_value.len,get_from(msg)->tag_value.s,
		   get_to(msg)->tag_value.len,get_to(msg)->tag_value.s,
		   get_cseq(msg)->number.len,get_cseq(msg)->number.s ) < 0 ) 
    {
	LOG(L_ERR,"ERROR: %s : snprintf failed\n",exports.name);
	goto error;
    }

    
    err = write_to_vm_fifo(cmd,strlen(cmd));
    if(err!=1)
	return err;
    
    return (*_tmb.t_reply)(msg,200,"OK");

 error:
    return 0; // !OK
}


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
	x = str2s( (unsigned char*)foo.s,foo.len,&err);
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
    if( !_load_tm ) {
	if(!(_load_tm=(load_tm_f)find_export("load_tm",NO_SCRIPT)) ){
	    LOG(L_ERR,"ERROR: vm_start: could not find export `load_tm'\n");
	    return -1;
	}
	if ( ((*_load_tm)(&_tmb)) == -1 ){
	    LOG(L_ERR,"ERROR: vm_start: load_tm failed\n");
	    return -1;
	}
    }
    return 0;
}

static int write_to_vm_fifo(void* buf, unsigned int len)
{
    int   fd_fifo;
    FILE* fp_fifo;
    
    if((fd_fifo = open("/tmp/am_fifo",O_WRONLY | O_NONBLOCK)) == -1){
	switch(errno){
	    case ENXIO:
		LOG(L_ERR,"ERROR: %s: ans_machine deamon is not running !\n",exports.name);
	    default:
		LOG(L_ERR,"ERROR: %s: %s\n",exports.name,strerror(errno));
	}
	return -1;
    }

    if ( !(fp_fifo = fdopen(fd_fifo,"w")) ) {
	LOG(L_ERR,"ERROR: %s: %s\n",exports.name,strerror(errno));
	return -1;
    }

    fwrite(buf,len,1,fp_fifo);
    fclose(fp_fifo);

    DBG("DEBUG: write_to_vm_fifo: write completed\n");

    return 1; // OK
}









