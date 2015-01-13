/*
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
 * \file
 * \brief Exec module:: Module interface
 * \ingroup exec 
 * Module: \ref exec
 *
 * functions for creating environment variables out of a request's
 * header; known compact header field names are translated to
 * canonical form; multiple header field occurrences are merged
 * into a single variable
 *
 * known limitations: 
 * - compact header field names unknown to parser will not be translated to 
 *   canonical form. Thus, environment variables may have either name and 
 *   users have to check for both of them.
 * - symbols in header field names will be translated to underscore
 *
 */

#include <stdlib.h>

#include "../../parser/msg_parser.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_via.h"
#include "../../parser/parse_uri.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../md5utils.h"
#include "../../char_msg_val.h"
#include "exec_hf.h"

extern int exec_bash_safety;

/* should be environment variables set by header fields ? */
unsigned int setvars=1;

/* insert a new header field into the structure; */
static int insert_hf( struct hf_wrapper **list, struct hdr_field *hf )
{
	struct hf_wrapper *w; /* new wrapper */
	struct hf_wrapper *i;

	w=(struct hf_wrapper *)pkg_malloc(sizeof(struct hf_wrapper));
	if (!w) {
		LM_ERR("ran out of pkg mem\n");
		return 0;
	}
	memset(w, 0, sizeof(struct hf_wrapper));
	w->var_type=W_HF;w->u.hf=hf; 
	w->prefix=HF_PREFIX; w->prefix_len=HF_PREFIX_LEN;

	/* is there another hf of the same type?... */
	for(i=*list; i; i=i->next_other) {
		if (i->var_type==W_HF && i->u.hf->type==hf->type) {
			/* if it is OTHER, check name too */
			if (hf->type==HDR_OTHER_T && (hf->name.len!=i->u.hf->name.len
					|| strncasecmp(i->u.hf->name.s, hf->name.s, 
					   hf->name.len)!=0))
				continue;
			/* yes, we found a hf of same type */
			w->next_same=i->next_same;
			w->next_other=i->next_other;
			i->next_same=w;
			break;
		}
	}
	/* ... no previous HF of the same type found */
	if (i==0) {
		w->next_other=*list;
		*list=w;
	}
	return 1;
}

static void release_hf_struct( struct hf_wrapper *list )
{
	struct hf_wrapper *i, *j, *nexts, *nexto;

	i=list;
	while(i) {
		nexto=i->next_other;
		j=i->next_same;
		pkg_free(i);
		/* release list of same type hf */
		while(j) {
			nexts=j->next_same;
			pkg_free(j);
			j=nexts;
		}
		i=nexto;
	}
}

/* if that is some of well-known header fields which have compact
 * form, return canonical form ... returns 1 and sets params;
 * 0 is returned otherwise */
static int compacthdr_type2str(hdr_types_t  type, char **hname, int *hlen )
{
	switch(type) {
		/* HDR_CONTENT_ENCODING: 'e' -- unsupported by parser */
		/* HDR_SUBJECT: 's' -- unsupported by parser */
		case HDR_VIA_T /* v */ : 
			*hname=VAR_VIA;
			*hlen=VAR_VIA_LEN;
			break;
		case HDR_CONTENTTYPE_T /* c */ : 
			*hname=VAR_CTYPE;
			*hlen=VAR_CTYPE_LEN;
			break;
		case HDR_FROM_T /* f */: 
			*hname=VAR_FROM;
			*hlen=VAR_FROM_LEN;
			break;
		case HDR_CALLID_T /* i */: 
			*hname=VAR_CALLID;
			*hlen=VAR_CALLID_LEN;
			break;
		case HDR_SUPPORTED_T /* k */: 
			*hname=VAR_SUPPORTED;
			*hlen=VAR_SUPPORTED_LEN;
			break;
		case HDR_CONTENTLENGTH_T /* l */: 
			*hname=VAR_CLEN;
			*hlen=VAR_CLEN_LEN;
			break;
		case HDR_CONTACT_T /* m */: 
			*hname=VAR_CONTACT;
			*hlen=VAR_CONTACT_LEN;
			break;
		case HDR_TO_T /* t */: 
			*hname=VAR_TO;
			*hlen=VAR_TO_LEN;
			break;
		case HDR_EVENT_T /* o */: 
			*hname=VAR_EVENT;
			*hlen=VAR_EVENT_LEN;
			break;
		default:	
			return 0;
	}
	return 1;
}


static int canonize_headername(str *orig, char **hname, int *hlen )
{
	char *c;
	int i;

	*hlen=orig->len;
	*hname=pkg_malloc(*hlen);
	if (!*hname) {
		LM_ERR("no pkg mem for hname\n");
		return 0;
	}
	for (c=orig->s, i=0; i<*hlen; i++, c++) {
		/* lowercase to uppercase */
		if (*c>='a' && *c<='z') 
			*((*hname)+i)=*c-('a'-'A');
			/* uppercase and numbers stay "as is" */
		else if ((*c>='A' && *c<='Z')||(*c>='0' && *c<='9')) 
			*((*hname)+i)=*c;
		/* legal symbols will be translated to underscore */
		else if (strchr(UNRESERVED_MARK HNV_UNRESERVED, *c)
				|| (*c==ESCAPE))
			*((*hname)+i)=HFN_SYMBOL;
		else {
			LM_ERR("print_var unexpected char '%c' in hfname %.*s\n", 
					*c, *hlen, orig->s );
			*((*hname)+i)=HFN_SYMBOL;
		}
	}
	return 1;
}


static int print_av_var(struct hf_wrapper *w)
{
	int env_len;
	char *env;
	char *c;

	env_len=w->u.av.attr.len+1/*assignment*/+w->u.av.val.len+1/*ZT*/;
	env=pkg_malloc(env_len);
	if (!env) {
		LM_ERR("no pkg mem\n");
		return 0;
	}
	c=env;
	memcpy(c, w->u.av.attr.s, w->u.av.attr.len); c+=w->u.av.attr.len;
	*c=EV_ASSIGN;c++;
	memcpy(c, w->u.av.val.s, w->u.av.val.len);c+=w->u.av.val.len;
	*c=0; /* zero termination */
	w->envvar=env;
	return 1;
}

/* creates a malloc-ed string with environment variable; returns 1 on success,
 * 0 on failure  */
static int print_hf_var(struct hf_wrapper *w, int offset)
{
	char *hname;
	int hlen;
	short canonical;
	char *envvar;
	int envvar_len;
	struct hf_wrapper *wi;
	char *c;

	/* make -Wall happy */
	hname=0;hlen=0;envvar=0;

	/* Make sure header names with possible compact forms
	 * will be printed canonically
	 */
	canonical=compacthdr_type2str(w->u.hf->type, &hname, &hlen);
	/* header field has not been made canonical using a table;
	 * do it now by uppercasing header-field name */
	if (!canonical) {
		if (!canonize_headername(&w->u.hf->name, &hname, &hlen)) {
			LM_ERR("canonize_hn error\n");
			return 0;
		}
	} 
	/* now we have a header name, let us generate the var */
	envvar_len=w->u.hf->body.len;
	for(wi=w->next_same; wi; wi=wi->next_same) { /* other values, separated */
		envvar_len+=1 /* separator */ + wi->u.hf->body.len;
	}
	envvar=pkg_malloc(w->prefix_len+hlen+1/*assignment*/+envvar_len+1/*ZT*/);
	if (!envvar) {
		LM_ERR("no pkg mem\n");
		goto error00;
	}
	memcpy(envvar, w->prefix, w->prefix_len); c=envvar+w->prefix_len;
	memcpy(c, hname, hlen ); c+=hlen;
	*c=EV_ASSIGN;c++;
	if (exec_bash_safety && w->u.hf->body.len>=4
			&& !strncmp(w->u.hf->body.s, "() {", 4)) {
		memcpy(c, w->u.hf->body.s+offset+2, w->u.hf->body.len-2 );
		c+=(w->u.hf->body.len-2);
	} else {
		memcpy(c, w->u.hf->body.s+offset, w->u.hf->body.len );
		c+=w->u.hf->body.len;
	}
	for(wi=w->next_same; wi; wi=wi->next_same) {
		*c=HF_SEPARATOR;c++;
		if (exec_bash_safety && wi->u.hf->body.len>=4
				&& !strncmp(wi->u.hf->body.s, "() {", 4)) {
			memcpy(c, wi->u.hf->body.s+offset+2, wi->u.hf->body.len-2 );
			c+=(wi->u.hf->body.len-2);
		} else {
			memcpy(c, wi->u.hf->body.s+offset, wi->u.hf->body.len );
			c+=wi->u.hf->body.len;
		}
	}
	*c=0; /* zero termination */
	LM_DBG("%s\n", envvar );
	
	w->envvar=envvar;
	if (!canonical) pkg_free(hname);
	return 1;

error00:
	if (!canonical) pkg_free(hname);
	return 0;
}

static int print_var(struct hf_wrapper *w, int offset)
{
	switch(w->var_type) {
		case W_HF: 
			return print_hf_var(w, offset);
		case W_AV: 
			return print_av_var(w);
		default:
		   	LM_CRIT("unknown type: %d\n", w->var_type );
			return 0;
	}
}

static void release_vars(struct hf_wrapper *list) 
{
	while(list) {
		if (list->envvar) {
			pkg_free(list->envvar);
			list->envvar=0;
		}
		list=list->next_other;
	}
}

/* create ordered HF structure in pkg memory */
static int build_hf_struct(struct sip_msg *msg, struct hf_wrapper **list)
{
	struct hdr_field *h;

	*list=0;
	/* create ordered header-field structure */
	for (h=msg->headers; h; h=h->next) {
		if (!insert_hf(list,h)) {
			LM_ERR("insert_hf failed\n");
			goto error00;
		}
	}
	return 1;
error00:
	release_hf_struct(*list);
	*list=0;
	return 0;

}

/* create env vars in malloc memory */
static int create_vars(struct hf_wrapper *list, int offset)
{
	int var_cnt;
	struct hf_wrapper *w;

	/* create variables now */
	var_cnt=0;
	for(w=list;w;w=w->next_other) {
		if (!print_var(w, offset)) {
			LM_ERR("create_vars failed\n");
			return 0;
		}
		var_cnt++;
	}

	return var_cnt;
}

environment_t *replace_env(struct hf_wrapper *list)
{
	int var_cnt;
	char **cp;
	struct hf_wrapper *w;
	char **new_env;
	int i;
	environment_t *backup_env;

	backup_env=(environment_t *)pkg_malloc(sizeof(environment_t));
	if (!backup_env) {
		LM_ERR("no pkg mem for backup env\n");
		return 0;
	}

	/* count length of current env list */
	var_cnt=0;
	for (cp=environ; *cp; cp++) var_cnt++;
	backup_env->old_cnt=var_cnt;
	/* count length of our extensions */
	for(w=list;w;w=w->next_other) var_cnt++;
	new_env=pkg_malloc((var_cnt+1)*sizeof(char *));
	if (!new_env) {
		LM_ERR("no pkg mem\n");
		pkg_free(backup_env);
		return 0;
	}
	/* put all var pointers into new environment */
	i=0;
	for (cp=environ; *cp; cp++) { /* replicate old env */
		new_env[i]=*cp;
		i++;
	}
	for (w=list;w;w=w->next_other) { /* append new env */
		new_env[i]=w->envvar;
		i++;
	}
	new_env[i]=0; /* zero termination */
	/* install new environment */
	backup_env->env=environ;
	environ=new_env;
	/* return previous environment */
	return backup_env;
}

void unset_env(environment_t *backup_env)
{
	char **cur_env, **cur_env0;
	int i;

	/* switch-over to backup environment */
	cur_env0=cur_env=environ;
	environ=backup_env->env;
	i=0;
	/* release environment */
	while(*cur_env) {
		/* leave previously existing vars alone */
		if (i>=backup_env->old_cnt) {
			pkg_free(*cur_env);
		}
		cur_env++;
		i++;
	}
	pkg_free(cur_env0);
	pkg_free(backup_env);
}

static int append_var(char *name, char *value, int len, struct hf_wrapper **list)
{
	struct hf_wrapper *w;

	w=(struct hf_wrapper *)pkg_malloc(sizeof(struct hf_wrapper));
	if (!w) {
		LM_ERR("ran out of pkg mem\n");
		return 0;
	}
	memset(w, 0, sizeof(struct hf_wrapper)); 
	w->var_type=W_AV;
	w->u.av.attr.s=name;
	w->u.av.attr.len=strlen(name);
	w->u.av.val.s=value;
	/* NULL strings considered empty, if len unknown, calculate it now */
	w->u.av.val.len= value==0?0:(len==0? strlen(value) : len);
	w->next_other=*list;
	*list=w;
	return 1;
}

static int append_fixed_vars(struct sip_msg *msg, struct hf_wrapper **list)
{
	static char tid[MD5_LEN];
	str *uri;
	struct sip_uri parsed_uri, oparsed_uri;
	char *val;
	int val_len;

	/* source ip */
	if (!append_var(EV_SRCIP, ip_addr2a(&msg->rcv.src_ip), 0, list)) {
		LM_ERR("append_var SRCIP failed \n");
		return 0;
	}
	/* request URI */
	uri=msg->new_uri.s && msg->new_uri.len ? 
		&msg->new_uri : &msg->first_line.u.request.uri;
	if (!append_var(EV_RURI, uri->s, uri->len, list )) {
		LM_ERR("append_var URI failed\n");
		return 0;
	}
	/* userpart of request URI */
	if (parse_uri(uri->s, uri->len, &parsed_uri)<0) {
		LM_WARN("uri not parsed\n");
	} else {
		if (!append_var(EV_USER, parsed_uri.user.s, 
					parsed_uri.user.len, list)) {
			LM_ERR("append_var USER failed\n");
			goto error;
		}
	}
	/* original URI */
	if (!append_var(EV_ORURI, msg->first_line.u.request.uri.s,
				msg->first_line.u.request.uri.len, list)) {
		LM_ERR("append_var O-URI failed\n");
		goto error;
	}
	/* userpart of request URI */
	if (parse_uri(msg->first_line.u.request.uri.s, 
				msg->first_line.u.request.uri.len, 
				&oparsed_uri)<0) {
		LM_WARN("orig URI not parsed\n");
	} else {
		if (!append_var(EV_OUSER, oparsed_uri.user.s, 
					oparsed_uri.user.len, list)) {
			LM_ERR("ppend_var OUSER failed\n");
			goto error;
		}
	}
	/* tid, transaction id == via/branch */
	if (!char_msg_val(msg, tid)) {
		LM_WARN("no tid can be determined\n");
		val=0; val_len=0;
	} else {
		val=tid;val_len=MD5_LEN;
	}
	if (!append_var(EV_TID, val,val_len, list)) {
		LM_ERR("append_var TID failed\n");
		goto error;
	}

	/* did, dialogue id == To-tag */
	if (!(msg->to && get_to(msg) ))  {
		LM_ERR("no to-tag\n");
		val=0; val_len=0;
	} else {
		val=get_to(msg)->tag_value.s;
		val_len=get_to(msg)->tag_value.len;
	}
	if (!append_var(EV_DID, val, val_len, list)) {
		LM_ERR("append_var DID failed\n");
		goto error;
	}
	return 1;
error:
	return 0;
}

environment_t *set_env(struct sip_msg *msg)
{
	struct hf_wrapper *hf_list;
	environment_t *backup_env;

	/* parse all so that we can pass all header fields to script */
	if (parse_headers(msg, HDR_EOH_F, 0)==-1) {
		LM_ERR("parsing failed\n");
		return 0;
	}

	hf_list=0;
	/* create a temporary structure with ordered header fields
	 * and create environment variables out of it */
	if (!build_hf_struct(msg, &hf_list)) {
		LM_ERR("build_hf_struct failed\n");
		return 0;
	}
	if (!append_fixed_vars(msg, &hf_list)) {
		LM_ERR("append_fixed_vars failed\n");
		goto error01;
	}
	/* create now the strings for environment variables */
	if (!create_vars(hf_list, 0)) {
		LM_ERR("create_vars failed\n");
		goto error00;
	}
	/* install the variables in current environment */
	backup_env=replace_env(hf_list);
	if (!backup_env) {
		LM_ERR("replace_env failed\n");
		goto error00;
	}
	/* release the ordered HF structure -- we only need the vars now */
	release_hf_struct(hf_list);
	return backup_env;

error00:
	release_vars(hf_list); /* release variables */
error01:
	release_hf_struct(hf_list); /* release temporary ordered HF struct */
	return 0;
}

