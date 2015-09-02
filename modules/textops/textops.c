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
 *
 */

/*!
 * \file
 * \brief Module interface
 * \ingroup textops
 * Module: \ref textops
 */

/**
 * @defgroup Textops Various text operatoins on messages
 * @brief Kamailio textops module
 */



#include "../../action.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../re.h"
#include "../../mod_fix.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_hname2.h"
#include "../../parser/parse_methods.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_param.h"
#include "../../parser/sdp/sdp.h"
#include "../../parser/sdp/sdp_helpr_funcs.h"
#include "../../lib/kcore/parse_privacy.h"
#include "../../msg_translator.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../lib/kcore/cmpapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* for regex */
#include <regex.h>
#include <time.h>
#include <sys/time.h>

#include "textops.h"
#include "txt_var.h"
#include "api.h"

MODULE_VERSION


/* RFC822-conforming dates format:

   %a -- abbreviated week of day name (locale), %d day of month
   as decimal number, %b abbreviated month name (locale), %Y
   year with century, %T time in 24h notation
*/
#define TIME_FORMAT "Date: %a, %d %b %Y %H:%M:%S GMT"
#define MAX_TIME 64

#define AUDIO_STR "audio"
#define AUDIO_STR_LEN 5


static int search_body_f(struct sip_msg*, char*, char*);
static int search_hf_f(struct sip_msg*, char*, char*, char*);
static int replace_f(struct sip_msg*, char*, char*);
static int replace_body_f(struct sip_msg*, char*, char*);
static int replace_all_f(struct sip_msg*, char*, char*);
static int replace_body_all_f(struct sip_msg*, char*, char*);
static int replace_body_atonce_f(struct sip_msg*, char*, char*);
static int subst_f(struct sip_msg*, char*, char*);
static int subst_uri_f(struct sip_msg*, char*, char*);
static int subst_user_f(struct sip_msg*, char*, char*);
static int subst_body_f(struct sip_msg*, char*, char*);
static int subst_hf_f(struct sip_msg*, char*, char*, char*);
static int filter_body_f(struct sip_msg*, char*, char*);
static int is_present_hf_f(struct sip_msg* msg, char* str_hf, char* foo);
static int search_append_body_f(struct sip_msg*, char*, char*);
static int append_to_reply_f(struct sip_msg* msg, char* key, char* str);
static int append_hf_1(struct sip_msg* msg, char* str1, char* str2);
static int append_hf_2(struct sip_msg* msg, char* str1, char* str2);
static int insert_hf_1(struct sip_msg* msg, char* str1, char* str2);
static int insert_hf_2(struct sip_msg* msg, char* str1, char* str2);
static int append_urihf(struct sip_msg* msg, char* str1, char* str2);
static int append_time_f(struct sip_msg* msg, char* , char *);
static int append_time_request_f(struct sip_msg* msg, char* , char *);
static int set_body_f(struct sip_msg* msg, char*, char *);
static int set_rpl_body_f(struct sip_msg* msg, char*, char *);
static int set_multibody_0(struct sip_msg* msg, char*, char *, char *);
static int set_multibody_1(struct sip_msg* msg, char*, char *, char *);
static int set_multibody_2(struct sip_msg* msg, char*, char *, char *);
static int set_multibody_3(struct sip_msg* msg, char*, char *, char *);
static int append_multibody_2(struct sip_msg* msg, char*, char *);
static int append_multibody_3(struct sip_msg* msg, char*, char *, char *);
static int fixup_multibody_f(void** param, int param_no);
static int remove_multibody_f(struct sip_msg *msg, char *);
static int is_method_f(struct sip_msg* msg, char* , char *);
static int has_body_f(struct sip_msg *msg, char *type, char *str2 );
static int in_list_f(struct sip_msg* _msg, char* _subject, char* _list,
 		     char* _sep);
static int cmp_str_f(struct sip_msg *msg, char *str1, char *str2 );
static int cmp_istr_f(struct sip_msg *msg, char *str1, char *str2 );
static int starts_with_f(struct sip_msg *msg, char *str1, char *str2 );
static int remove_hf_re_f(struct sip_msg* msg, char* key, char* foo);
static int is_present_hf_re_f(struct sip_msg* msg, char* key, char* foo);
static int is_audio_on_hold_f(struct sip_msg *msg, char *str1, char *str2 );
static int fixup_substre(void**, int);
static int hname_fixup(void** param, int param_no);
static int free_hname_fixup(void** param, int param_no);
static int fixup_method(void** param, int param_no);
static int add_header_fixup(void** param, int param_no);
static int fixup_body_type(void** param, int param_no);
static int fixup_in_list(void** param, int param_no);
static int fixup_free_in_list(void** param, int param_no);
int fixup_regexpNL_none(void** param, int param_no);
static int fixup_search_hf(void** param, int param_no);
static int fixup_subst_hf(void** param, int param_no);

static int mod_init(void);

static tr_export_t mod_trans[] = {
	{ {"re", sizeof("re")-1}, /* regexp class */
		tr_txt_parse_re },

	{ { 0, 0 }, 0 }
};

static cmd_export_t cmds[]={
	{"search",           (cmd_function)search_f,          1,
		fixup_regexp_null, fixup_free_regexp_null,
		ANY_ROUTE},
	{"search_body",      (cmd_function)search_body_f,     1,
		fixup_regexp_null, fixup_free_regexp_null,
		ANY_ROUTE},
	{"search_hf",      (cmd_function)search_hf_f,         3,
		fixup_search_hf, 0,
		ANY_ROUTE},
	{"search_append",    (cmd_function)search_append_f,   2,
		fixup_regexp_none,fixup_free_regexp_none,
		ANY_ROUTE},
	{"search_append_body", (cmd_function)search_append_body_f,   2,
		fixup_regexp_none, fixup_free_regexp_none, 
		ANY_ROUTE},
	{"replace",          (cmd_function)replace_f,         2,
		fixup_regexp_none, fixup_free_regexp_none,
		ANY_ROUTE},
	{"replace_body",     (cmd_function)replace_body_f,    2,
		fixup_regexp_none, fixup_free_regexp_none,
		ANY_ROUTE},
	{"replace_all",      (cmd_function)replace_all_f,     2,
		fixup_regexp_none, fixup_free_regexp_none,
		ANY_ROUTE},
	{"replace_body_all", (cmd_function)replace_body_all_f,2,
		fixup_regexp_none, fixup_free_regexp_none,
		ANY_ROUTE},
	{"replace_body_atonce", (cmd_function)replace_body_atonce_f,2,
		fixup_regexpNL_none, fixup_free_regexp_none,
		ANY_ROUTE},
	{"append_to_reply",  (cmd_function)append_to_reply_f, 1,
		fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"append_hf",        (cmd_function)append_hf_1,       1,
		add_header_fixup, 0,
		ANY_ROUTE},
	{"append_hf",        (cmd_function)append_hf_2,       2,
		add_header_fixup, 0,
		ANY_ROUTE},
	{"insert_hf",        (cmd_function)insert_hf_1,       1, 
		add_header_fixup, 0,
		ANY_ROUTE},
	{"insert_hf",        (cmd_function)insert_hf_2,       2, 
		add_header_fixup, 0,
		ANY_ROUTE},
	{"append_urihf",     (cmd_function)append_urihf,      2,
		fixup_str_str, fixup_free_str_str,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"remove_hf",        (cmd_function)remove_hf_f,       1,
		hname_fixup, free_hname_fixup,
		ANY_ROUTE},
	{"remove_hf_re",     (cmd_function)remove_hf_re_f,    1,
		fixup_regexp_null, fixup_free_regexp_null,
		ANY_ROUTE},
	{"is_present_hf",    (cmd_function)is_present_hf_f,   1,
		hname_fixup, free_hname_fixup,
		ANY_ROUTE},
	{"is_present_hf_re", (cmd_function)is_present_hf_re_f,1,
		fixup_regexp_null, fixup_free_regexp_null,
		ANY_ROUTE},
	{"subst",            (cmd_function)subst_f,           1,
		fixup_substre, 0,
		ANY_ROUTE},
	{"subst_uri",        (cmd_function)subst_uri_f,       1,
		fixup_substre, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"subst_user",       (cmd_function)subst_user_f,      1,
		fixup_substre, 0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"subst_body",       (cmd_function)subst_body_f,      1,
		fixup_substre, 0,
		ANY_ROUTE},
	{"subst_hf",         (cmd_function)subst_hf_f,        3,
		fixup_subst_hf, 0,
		ANY_ROUTE},
	{"filter_body",      (cmd_function)filter_body_f,     1,
		fixup_str_null, 0,
		ANY_ROUTE},
	{"append_time",      (cmd_function)append_time_f,     0,
		0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"set_body",         (cmd_function)set_body_f,        2,
		fixup_spve_spve, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE },
	{"set_reply_body",     (cmd_function)set_rpl_body_f,    2,
		fixup_spve_spve, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"is_method",        (cmd_function)is_method_f,       1,
		fixup_method, 0,
		ANY_ROUTE},
	{"has_body",         (cmd_function)has_body_f,        0,
		0, 0,
		ANY_ROUTE},
	{"has_body",         (cmd_function)has_body_f,        1, 
		fixup_body_type, 0,
		ANY_ROUTE},
	{"is_privacy",       (cmd_function)is_privacy_f,      1,
		fixup_privacy, 0,
		ANY_ROUTE},
 	{"in_list", (cmd_function)in_list_f, 3, fixup_in_list,
	        fixup_free_in_list,
 	        ANY_ROUTE},
	{"cmp_str",  (cmd_function)cmp_str_f, 2,
		fixup_spve_spve, 0,
		ANY_ROUTE},
	{"cmp_istr",  (cmd_function)cmp_istr_f, 2,
		fixup_spve_spve, 0,
		ANY_ROUTE},
	{"starts_with",  (cmd_function)starts_with_f, 2,
		fixup_spve_spve, 0,
		ANY_ROUTE},
	{"is_audio_on_hold",  (cmd_function)is_audio_on_hold_f, 0,
		0, 0,
		ANY_ROUTE},
	{"append_time_to_request", (cmd_function)append_time_request_f, 0,
		0, 0,
		ANY_ROUTE},

	{"bind_textops",      (cmd_function)bind_textops,       0, 0, 0,
		0},
	{"set_body_multipart",         (cmd_function)set_multibody_0,        0,
		0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"set_body_multipart",         (cmd_function)set_multibody_1,        1,
		fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"set_body_multipart",         (cmd_function)set_multibody_2,        2,
		fixup_spve_spve, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"set_body_multipart",         (cmd_function)set_multibody_3,        3,
		fixup_multibody_f, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"append_body_part",     (cmd_function)append_multibody_2,    2,
		fixup_spve_spve, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"append_body_part",     (cmd_function)append_multibody_3,    3,
		fixup_multibody_f, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },
	{"remove_body_part",     (cmd_function)remove_multibody_f,    1,
		fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE },

	{0,0,0,0,0,0}
};


struct module_exports exports= {
	"textops",  /* module name*/
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* exported functions */
	0,          /* module parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	0,          /* per-child init function */
};


static int mod_init(void)
{
	return 0;
}

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	return register_trans_mod(path, mod_trans);
}

static char *get_header(struct sip_msg *msg)
{
	return msg->buf+msg->first_line.len;
}



int search_f(struct sip_msg* msg, char* key, char* str2)
{
	/*we registered only 1 param, so we ignore str2*/
	regmatch_t pmatch;

	if (regexec((regex_t*) key, msg->buf, 1, &pmatch, 0)!=0) return -1;
	return 1;
}


static int search_body_f(struct sip_msg* msg, char* key, char* str2)
{
	str body;
	/*we registered only 1 param, so we ignore str2*/
	regmatch_t pmatch;

	body.s = get_body(msg);
	if (body.s==0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s-msg->buf);
	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	if (regexec((regex_t*) key, body.s, 1, &pmatch, 0)!=0) return -1;
	return 1;
}


int search_append_f(struct sip_msg* msg, char* key, char* str2)
{
	struct lump* l;
	regmatch_t pmatch;
	char* s;
	int len;
	char *begin;
	int off;

	begin=get_header(msg); /* msg->orig/buf previously .. uri problems */
	off=begin-msg->buf;

	if (regexec((regex_t*) key, begin, 1, &pmatch, 0)!=0) return -1;
	if (pmatch.rm_so!=-1){
		if ((l=anchor_lump(msg, off+pmatch.rm_eo, 0, 0))==0)
			return -1;
		len=strlen(str2);
		s=pkg_malloc(len);
		if (s==0){
			LM_ERR("memory allocation failure\n");
			return -1;
		}
		memcpy(s, str2, len); 
		if (insert_new_lump_after(l, s, len, 0)==0){
			LM_ERR("could not insert new lump\n");
			pkg_free(s);
			return -1;
		}
		return 1;
	}
	return -1;
}

static int search_append_body_f(struct sip_msg* msg, char* key, char* str2)
{
	struct lump* l;
	regmatch_t pmatch;
	char* s;
	int len;
	int off;
	str body;

	body.s = get_body(msg);
	if (body.s==0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s-msg->buf);
	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	off=body.s-msg->buf;

	if (regexec((regex_t*) key, body.s, 1, &pmatch, 0)!=0) return -1;
	if (pmatch.rm_so!=-1){
		if ((l=anchor_lump(msg, off+pmatch.rm_eo, 0, 0))==0)
			return -1;
		len=strlen(str2);
		s=pkg_malloc(len);
		if (s==0){
			LM_ERR("memory allocation failure\n");
			return -1;
		}
		memcpy(s, str2, len); 
		if (insert_new_lump_after(l, s, len, 0)==0){
			LM_ERR("could not insert new lump\n");
			pkg_free(s);
			return -1;
		}
		return 1;
	}
	return -1;
}


static int replace_all_f(struct sip_msg* msg, char* key, char* str2)
{
	struct lump* l;
	regmatch_t pmatch;
	char* s;
	int len;
	char* begin;
	int off;
	int ret;
	int eflags;

	begin = get_header(msg);
	ret=-1; /* pessimist: we will not find any */
	len=strlen(str2);
	eflags=0; /* match ^ at the beginning of the string*/

	while (begin<msg->buf+msg->len 
				&& regexec((regex_t*) key, begin, 1, &pmatch, eflags)==0) {
		off=begin-msg->buf;
		if (pmatch.rm_so==-1){
			LM_ERR("offset unknown\n");
			return -1;
		}
		if (pmatch.rm_so==pmatch.rm_eo){
			LM_ERR("matched string is empty... invalid regexp?\n");
			return -1;
		}
		if ((l=del_lump(msg, pmatch.rm_so+off,
						pmatch.rm_eo-pmatch.rm_so, 0))==0) {
			LM_ERR("del_lump failed\n");
			return -1;
		}
		s=pkg_malloc(len);
		if (s==0){
			LM_ERR("memory allocation failure\n");
			return -1;
		}
		memcpy(s, str2, len); 
		if (insert_new_lump_after(l, s, len, 0)==0){
			LM_ERR("could not insert new lump\n");
			pkg_free(s);
			return -1;
		}
		/* new cycle */
		begin=begin+pmatch.rm_eo;
		/* is it still a string start */
		if (*(begin-1)=='\n' || *(begin-1)=='\r')
			eflags&=~REG_NOTBOL;
		else
			eflags|=REG_NOTBOL;
		ret=1;
	} /* while found ... */
	return ret;
}

static int do_replace_body_f(struct sip_msg* msg, char* key, char* str2, int nobol)
{
	struct lump* l;
	regmatch_t pmatch;
	char* s;
	int len;
	char* begin;
	int off;
	int ret;
	int eflags;
	str body;

	body.s = get_body(msg);
	if (body.s==0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s-msg->buf);
	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	begin=body.s;
	ret=-1; /* pessimist: we will not find any */
	len=strlen(str2);
	eflags=0; /* match ^ at the beginning of the string*/

	while (begin<msg->buf+msg->len 
				&& regexec((regex_t*) key, begin, 1, &pmatch, eflags)==0) {
		off=begin-msg->buf;
		if (pmatch.rm_so==-1){
			LM_ERR("offset unknown\n");
			return -1;
		}
		if (pmatch.rm_so==pmatch.rm_eo){
			LM_ERR("matched string is empty... invalid regexp?\n");
			return -1;
		}
		if ((l=del_lump(msg, pmatch.rm_so+off,
						pmatch.rm_eo-pmatch.rm_so, 0))==0) {
			LM_ERR("del_lump failed\n");
			return -1;
		}
		s=pkg_malloc(len);
		if (s==0){
			LM_ERR("memory allocation failure\n");
			return -1;
		}
		memcpy(s, str2, len); 
		if (insert_new_lump_after(l, s, len, 0)==0){
			LM_ERR("could not insert new lump\n");
			pkg_free(s);
			return -1;
		}
		/* new cycle */
		begin=begin+pmatch.rm_eo;
		/* is it still a string start */
		if (nobol && (*(begin-1)=='\n' || *(begin-1)=='\r'))
			eflags&=~REG_NOTBOL;
		else
			eflags|=REG_NOTBOL;
		ret=1;
	} /* while found ... */
	return ret;
}

static int replace_body_all_f(struct sip_msg* msg, char* key, char* str2)
{
	return do_replace_body_f(msg, key, str2, 1);
}

static int replace_body_atonce_f(struct sip_msg* msg, char* key, char* str2)
{
	return do_replace_body_f(msg, key, str2, 0);
}

static int replace_f(struct sip_msg* msg, char* key, char* str2)
{
	struct lump* l;
	regmatch_t pmatch;
	char* s;
	int len;
	char* begin;
	int off;

	begin=get_header(msg); /* msg->orig previously .. uri problems */

	if (regexec((regex_t*) key, begin, 1, &pmatch, 0)!=0) return -1;
	off=begin-msg->buf;

	if (pmatch.rm_so!=-1){
		if ((l=del_lump(msg, pmatch.rm_so+off,
						pmatch.rm_eo-pmatch.rm_so, 0))==0)
			return -1;
		len=strlen(str2);
		s=pkg_malloc(len);
		if (s==0){
			LM_ERR("memory allocation failure\n");
			return -1;
		}
		memcpy(s, str2, len); 
		if (insert_new_lump_after(l, s, len, 0)==0){
			LM_ERR("could not insert new lump\n");
			pkg_free(s);
			return -1;
		}
		
		return 1;
	}
	return -1;
}

static int replace_body_f(struct sip_msg* msg, char* key, char* str2)
{
	struct lump* l;
	regmatch_t pmatch;
	char* s;
	int len;
	char* begin;
	int off;
	str body;

	body.s = get_body(msg);
	if (body.s==0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s-msg->buf);
	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	begin=body.s; /* msg->orig previously .. uri problems */

	if (regexec((regex_t*) key, begin, 1, &pmatch, 0)!=0) return -1;
	off=begin-msg->buf;

	if (pmatch.rm_so!=-1){
		if ((l=del_lump(msg, pmatch.rm_so+off,
						pmatch.rm_eo-pmatch.rm_so, 0))==0)
			return -1;
		len=strlen(str2);
		s=pkg_malloc(len);
		if (s==0){
			LM_ERR("memory allocation failure\n");
			return -1;
		}
		memcpy(s, str2, len); 
		if (insert_new_lump_after(l, s, len, 0)==0){
			LM_ERR("could not insert new lump\n");
			pkg_free(s);
			return -1;
		}
		
		return 1;
	}
	return -1;
}


/* sed-perl style re: s/regular expression/replacement/flags */
static int subst_f(struct sip_msg* msg, char*  subst, char* ignored)
{
	struct lump* l;
	struct replace_lst* lst;
	struct replace_lst* rpl;
	char* begin;
	struct subst_expr* se;
	int off;
	int ret;
	int nmatches;
	
	se=(struct subst_expr*)subst;
	begin=get_header(msg);  /* start after first line to avoid replacing
							   the uri */
	off=begin-msg->buf;
	ret=-1;
	if ((lst=subst_run(se, begin, msg, &nmatches))==0)
		goto error; /* not found */
	for (rpl=lst; rpl; rpl=rpl->next){
		LM_DBG("%s: replacing at offset %d [%.*s] with [%.*s]\n",
				exports.name, rpl->offset+off,
				rpl->size, rpl->offset+off+msg->buf,
				rpl->rpl.len, rpl->rpl.s);
		if ((l=del_lump(msg, rpl->offset+off, rpl->size, 0))==0)
			goto error;
		/* hack to avoid re-copying rpl, possible because both 
		 * replace_lst & lumps use pkg_malloc */
		if (insert_new_lump_after(l, rpl->rpl.s, rpl->rpl.len, 0)==0){
			LM_ERR("%s: could not insert new lump\n", exports.name);
			goto error;
		}
		/* hack continued: set rpl.s to 0 so that replace_lst_free will
		 * not free it */
		rpl->rpl.s=0;
		rpl->rpl.len=0;
	}
	ret=1;
error:
	LM_DBG("lst was %p\n", lst);
	if (lst) replace_lst_free(lst);
	if (nmatches<0)
		LM_ERR("%s: subst_run failed\n", exports.name);
	return ret;
}



/* sed-perl style re: s/regular expression/replacement/flags, like
 *  subst but works on the message uri */
static int subst_uri_f(struct sip_msg* msg, char*  subst, char* ignored)
{
	char* tmp;
	int len;
	char c;
	struct subst_expr* se;
	str* result;
	
	se=(struct subst_expr*)subst;
	if (msg->new_uri.s){
		len=msg->new_uri.len;
		tmp=msg->new_uri.s;
	}else{
		tmp=msg->first_line.u.request.uri.s;
		len	=msg->first_line.u.request.uri.len;
	};
	/* ugly hack: 0 s[len], and restore it afterward
	 * (our re functions require 0 term strings), we can do this
	 * because we always alloc len+1 (new_uri) and for first_line, the
	 * message will always be > uri.len */
	c=tmp[len];
	tmp[len]=0;
	result=subst_str(tmp, msg, se, 0); /* pkg malloc'ed result */
	tmp[len]=c;
	if (result){
		LM_DBG("%s match - old uri= [%.*s], new uri= [%.*s]\n",
				exports.name, len, tmp,
				(result->len)?result->len:0,(result->s)?result->s:"");
		if (msg->new_uri.s) pkg_free(msg->new_uri.s);
		msg->new_uri=*result;
		msg->parsed_uri_ok=0; /* reset "use cached parsed uri" flag */
		ruri_mark_new();
		pkg_free(result); /* free str* pointer */
		return 1; /* success */
	}
	return -1; /* false, no subst. made */
}
	


/* sed-perl style re: s/regular expression/replacement/flags, like
 *  subst but works on the user part of the uri */
static int subst_user_f(struct sip_msg* msg, char*  subst, char* ignored)
{
	int rval;
	str* result;
	struct subst_expr* se;
	struct action act;
	struct run_act_ctx h;
	str user;
	char c;
	int nmatches;

	c=0;
	if (parse_sip_msg_uri(msg)<0){
		return -1; /* error, bad uri */
	}
	if (msg->parsed_uri.user.s==0){
		/* no user in uri */
		user.s="";
		user.len=0;
	}else{
		user=msg->parsed_uri.user;
		c=user.s[user.len];
		user.s[user.len]=0;
	}
	se=(struct subst_expr*)subst;
	result=subst_str(user.s, msg, se, &nmatches);/* pkg malloc'ed result */
	if (c)	user.s[user.len]=c;
	if (result == NULL) {
		if (nmatches<0)
			LM_ERR("subst_user(): subst_str() failed\n");
		return -1;
	}
	/* result->s[result->len] = '\0';  --subst_str returns 0-term strings */
	memset(&act, 0, sizeof(act)); /* be on the safe side */
	act.type = SET_USER_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = result->s;
	init_run_actions_ctx(&h);
	rval = do_action(&h, &act, msg);
	pkg_free(result->s);
	pkg_free(result);
	return rval;
}


/* sed-perl style re: s/regular expression/replacement/flags */
static int subst_body_f(struct sip_msg* msg, char*  subst, char* ignored)
{
	struct lump* l;
	struct replace_lst* lst;
	struct replace_lst* rpl;
	char* begin;
	struct subst_expr* se;
	int off;
	int ret;
	int nmatches;
	str body;

	body.s = get_body(msg);
	if (body.s==0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s-msg->buf);
	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}
	
	se=(struct subst_expr*)subst;
	begin=body.s;
	
	off=begin-msg->buf;
	ret=-1;
	if ((lst=subst_run(se, begin, msg, &nmatches))==0)
		goto error; /* not found */
	for (rpl=lst; rpl; rpl=rpl->next){
		LM_DBG("%s replacing at offset %d [%.*s] with [%.*s]\n",
				exports.name, rpl->offset+off,
				rpl->size, rpl->offset+off+msg->buf,
				rpl->rpl.len, rpl->rpl.s);
		if ((l=del_lump(msg, rpl->offset+off, rpl->size, 0))==0)
			goto error;
		/* hack to avoid re-copying rpl, possible because both 
		 * replace_lst & lumps use pkg_malloc */
		if (insert_new_lump_after(l, rpl->rpl.s, rpl->rpl.len, 0)==0){
			LM_ERR("%s could not insert new lump\n",
					exports.name);
			goto error;
		}
		/* hack continued: set rpl.s to 0 so that replace_lst_free will
		 * not free it */
		rpl->rpl.s=0;
		rpl->rpl.len=0;
	}
	ret=1;
error:
	LM_DBG("lst was %p\n", lst);
	if (lst) replace_lst_free(lst);
	if (nmatches<0)
		LM_ERR("%s subst_run failed\n", exports.name);
	return ret;
}


static inline int find_line_start(char *text, unsigned int text_len,
				  char **buf, unsigned int *buf_len)
{
    char *ch, *start;
    unsigned int len;

    start = *buf;
    len = *buf_len;

    while (text_len <= len) {
	if (strncmp(text, start, text_len) == 0) {
	    *buf = start;
	    *buf_len = len;
	    return 1;
	}
	if ((ch = memchr(start, 13, len - 1))) {
	    if (*(ch + 1) != 10) {
		LM_ERR("No LF after CR\n");
		return 0;
	    }
	    len = len - (ch - start + 2);
	    start = ch + 2;
	} else {
	    LM_ERR("No CRLF found\n");
	    return 0;
	}
    }
    return 0;
}

/**
 * return:
 *  1: multipart
 */
static int check_multipart(struct sip_msg *msg)
{
	int mime;

	/* the function search for and parses the Content-Type hdr */
	mime = parse_content_type_hdr (msg);
	if(mime<0) {
		LM_ERR("failed to extract content type hdr\n");
		return -1;
	}
	if(mime!=MIMETYPE(MULTIPART,MIXED)) return 0;
	return 1;
}

/* Filters multipart/mixed body by leaving out everything else except
 * first body part of given content type. */
static int filter_body_f(struct sip_msg* msg, char* _content_type,
			 char* ignored)
{
	char *start;
	unsigned int len;
	str *content_type, body;
	str boundary = {0,0};

	body.s = get_body(msg);
	if (body.s == 0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len - (int)(body.s - msg->buf);
	if (body.len == 0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	if(check_multipart(msg)!=1) {
		LM_WARN("body not multipart\n");
		return -1;
	}
	if(get_boundary(msg, &boundary)!=0) {
		return -1;
	}
	content_type = (str *)_content_type;
	start = body.s;
	len = body.len;

	while (find_line_start("Content-Type: ", 14, &start, &len))
	{
		start = start + 14;
		len = len - 14;
		LM_DBG("line: [%.*s]\n", len, start);
		if (len > content_type->len + 2) {
			if (strncasecmp(start, content_type->s, content_type->len)== 0)
			{
				LM_DBG("found content type %.*s\n",
					content_type->len, content_type->s);
				start = start + content_type->len;
				if ((*start != 13) || (*(start + 1) != 10))
				{
					LM_ERR("no CRLF found after content type\n");
					goto err;
				}
				start = start + 2;
				len = len - content_type->len - 2;
				while ((len > 0) && ((*start == 13) || (*start == 10)))
				{
					len = len - 1;
					start = start + 1;
				}
				if (del_lump(msg, body.s - msg->buf, start - body.s, 0)== 0)
				{
					LM_ERR("deleting lump <%.*s> failed\n",
					(int)(start - body.s), body.s);
					goto err;
				}
				if (find_line_start(boundary.s, boundary.len, &start,
					&len))
				{
					if (del_lump(msg, start - msg->buf, len, 0) == 0)
					{
						LM_ERR("deleting lump <%.*s> failed\n", len, start);
						goto err;
					}
					else
					{
						pkg_free(boundary.s);
						return 1;
					}
				}
				else
				{
					LM_ERR("boundary not found after content\n");
					goto err;
				}
			}
		} else {
			goto err;
		}
	}
 err:
	if(boundary.s) pkg_free(boundary.s);
	return -1;
}


int remove_hf_f(struct sip_msg* msg, char* str_hf, char* foo)
{
	struct hdr_field *hf;
	struct lump* l;
	int cnt;
	gparam_p gp;

	gp = (gparam_p)str_hf;
	cnt=0;

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);
	for (hf=msg->headers; hf; hf=hf->next) {
		/* for well known header names str_hf->s will be set to NULL 
		   during parsing of kamailio.cfg and str_hf->len contains 
		   the header type */
		if(gp->type==GPARAM_TYPE_INT)
		{
			if (gp->v.i!=hf->type)
				continue;
		} else {
			if (hf->name.len!=gp->v.str.len)
				continue;
			if (cmp_hdrname_str(&hf->name, &gp->v.str)!=0)
				continue;
		}
		l=del_lump(msg, hf->name.s-msg->buf, hf->len, 0);
		if (l==0) {
			LM_ERR("no memory\n");
			return -1;
		}
		cnt++;
	}
	return cnt==0 ? -1 : 1;
}

static int remove_hf_re_f(struct sip_msg* msg, char* key, char* foo)
{
	struct hdr_field *hf;
	struct lump* l;
	int cnt;
	regex_t *re;
	char c;
	regmatch_t pmatch;

	re = (regex_t*)key;
	cnt=0;

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);
	for (hf=msg->headers; hf; hf=hf->next)
	{
		c = hf->name.s[hf->name.len];
		hf->name.s[hf->name.len] = '\0';
		if (regexec(re, hf->name.s, 1, &pmatch, 0)!=0)
		{
			hf->name.s[hf->name.len] = c;
			continue;
		}
		hf->name.s[hf->name.len] = c;
		l=del_lump(msg, hf->name.s-msg->buf, hf->len, 0);
		if (l==0)
		{
			LM_ERR("cannot remove header\n");
			return -1;
		}
		cnt++;
	}

	return cnt==0 ? -1 : 1;
}

static int is_present_hf_f(struct sip_msg* msg, char* str_hf, char* foo)
{
	struct hdr_field *hf;
	gparam_p gp;

	gp = (gparam_p)str_hf;

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);
	for (hf=msg->headers; hf; hf=hf->next) {
		if(gp->type==GPARAM_TYPE_INT)
		{
			if (gp->v.i!=hf->type)
				continue;
		} else {
			if (hf->name.len!=gp->v.str.len)
				continue;
			if (cmp_hdrname_str(&hf->name,&gp->v.str)!=0)
				continue;
		}
		return 1;
	}
	return -1;
}

static int is_present_hf_re_f(struct sip_msg* msg, char* key, char* foo)
{
	struct hdr_field *hf;
	regex_t *re;
	regmatch_t pmatch;
	char c;

	re = (regex_t*)key;

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);
	for (hf=msg->headers; hf; hf=hf->next)
	{
		c = hf->name.s[hf->name.len];
		hf->name.s[hf->name.len] = '\0';
		if (regexec(re, hf->name.s, 1, &pmatch, 0)!=0)
		{
			hf->name.s[hf->name.len] = c;
			continue;
		}
		hf->name.s[hf->name.len] = c;
		return 1;
	}

	return -1;
}


static int fixup_substre(void** param, int param_no)
{
	struct subst_expr* se;
	str subst;

	LM_DBG("%s module -- fixing %s\n", exports.name, (char*)(*param));
	if (param_no!=1) return 0;
	subst.s=*param;
	subst.len=strlen(*param);
	se=subst_parser(&subst);
	if (se==0){
		LM_ERR("%s: bad subst. re %s\n", exports.name, 
				(char*)*param);
		return E_BAD_RE;
	}
	/* don't free string -- needed for specifiers */
	/* pkg_free(*param); */
	/* replace it with the compiled subst. re */
	*param=se;
	return 0;
}


static int append_time_f(struct sip_msg* msg, char* p1, char *p2)
{


	size_t len;
	char time_str[MAX_TIME];
	time_t now;
	struct tm *bd_time;

	now=time(0);

	bd_time=gmtime(&now);
	if (bd_time==NULL) {
		LM_ERR("gmtime failed\n");
		return -1;
	}

	len=strftime(time_str, MAX_TIME, TIME_FORMAT, bd_time);
	if (len>MAX_TIME-2 || len==0) {
		LM_ERR("unexpected time length\n");
		return -1;
	}

	time_str[len]='\r';
	time_str[len+1]='\n';


	if (add_lump_rpl(msg, time_str, len+2, LUMP_RPL_HDR)==0)
	{
		LM_ERR("unable to add lump\n");
		return -1;
	}

	return 1;
}

static int append_time_request_f(struct sip_msg* msg, char* p1, char *p2)
{
	str time_str = {0, 0};
	time_t now;
	struct tm *bd_time;
	struct hdr_field *hf = msg->headers;
	struct lump *anchor = anchor_lump(msg, hf->name.s + hf->len - msg->buf, 0, 0);

	now=time(0);

	bd_time=gmtime(&now);
	if (bd_time==NULL) {
		LM_ERR("gmtime failed\n");
		goto error;
	}

	time_str.s = pkg_malloc(MAX_TIME);
	time_str.len=strftime(time_str.s, MAX_TIME, TIME_FORMAT, bd_time);
	if (time_str.len>MAX_TIME-2 || time_str.len==0) {
		LM_ERR("unexpected time length\n");
		goto error;
	}

	time_str.s[time_str.len++]='\r';
	time_str.s[time_str.len++]='\n';

	if (anchor == NULL)
	{
		LM_ERR("Problem with getting anchor");
		goto error;
	}

	if (insert_new_lump_after(anchor, time_str.s, time_str.len, 0) == 0)
	{
		LM_ERR("unable to add lump\n");
		goto error;
	}

	return 1;
error:
	if (time_str.s != NULL)
		pkg_free(time_str.s);

	return -1;
}

static int set_body_f(struct sip_msg* msg, char* p1, char* p2)
{
	struct lump *anchor;
	char* buf;
	int len;
	char* value_s;
	int value_len;
	str body = {0,0};
	str nb = {0,0};
	str nc = {0,0};

	if(p1==0 || p2==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)p1, &nb)!=0)
	{
		LM_ERR("unable to get p1\n");
		return -1;
	}
	if(nb.s==NULL || nb.len == 0)
	{
		LM_ERR("invalid body parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)p2, &nc)!=0)
	{
		LM_ERR("unable to get p2\n");
		return -1;
	}
	if(nc.s==NULL || nc.len == 0)
	{
		LM_ERR("invalid content-type parameter\n");
		return -1;
	}

	body.len = 0;
	body.s = get_body(msg);
	if (body.s==0)
	{
		LM_ERR("malformed sip message\n");
		return -1;
	}

	del_nonshm_lump( &(msg->body_lumps) );
	msg->body_lumps = NULL;

	if (msg->content_length) 
	{
		body.len = get_content_length( msg );
		if(body.len > 0)
		{
			if(body.s+body.len>msg->buf+msg->len)
			{
				LM_ERR("invalid content length: %d\n", body.len);
				return -1;
			}
			if(del_lump(msg, body.s - msg->buf, body.len, 0) == 0)
			{
				LM_ERR("cannot delete existing body");
				return -1;
			}
		}
	}

	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);

	if (anchor == 0)
	{
		LM_ERR("failed to get anchor\n");
		return -1;
	} 

	if (msg->content_length==0)
	{
		/* need to add Content-Length */
		len = nb.len;
		value_s=int2str(len, &value_len);
		LM_DBG("content-length: %d (%s)\n", value_len, value_s);

		len=CONTENT_LENGTH_LEN+value_len+CRLF_LEN;
		buf=pkg_malloc(sizeof(char)*(len));

		if (buf==0)
		{
			LM_ERR("out of pkg memory\n");
			return -1;
		}

		memcpy(buf, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		memcpy(buf+CONTENT_LENGTH_LEN, value_s, value_len);
		memcpy(buf+CONTENT_LENGTH_LEN+value_len, CRLF, CRLF_LEN);
		if (insert_new_lump_after(anchor, buf, len, 0) == 0)
		{
			LM_ERR("failed to insert content-length lump\n");
			pkg_free(buf);
			return -1;
		}
	}

	/* add content-type */
	if(msg->content_type==NULL || msg->content_type->body.len!=nc.len
			|| strncmp(msg->content_type->body.s, nc.s, nc.len)!=0)
	{
		if(msg->content_type!=NULL)
			if(del_lump(msg, msg->content_type->name.s-msg->buf,
						msg->content_type->len, 0) == 0)
			{
				LM_ERR("failed to delete content type\n");
				return -1;
			}
		value_len = nc.len;
		len=sizeof("Content-Type: ") - 1 + value_len + CRLF_LEN;
		buf=pkg_malloc(sizeof(char)*(len));

		if (buf==0)
		{
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(buf, "Content-Type: ", sizeof("Content-Type: ") - 1);
		memcpy(buf+sizeof("Content-Type: ") - 1, nc.s, value_len);
		memcpy(buf+sizeof("Content-Type: ") - 1 + value_len, CRLF, CRLF_LEN);
		if (insert_new_lump_after(anchor, buf, len, 0) == 0)
		{
			LM_ERR("failed to insert content-type lump\n");
			pkg_free(buf);
			return -1;
		}
	}	
	anchor = anchor_lump(msg, body.s - msg->buf, 0, 0);

	if (anchor == 0)
	{
		LM_ERR("failed to get body anchor\n");
		return -1;
	} 

	buf=pkg_malloc(sizeof(char)*(nb.len));
	if (buf==0)
	{
		LM_ERR("out of pkg memory\n");
		return -1;
	}
	memcpy(buf, nb.s, nb.len);
	if (insert_new_lump_after(anchor, buf, nb.len, 0) == 0)
	{
		LM_ERR("failed to insert body lump\n");
		pkg_free(buf);
		return -1;
	}
	LM_DBG("new body: [%.*s]", nb.len, nb.s);
	return 1;
}

static int set_rpl_body_f(struct sip_msg* msg, char* p1, char* p2)
{
	char* buf;
	int len;
	int value_len;
	str nb = {0,0};
	str nc = {0,0};

	if(p1==0 || p2==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)p1, &nb)!=0)
	{
		LM_ERR("unable to get p1\n");
		return -1;
	}
	if(nb.s==NULL || nb.len == 0)
	{
		LM_ERR("invalid body parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)p2, &nc)!=0)
	{
		LM_ERR("unable to get p2\n");
		return -1;
	}
	if(nc.s==NULL || nc.len == 0)
	{
		LM_ERR("invalid content-type parameter\n");
		return -1;
	}

	/* add content-type */
	value_len = nc.len;
	len=sizeof("Content-Type: ") - 1 + value_len + CRLF_LEN;
	buf=pkg_malloc(sizeof(char)*(len));

	if (buf==0)
	{
		LM_ERR("out of pkg memory\n");
		return -1;
	}
	memcpy(buf, "Content-Type: ", sizeof("Content-Type: ") - 1);
	memcpy(buf+sizeof("Content-Type: ") - 1, nc.s, value_len);
	memcpy(buf+sizeof("Content-Type: ") - 1 + value_len, CRLF, CRLF_LEN);
	if (add_lump_rpl(msg, buf, len, LUMP_RPL_HDR) == 0)
	{
		LM_ERR("failed to insert content-type lump\n");
		pkg_free(buf);
		return -1;
	}
	pkg_free(buf);

	if (add_lump_rpl( msg, nb.s, nb.len, LUMP_RPL_BODY)==0) {
		LM_ERR("cannot add body lump\n");
		return -1;
	}
		
	return 1;
}

static str* generate_boundary(str *txt, str *content_type,
	str *content_disposition, str *delimiter, unsigned int initial)
{
	unsigned int i = 0;
	str cth = {"Content-Type: ", 14};
	str cdh = {"Content-Disposition: ", 21};
	str* n;
	unsigned int flag = 0;

	if(txt==NULL||txt->len==0
		||content_type==NULL||content_type->len==0
		||delimiter==NULL||delimiter->len==0)
	{
		LM_ERR("invalid parameters\n");
		return NULL;
	}
	if (delimiter->s[0] == '-') {
		LM_ERR("delimiter with initial '-'. Invalid parameter.\n");
		return NULL;
	}
	n = pkg_malloc(sizeof(str));
	if(n==NULL)
	{
		LM_ERR("out of pkg memory\n");
		return NULL;
	}
	n->len = delimiter->len + 2 + CRLF_LEN;
	if(initial) n->len = 2*n->len;
	if(strncmp("\r\n\r\n", txt->s+txt->len-4,4)!=0)
	{
		n->len = n->len + CRLF_LEN;
		flag = 1;
		LM_DBG("adding final CRLF+CRLF\n");
	}
	n->len=n->len + cth.len + content_type->len + 2*CRLF_LEN;
	if(content_disposition->len>0)
	{
		n->len = n->len + cdh.len + content_disposition->len + CRLF_LEN;
	}
	n->len = n->len + txt->len;
	n->s = pkg_malloc(sizeof(char)*(n->len));
	if(n->s==0)
	{
		LM_ERR("out of pkg memory\n");
		pkg_free(n);
		return NULL;
	}
	memset(n->s, 0, sizeof(char)*n->len);
	if(initial)
	{
		memcpy(n->s, "--", 2); i=2;
		memcpy(n->s+i, delimiter->s, delimiter->len); i=i+delimiter->len;
		memcpy(n->s+i, CRLF, CRLF_LEN); i=i+CRLF_LEN;
	}

	memcpy(n->s+i, cth.s, cth.len); i=i+cth.len;
	memcpy(n->s+i, content_type->s, content_type->len); i=i+content_type->len;
	memcpy(n->s+i, CRLF, CRLF_LEN); i=i+CRLF_LEN;

	if(content_disposition->len>0)
	{
		memcpy(n->s+i, cdh.s, cdh.len); i=i+cdh.len;
		memcpy(n->s+i, content_disposition->s, content_disposition->len);
		i=i+content_disposition->len;
		memcpy(n->s+i, CRLF, CRLF_LEN); i=i+CRLF_LEN;
	}
	memcpy(n->s+i, CRLF, CRLF_LEN); i=i+CRLF_LEN;

	memcpy(n->s+i, txt->s, txt->len); i=i+txt->len;
	if(flag) { memcpy(n->s+i, CRLF, CRLF_LEN); i=i+CRLF_LEN; }

	memcpy(n->s+i, "--", 2); i=i+2;
	memcpy(n->s+i, delimiter->s, delimiter->len); i=i+delimiter->len;
	memcpy(n->s+i, CRLF, CRLF_LEN); i=i+CRLF_LEN;

	if(i!=n->len)
	{
		LM_ERR("out of bounds\n");
	}
	return n;
}

int set_multibody_helper(struct sip_msg* msg, char* p1, char* p2, char* p3)
{
	struct lump *anchor;
	char* buf = NULL;
	int len;
	char* value_s;
	int value_len;
	str body = {0,0};
	str nb = {0,0};
	str oc = {0,0};
	str cd = {0,0};
	str delimiter = {0,0};
	str default_delimiter = {"unique-boundary-1", 17};
	str nc = {0,0};
	str cth = {"Content-Type: ", 14};
	str* nbb = NULL;
	unsigned int convert = 0;
	fparam_t header;
	header.orig = NULL;
	header.type = FPARAM_STR;
	header.v.str.s = "Mime-Version: 1.0\r\n";
	header.v.str.len = 19;

	if(p3==0)
	{
		delimiter.s = default_delimiter.s;
		delimiter.len = default_delimiter.len;
	}
	else
	{
		if(fixup_get_svalue(msg, (gparam_p)p3, &delimiter)!=0)
		{
			LM_ERR("unable to get p3\n");
			return -1;
		}
		if(delimiter.s==NULL || delimiter.len == 0)
		{
			LM_ERR("invalid boundary parameter\n");
			return -1;
		}
	}
	LM_DBG("delimiter<%d>:[%.*s]\n", delimiter.len, delimiter.len, delimiter.s);
	if(p1==0 || p2==0)
	{
		if(check_multipart(msg)==1) {
			LM_WARN("body is already multipart. Do nothing\n");
			return -1;
		}
		convert = 1;
	}
	else
	{
		if(fixup_get_svalue(msg, (gparam_p)p1, &nb)!=0)
		{
			LM_ERR("unable to get p1\n");
			return -1;
		}
		if(nb.s==NULL || nb.len == 0)
		{
			LM_ERR("invalid body parameter\n");
			return -1;
		}
		if(fixup_get_svalue(msg, (gparam_p)p2, &oc)!=0)
		{
			LM_ERR("unable to get p2\n");
			return -1;
		}
		if(oc.s==NULL || oc.len==0)
		{
			LM_ERR("invalid content-type parameter\n");
			return -1;
		}
		if(check_multipart(msg)==1) {
			convert = -1;
		}
	}

	body.len = 0;
	body.s = get_body(msg);
	if(body.s==0)
	{
		LM_ERR("malformed sip message\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s-msg->buf);

	del_nonshm_lump( &(msg->body_lumps) );
	msg->body_lumps = NULL;

	if(msg->content_length)
	{
		if(body.len > 0)
		{
			if(body.s+body.len>msg->buf+msg->len)
			{
				LM_ERR("invalid content length: %d\n", body.len);
				return -1;
			}
			if(convert==1)
			{
				/* need to copy body */
				nb.s=pkg_malloc(sizeof(char)*body.len);
				if (nb.s==0)
				{
					LM_ERR("out of pkg memory\n");
					return -1;
				}
				memcpy(nb.s, body.s, body.len);
				nb.len = body.len;
				if(msg->content_type!=NULL && msg->content_type->body.s!=NULL)
				{
					oc.len = msg->content_type->body.len;
					oc.s=pkg_malloc(sizeof(char)*oc.len);
					if (oc.s==0)
					{
						LM_ERR("out of pkg memory\n");
						goto error;
					}
					memcpy(oc.s, msg->content_type->body.s, oc.len);
				}
			}
			if(del_lump(msg, body.s-msg->buf, body.len, 0) == 0)
			{
				LM_ERR("cannot delete existing body");
				goto error;
			}
		}
	}

	anchor = anchor_lump(msg, msg->unparsed-msg->buf, 0, 0);
	if(anchor==0)
	{
		LM_ERR("failed to get anchor\n");
		goto error;
	}

	/* get initial boundary */
	nbb = generate_boundary(&nb, &oc, &cd, &delimiter, 1);
	if(nbb==NULL)
	{
		LM_ERR("couldn't create initial boundary\n");
		goto error;
	}

	if(msg->content_length==0)
	{
		/* need to add Content-Length */
		len = nbb->len;
		value_s=int2str(len, &value_len);

		len=CONTENT_LENGTH_LEN+value_len+CRLF_LEN;
		buf=pkg_malloc(sizeof(char)*len);

		if (buf==0)
		{
			LM_ERR("out of pkg memory\n");
			goto error;
		}

		memcpy(buf, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		memcpy(buf+CONTENT_LENGTH_LEN, value_s, value_len);
		memcpy(buf+CONTENT_LENGTH_LEN+value_len, CRLF, CRLF_LEN);
		if (insert_new_lump_after(anchor, buf, len, 0) == 0)
		{
			LM_ERR("failed to insert content-length lump\n");
			goto error;
		}
		buf = NULL;
	}

	if(convert!=-1)
	{
		/* set new content type with delimiter */
		nc.len = delimiter.len + 27;
		nc.s = pkg_malloc(sizeof(char)*nc.len);
		memcpy(nc.s, "multipart/mixed;boundary=\"", 26);
		memcpy(nc.s+26, delimiter.s, delimiter.len);
		nc.s[26+delimiter.len] = '"';
		LM_DBG("content-type<%d>:[%.*s]\n", nc.len, nc.len, nc.s);
		/* add content-type */
		if(msg->content_type==NULL || msg->content_type->body.len!=nc.len
				|| strncmp(msg->content_type->body.s, nc.s, nc.len)!=0)
		{
			if(msg->content_type!=NULL)
				if(del_lump(msg, msg->content_type->name.s-msg->buf,
							msg->content_type->len, 0) == 0)
				{
					LM_ERR("failed to delete content type\n");
					goto error;
				}
			value_len = nc.len;
			len = cth.len + value_len + CRLF_LEN;
			buf = pkg_malloc(sizeof(char)*len);

			if(buf==0)
			{
				LM_ERR("out of pkg memory\n");
				goto error;
			}
			memcpy(buf, cth.s, cth.len);
			memcpy(buf + cth.len, nc.s, value_len);
			memcpy(buf + cth.len + value_len, CRLF, CRLF_LEN);
			if (insert_new_lump_after(anchor, buf, len, 0) == 0)
			{
				LM_ERR("failed to insert content-type lump\n");
				goto error;
			}
			buf = NULL;
		}
		/* add Mime-Version header */
		if(add_hf_helper(msg, 0, 0, &header, 0, 0)<0)
		{
			LM_ERR("failed to add Mime-Version header\n");
			goto error;
		}
	}
	anchor = anchor_lump(msg, body.s - msg->buf, 0, 0);
	if(anchor==0)
	{
		LM_ERR("failed to get body anchor\n");
		goto error;
	}

	if(insert_new_lump_after(anchor, nbb->s, nbb->len, 0)==0)
	{
		LM_ERR("failed to insert body lump\n");
		goto error;
	}
	pkg_free(nbb);
	if(nc.s!=NULL) pkg_free(nc.s);
	LM_DBG("set flag FL_BODY_MULTIPART\n");
	msg->msg_flags |= FL_BODY_MULTIPART;
	return 1;

error:
	if(nbb!=NULL) { pkg_free(nbb->s); pkg_free(nbb); }
	if(nc.s!=NULL) pkg_free(nc.s);
	if(buf!=NULL) pkg_free(buf);
	if(convert && nb.s!=NULL) pkg_free(nb.s);
	if(convert && oc.s!=NULL) pkg_free(oc.s);
	return -1;
}

static int set_multibody_0(struct sip_msg* msg, char* p1, char* p2, char* p3)
{
	return set_multibody_helper(msg, NULL, NULL, NULL);
}

static int set_multibody_1(struct sip_msg* msg, char* p1, char* p2, char* p3)
{
	return set_multibody_helper(msg, NULL, NULL, p1);
}

static int set_multibody_2(struct sip_msg* msg, char* p1, char* p2, char* p3)
{
	return set_multibody_helper(msg, p1, p2, NULL);
}

static int set_multibody_3(struct sip_msg* msg, char* p1, char* p2, char *p3)
{
	return set_multibody_helper(msg, p1, p2, p3);
}

int append_multibody_helper(struct sip_msg* msg, char* p1, char* p2, char* p3)
{
	struct lump *l;
	int off;
	str body = {0,0};
	str nc = {0,0};
	str cd = {0,0};
	str txt = {0,0};
	str* nbb = NULL;
	str delimiter = {0,0};

	if(p1==0 || p2==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)p1, &txt)!=0)
	{
		LM_ERR("unable to get p1\n");
		return -1;
	}
	if(txt.s==NULL || txt.len==0)
	{
		LM_ERR("invalid body parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)p2, &nc)!=0)
	{
		LM_ERR("unable to get p2\n");
		return -1;
	}
	if(nc.s==NULL || nc.len==0)
	{
		LM_ERR("invalid content-type parameter\n");
		return -1;
	}
	if(p3!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)p3, &cd)!=0)
		{
			LM_ERR("unable to get p3\n");
			return -1;
		}
	}

	body.s = get_body(msg);
	if(body.s==0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s-msg->buf);
	if(body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	off=body.s-msg->buf;
	if((l=anchor_lump(msg, off+body.len, 0, 0))==0)
	{
		LM_ERR("WTF\n");
		return -1;
	}
	/* get delimiter no initial -- */
	if(get_mixed_part_delimiter(&msg->content_type->body, &delimiter) < 0) {
		LM_ERR("Cannot get boundary. Is body multipart?\n");
		return -1;
	}
	nbb = generate_boundary(&txt, &nc, &cd, &delimiter, 0);
	if(nbb==NULL)
	{
		LM_ERR("couldn't create initial boundary\n");
		return -1;
	}
	if(insert_new_lump_after(l, nbb->s, nbb->len, 0)==0){
		LM_ERR("could not insert new lump\n");
		pkg_free(nbb->s); pkg_free(nbb);
		return -1;
	}
	pkg_free(nbb);
	if(!(msg->msg_flags&FL_BODY_MULTIPART))
	{
		LM_DBG("set flag FL_BODY_MULTIPART\n");
		msg->msg_flags |= FL_BODY_MULTIPART;
	}
	return 1;
}

static int append_multibody_2(struct sip_msg* msg, char* p1, char* p2)
{
	return append_multibody_helper(msg, p1, p2, NULL);
}

static int append_multibody_3(struct sip_msg* msg, char* p1, char* p2, char *p3)
{
	return append_multibody_helper(msg, p1, p2, p3);
}

static int fixup_multibody_f(void** param, int param_no)
{
	int ret;
	fparam_t* fp;

	if(param_no<=3){
		if((ret=fix_param_types(FPARAM_PVE, param))<0){
			ERR("Cannot convert function parameter %d to spve \n",
					param_no);
			return E_UNSPEC;
		} else {
			fp=(fparam_t*)*param;
			if((ret==0) && (fp->v.pve->spec==0
						|| fp->v.pve->spec->getf==0)){
				fparam_free_restore(param);
				return fix_param_types(FPARAM_STR, param);
			} else if(ret==1)
				return fix_param_types(FPARAM_STR, param);
			return ret;
		}
	} else {
		LM_ERR("wrong number of parameters\n");
		return E_UNSPEC;
	}
}

static inline int get_line(char *s, int len)
{
	char *ch;

	if ((ch = memchr(s, 13, len))) {
		if (*(ch + 1) != 10) {
			LM_ERR("No LF after CR\n");
			return 0;
		}
		return ch - s + 2;
	} else {
		LM_ERR("No CRLF found\n");
		return len;
	}
	return 0;
}

static int remove_multibody_f(struct sip_msg* msg, char* p1)
{
	char *start, *end;
	unsigned int len, t;
	str content_type, body;
	str boundary = {0,0};

	if(p1==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)p1, &content_type)!=0)
	{
		LM_ERR("unable to get p1\n");
		return -1;
	}

	body.s = get_body(msg);
	if (body.s == 0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len - (int)(body.s - msg->buf);
	if (body.len == 0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	if(get_boundary(msg, &boundary)!=0) {
		LM_ERR("Cannot get boundary. Is body multipart?\n");
		return -1;
	}

	start = body.s;
	len = body.len;

	while (find_line_start("Content-Type: ", 14, &start, &len))
	{
		end = start + 14;
		len = len - 14;
		if (len > (content_type.len + 2)) {
			if (strncasecmp(end, content_type.s, content_type.len)== 0)
			{
				LM_DBG("found content type %.*s\n",
					content_type.len, content_type.s);
				end = end + content_type.len;
				if ((*end != 13) || (*(end + 1) != 10))
				{
					LM_ERR("no CRLF found after content type\n");
					goto err;
				}
				end = end + 2;
				len = len - content_type.len - 2;
				if (find_line_start(boundary.s, boundary.len, &end,
					&len))
				{
					LM_DBG("found boundary %.*s\n", boundary.len, boundary.s);
					end = end + boundary.len;
					len = len - boundary.len;
					if (!(t = get_line(end, len))) goto err;
					end += t; len = end-start;
					if (del_lump(msg, start - msg->buf, len, 0) == 0)
					{
						LM_ERR("deleting lump <%.*s> failed\n", len, start);
						goto err;
					}
					pkg_free(boundary.s);
					if(!(msg->msg_flags&FL_BODY_MULTIPART))
					{
						LM_DBG("set flag FL_BODY_MULTIPART\n");
						msg->msg_flags |= FL_BODY_MULTIPART;
					}
					return 1;
				}
				LM_ERR("boundary not found after content\n");
				goto err;
			}
			start = end;
		}
		else goto err;
	}
 err:
	pkg_free(boundary.s);
	return -1;
}

static int append_to_reply_f(struct sip_msg* msg, char* key, char* str0)
{
	str s0;

	if(key==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)key, &s0)!=0)
	{
		LM_ERR("cannot print the format\n");
		return -1;
	}
 
	if ( add_lump_rpl( msg, s0.s, s0.len, LUMP_RPL_HDR)==0 )
	{
		LM_ERR("unable to add lump_rl\n");
		return -1;
	}

	return 1;
}


/* add str1 to end of header or str1.r-uri.str2 */

int add_hf_helper(struct sip_msg* msg, str *str1, str *str2,
		gparam_p hfval, int mode, gparam_p hfanc)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *s;
	int len;
	str s0;

	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		return -1;
	}
	
	hf = 0;
	if(hfanc!=NULL) {
		for (hf=msg->headers; hf; hf=hf->next) {
			if(hfanc->type==GPARAM_TYPE_INT)
			{
				if (hfanc->v.i!=hf->type)
					continue;
			} else {
				if (hf->name.len!=hfanc->v.str.len)
					continue;
				if (cmp_hdrname_str(&hf->name,&hfanc->v.str)!=0)
					continue;
			}
			break;
		}
	}

	if(mode == 0) { /* append */
		if(hf==0) { /* after last header */
			anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
		} else { /* after hf */
			anchor = anchor_lump(msg, hf->name.s + hf->len - msg->buf, 0, 0);
		}
	} else { /* insert */
		if(hf==0) { /* before first header */
			anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0, 0);
		} else { /* before hf */
			anchor = anchor_lump(msg, hf->name.s - msg->buf, 0, 0);
		}
	}

	if(anchor == 0) {
		LM_ERR("can't get anchor\n");
		return -1;
	}

	if(str1) {
		s0 = *str1;
	} else {
		if(hfval) {
			if(fixup_get_svalue(msg, hfval, &s0)!=0)
			{
				LM_ERR("cannot print the format\n");
				return -1;
			}
		} else {
			s0.len = 0;
			s0.s   = 0;
		}
	}
		
	len=s0.len;
	if (str2) len+= str2->len + REQ_LINE(msg).uri.len;

	if(len==0) {
		LM_INFO("nothing to add\n");
		return -1;
	}

	s  = (char*)pkg_malloc(len);
	if (!s) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}

	if (likely(s0.len>0)){
		memcpy(s, s0.s, s0.len);
	}

	if (str2) {
		memcpy(s+s0.len, REQ_LINE(msg).uri.s, REQ_LINE(msg).uri.len);
		memcpy(s+s0.len+REQ_LINE(msg).uri.len, str2->s, str2->len );
	}

	if (insert_new_lump_before(anchor, s, len, 0) == 0) {
		LM_ERR("can't insert lump\n");
		pkg_free(s);
		return -1;
	}
	return 1;
}

static int append_hf_1(struct sip_msg *msg, char *str1, char *str2 )
{
	return add_hf_helper(msg, 0, 0, (gparam_p)str1, 0, 0);
}

static int append_hf_2(struct sip_msg *msg, char *str1, char *str2 )
{
	return add_hf_helper(msg, 0, 0, (gparam_p)str1, 0,
			(gparam_p)str2);
}

static int insert_hf_1(struct sip_msg *msg, char *str1, char *str2 )
{
	return add_hf_helper(msg, 0, 0, (gparam_p)str1, 1, 0);
}

static int insert_hf_2(struct sip_msg *msg, char *str1, char *str2 )
{
	return add_hf_helper(msg, 0, 0, (gparam_p)str1, 1, 
			(gparam_p)str2);
}

static int append_urihf(struct sip_msg *msg, char *str1, char *str2)
{
	return add_hf_helper(msg, (str*)str1, (str*)str2, 0, 0, 0);
}

static int is_method_f(struct sip_msg *msg, char *meth, char *str2 )
{
	str *m;

	m = (str*)meth;
	if(msg->first_line.type==SIP_REQUEST)
	{
		if(m->s==0)
			return (msg->first_line.u.request.method_value&m->len)?1:-1;
		else
			return (msg->first_line.u.request.method_value==METHOD_OTHER
					&& msg->first_line.u.request.method.len==m->len
					&& (strncasecmp(msg->first_line.u.request.method.s, m->s,
					m->len)==0))?1:-1;
	}
	if(parse_headers(msg, HDR_CSEQ_F, 0)!=0 || msg->cseq==NULL)
	{
		LM_ERR("cannot parse cseq header\n");
		return -1; /* should it be 0 ?!?! */
	}
	if(m->s==0)
		return (get_cseq(msg)->method_id&m->len)?1:-1;
	else
		return (get_cseq(msg)->method_id==METHOD_OTHER
				&& get_cseq(msg)->method.len==m->len
				&& (strncasecmp(get_cseq(msg)->method.s, m->s,
				m->len)==0))?1:-1;
}


/*
 * Convert char* header_name to str* parameter
 */
static int hname_fixup(void** param, int param_no)
{
	char c;
	struct hdr_field hdr;
	gparam_p gp = NULL;
	
	gp = (gparam_p)pkg_malloc(sizeof(gparam_t));
	if(gp == NULL)
	{
		LM_ERR("no more memory\n");
		return E_UNSPEC;
	}
	memset(gp, 0, sizeof(gparam_t));

	gp->v.str.s = (char*)*param;
	gp->v.str.len = strlen(gp->v.str.s);
	if(gp->v.str.len==0)
	{
		LM_ERR("empty header name parameter\n");
		pkg_free(gp);
		return E_UNSPEC;
	}
	
	c = gp->v.str.s[gp->v.str.len];
	gp->v.str.s[gp->v.str.len] = ':';
	gp->v.str.len++;
	
	if (parse_hname2_short(gp->v.str.s, gp->v.str.s + gp->v.str.len, &hdr)==0)
	{
		LM_ERR("error parsing header name\n");
		pkg_free(gp);
		return E_UNSPEC;
	}
	
	gp->v.str.len--;
	gp->v.str.s[gp->v.str.len] = c;

	if (hdr.type!=HDR_OTHER_T && hdr.type!=HDR_ERROR_T)
	{
		LM_DBG("using hdr type (%d) instead of <%.*s>\n",
				hdr.type, gp->v.str.len, gp->v.str.s);
		pkg_free(gp->v.str.s);
		gp->v.str.s = NULL;
		gp->v.i = hdr.type;
		gp->type = GPARAM_TYPE_INT;
	} else {
		gp->type = GPARAM_TYPE_STR;
		LM_DBG("using hdr type name <%.*s>\n", gp->v.str.len, gp->v.str.s);
	}
	
	*param = (void*)gp;
	return 0;
}

static int free_hname_fixup(void** param, int param_no)
{
	if(*param)
	{
		if(((gparam_p)(*param))->type==GPARAM_TYPE_STR)
			pkg_free(((gparam_p)(*param))->v.str.s);
		pkg_free(*param);
		*param = 0;
	}
	return 0;
}

/*
 * Convert char* method to str* parameter
 */
static int fixup_method(void** param, int param_no)
{
	str* s;
	char *p;
	int m;
	unsigned int method;
	
	s = (str*)pkg_malloc(sizeof(str));
	if (!s) {
		LM_ERR("no pkg memory left\n");
		return E_UNSPEC;
	}

	s->s = (char*)*param;
	s->len = strlen(s->s);
	if(s->len==0)
	{
		LM_ERR("empty method name\n");
		pkg_free(s);
		return E_UNSPEC;
	}
	m=0;
	p=s->s;
	while(*p)
	{
		if(*p=='|')
		{
			*p = ',';
			m=1;
		}
		p++;
	}
	if(parse_methods(s, &method)!=0)
	{
		LM_ERR("bad method names\n");
		pkg_free(s);
		return E_UNSPEC;
	}

	if(m==1)
	{
		if(method==METHOD_UNDEF || method&METHOD_OTHER)
		{
			LM_ERR("unknown method in list [%.*s/%d] - must be only defined methods\n",
				s->len, s->s, method);
			pkg_free(s);
			return E_UNSPEC;
		}
		LM_DBG("using id for methods [%.*s/%d]\n",
				s->len, s->s, method);
		s->s = 0;
		s->len = method;
	} else {
		if(method!=METHOD_UNDEF && method!=METHOD_OTHER)
		{
			LM_DBG("using id for method [%.*s/%d]\n",
				s->len, s->s, method);
			s->s = 0;
			s->len = method;
		} else
			LM_DBG("name for method [%.*s/%d]\n",
				s->len, s->s, method);
	}

	*param = (void*)s;
	return 0;
}

/*
 * Convert char* privacy value to corresponding bit value
 */
int fixup_privacy(void** param, int param_no)
{
    str p;
    unsigned int val;

    p.s = (char*)*param;
    p.len = strlen(p.s);

    if (p.len == 0) {
	LM_ERR("empty privacy value\n");
	return E_UNSPEC;
    }

    if (parse_priv_value(p.s, p.len, &val) != p.len) {
	LM_ERR("invalid privacy value\n");
	return E_UNSPEC;
    }
    
    *param = (void *)(long)val;
    return 0;
}

/*
 * Fix in_list params: subject and list (strings that may contain pvars),
 * separator (string)
 */
static int fixup_in_list(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) return fixup_spve_null(param, 1);
    
    if (param_no == 3) {
	if ((strlen((char *)*param) != 1) || (*((char *)(*param)) == 0)) {
	    LM_ERR("invalid separator parameter\n");
 	    return -1;
 	}
 	return 0;
    }
 
    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}
 
/*
 * Free in_list params
 */
static int fixup_free_in_list(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
	return fixup_free_spve_null(param, 1);
    }
 
    if (param_no == 3) return 0;
     
    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

static int add_header_fixup(void** param, int param_no)
{
	if(param_no==1)
	{
		return fixup_spve_null(param, param_no);
	} else if(param_no==2) {
		return hname_fixup(param, param_no);
	} else {
		LM_ERR("wrong number of parameters\n");
		return E_UNSPEC;
	}
}


static int fixup_body_type(void** param, int param_no)
{
	char *p;
	char *r;
	unsigned int type;

	if(param_no==1) {
		p = (char*)*param;
		if (p==0 || p[0]==0) {
			type = 0;
		} else {
			r = decode_mime_type( p, p+strlen(p) , &type);
			if (r==0) {
				LM_ERR("unsupported mime <%s>\n",p);
				return E_CFG;
			}
			if ( r!=p+strlen(p) ) {
				LM_ERR("multiple mimes not supported!\n");
				return E_CFG;
			}
		}
		pkg_free(*param);
		*param = (void*)(long)type;
	}
	return 0;

}


static int has_body_f(struct sip_msg *msg, char *type, char *str2 )
{
	int mime;

	/* parse content len hdr */
	if ( msg->content_length==NULL &&
	(parse_headers(msg,HDR_CONTENTLENGTH_F, 0)==-1||msg->content_length==NULL))
		return -1;

	if (get_content_length (msg)==0) {
		LM_DBG("content length is zero\n");
		/* Nothing to see here, please move on. */
		return -1;
	}

	/* check type also? */
	if (type==0)
		return 1;

	/* the function search for and parses the Content-Type hdr */
	mime = parse_content_type_hdr (msg);
	if (mime<0) {
		LM_ERR("failed to extract content type hdr\n");
		return -1;
	}
	if (mime==0) {
		/* content type hdr not found -> according the RFC3261 we
		 * assume APPLICATION/SDP  --bogdan */
		mime = ((TYPE_APPLICATION << 16) + SUBTYPE_SDP);
	}
	LM_DBG("content type is %d\n",mime);

	if ( (unsigned int)mime!=(unsigned int)(unsigned long)type )
		return -1;

	return 1;
}


int is_privacy_f(struct sip_msg *msg, char *_privacy, char *str2 )
{
    if (parse_privacy(msg) == -1)
	return -1;

    return get_privacy_values(msg) & ((unsigned int)(long)_privacy) ? 1 : -1;

}

/* 
 * Checks if subject is found in list
 */
int in_list_f(struct sip_msg* _m, char* _subject, char* _list, char* _sep)
{
    str subject, list;
    int sep;
    char *at, *past, *next_sep, *s;

    if (fixup_get_svalue(_m, (gparam_p)_subject, &subject) != 0) {
		LM_ERR("cannot get subject value\n");
		return -1;
    } else {
		if (subject.len == 0) {
			LM_ERR("subject cannot be empty string\n");
			return -1;
		}
    }

    if (fixup_get_svalue(_m, (gparam_p)_list, &list) != 0) {
		LM_ERR("cannot get list value\n");
		return -1;
    } else {
		if (list.len == 0) return -1;
    }

    sep = _sep[0];

    at = list.s;
    past = list.s + list.len;

	/* Eat leading white space */
	while ((at < past) && 
		  ((*at == ' ') || (*at == '\t') || (*at == '\r') || (*at == '\n') )) {
			at++;
	}

    while (at < past) {
		next_sep = index(at, sep);
		s = next_sep;
		
		if (s == NULL) {
			/* Eat trailing white space */
			while ((at < past) && 
				  ((*(past-1) == ' ') || (*(past-1) == '\t') || (*(past-1) == '\r') || (*(past-1) == '\n') )) {
					past--;
			}
			if ((subject.len == (past - at)) &&
				strncmp(at, subject.s, subject.len) == 0) {
				return 1;
			} else {
				return -1;
			}
		} else {
			/* Eat trailing white space */
			while ((at < s) && 
				  ((*(s-1) == ' ') || (*(s-1) == '\t') || (*(s-1) == '\r') || (*(s-1) == '\n') )) {
					s--;
			}
			if ((subject.len == (s - at)) &&
				strncmp(at, subject.s, subject.len) == 0) {
				return 1;
			} else {
				at = next_sep + 1;
				/* Eat leading white space */
				while ((at < past) && 
					  ((*at == ' ') || (*at == '\t') || (*at == '\r') || (*at == '\n') )) {
						at++;
				}
			}
		}
    }

    return -1;
}

static int cmp_str_f(struct sip_msg *msg, char *str1, char *str2 )
{
	str s1;
	str s2;
	int ret;

	if(fixup_get_svalue(msg, (gparam_p)str1, &s1)!=0)
	{
		LM_ERR("cannot get first parameter\n");
		return -8;
	}
	if(fixup_get_svalue(msg, (gparam_p)str2, &s2)!=0)
	{
		LM_ERR("cannot get second parameter\n");
		return -8;
	}
	ret = cmp_str(&s1, &s2);
	if(ret==0)
		return 1;
	if(ret>0)
		return -1;
	return -2;
}

static int cmp_istr_f(struct sip_msg *msg, char *str1, char *str2)
{
	str s1;
	str s2;
	int ret;

	if(fixup_get_svalue(msg, (gparam_p)str1, &s1)!=0)
	{
		LM_ERR("cannot get first parameter\n");
		return -8;
	}
	if(fixup_get_svalue(msg, (gparam_p)str2, &s2)!=0)
	{
		LM_ERR("cannot get second parameter\n");
		return -8;
	}
	ret = cmpi_str(&s1, &s2);
	if(ret==0)
		return 1;
	if(ret>0)
		return -1;
	return -2;
}

static int starts_with_f(struct sip_msg *msg, char *str1, char *str2 )
{
	str s1;
	str s2;
	int ret;

	if(fixup_get_svalue(msg, (gparam_p)str1, &s1)!=0)
	{
		LM_ERR("cannot get first parameter\n");
		return -8;
	}
	if(fixup_get_svalue(msg, (gparam_p)str2, &s2)!=0)
	{
		LM_ERR("cannot get second parameter\n");
		return -8;
	}
	if (s1.len < s2.len) return -1;
	ret = strncmp(s1.s, s2.s, s2.len);
	if(ret==0)
		return 1;
	if(ret>0)
		return -1;
	return -2;
}

static int is_audio_on_hold_f(struct sip_msg *msg, char *str1, char *str2 )
{
	int sdp_session_num = 0, sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;

	if (0 == parse_sdp(msg)) {
		for(;;) {
			sdp_session = get_sdp_session(msg, sdp_session_num);
			if(!sdp_session) break;
			sdp_stream_num = 0;
			for(;;) {
				sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
				if(!sdp_stream) break;
				if(sdp_stream->media.len==AUDIO_STR_LEN &&
					strncmp(sdp_stream->media.s,AUDIO_STR,AUDIO_STR_LEN)==0 &&
					sdp_stream->is_on_hold)
					return 1;
				sdp_stream_num++;
			}
			sdp_session_num++;
		}
	}
	return -1;
}

int fixup_regexpNL_none(void** param, int param_no)
{
	regex_t* re;

	if (param_no != 1 && param_no != 2 )
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	if (param_no == 2)
		return 0;
	/* param 1 */
	if ((re=pkg_malloc(sizeof(regex_t)))==0) {
		LM_ERR("no more pkg memory\n");
		return E_OUT_OF_MEM;
	}
	if (regcomp(re, *param, REG_EXTENDED|REG_ICASE)) {
		pkg_free(re);
		LM_ERR("bad re %s\n", (char*)*param);
		return E_BAD_RE;
	}
	/* free string */
	pkg_free(*param);
	/* replace it with the compiled re */
	*param=re;
	return 0;
}

/*! \brief
 * fixup for functions that get two parameters
 * - first parameter is converted to regular expression structure
 * - second parameter is not converted
 */
int fixup_regexp_none(void** param, int param_no)
{
	if (param_no != 1 && param_no != 2 )
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	if (param_no == 1)
		return fixup_regexp_null(param, 1);
	return 0;
}

/**
 * fixup free for functions that get two parameters
 * - first parameter was converted to regular expression
 * - second parameter was notconverted
 */
int fixup_free_regexp_none(void** param, int param_no)
{
	if (param_no != 1 && param_no != 2 )
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	if (param_no == 1)
		return fixup_free_regexp_null(param, 1);
	return 0;
}


/**
 *
 */
static int search_hf_f(struct sip_msg* msg, char* str_hf, char* re, char *flags)
{
	hdr_field_t *hf;
	hdr_field_t *hfl = NULL;
	str body;
	gparam_t *gp;
	regmatch_t pmatch;
	char c;
	int ret;

	gp = (gparam_t*)str_hf;

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);
	for (hf=msg->headers; hf; hf=hf->next) {
		if(gp->type==GPARAM_TYPE_INT)
		{
			if (gp->v.i!=hf->type)
				continue;
		} else {
			if (hf->name.len!=gp->v.str.len)
				continue;
			if (cmp_hdrname_str(&hf->name,&gp->v.str)!=0)
				continue;
		}

		if(flags==NULL || *flags!='l')
		{
			body = hf->body;
			c = body.s[body.len];
			body.s[body.len] = '\0';
			ret = regexec((regex_t*) re, body.s, 1, &pmatch, 0);
			body.s[body.len] = c;
			if(ret==0)
			{
				/* match */
				if(flags==NULL || *flags!='l')
					return 1;
			} else {
				if(flags!=NULL && *flags=='f')
					return -1;
			}
		} else {
			hfl = hf;
		}
	}
	if(hfl!=NULL)
	{
		hf = hfl;
		body = hf->body;
		c = body.s[body.len];
		body.s[body.len] = '\0';
		ret = regexec((regex_t*) re, body.s, 1, &pmatch, 0);
		body.s[body.len] = c;
		if(ret==0)
			return 1;
	}
	return -1;
}

/*
 * Convert header name, regexp and flags
 */
static int fixup_search_hf(void** param, int param_no)
{
	if(param_no==1)
		return hname_fixup(param, param_no);
	if(param_no==2)
		return fixup_regexp_null(param, 1);
	return 0;
}

/* sed-perl style re: s/regular expression/replacement/flags */
static int subst_hf_f(struct sip_msg *msg, char *str_hf, char *subst, char *flags)
{
	struct lump* l;
	struct replace_lst* lst = NULL;
	struct replace_lst* rpl = NULL;
	char* begin;
	struct subst_expr* se;
	int off;
	int nmatches=0;
	str body;
	hdr_field_t *hf;
	hdr_field_t *hfl = NULL;
	gparam_t *gp;
	char c;
	int ret;

	ret = -1;
	gp = (gparam_t*)str_hf;
	se=(struct subst_expr*)subst;

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);
	for (hf=msg->headers; hf; hf=hf->next) {
		if(gp->type==GPARAM_TYPE_INT)
		{
			if (gp->v.i!=hf->type)
				continue;
		} else {
			if (hf->name.len!=gp->v.str.len)
				continue;
			if (cmp_hdrname_str(&hf->name,&gp->v.str)!=0)
				continue;
		}

		if(flags==NULL || *flags!='l')
		{
			body = hf->body;
			c = body.s[body.len];
			body.s[body.len] = '\0';

			begin=body.s;

			off=begin-msg->buf;
			lst=subst_run(se, begin, msg, &nmatches);
			body.s[body.len] = c;
			if(lst==0 && flags!=NULL && *flags=='f')
				goto error; /* not found */
			if(lst!=0)
				ret=1;
			for (rpl=lst; rpl; rpl=rpl->next)
			{
				LM_DBG("%s replacing at offset %d [%.*s] with [%.*s]\n",
						exports.name, rpl->offset+off,
						rpl->size, rpl->offset+off+msg->buf,
						rpl->rpl.len, rpl->rpl.s);
				if ((l=del_lump(msg, rpl->offset+off, rpl->size, 0))==0)
				{
					ret=-1;
					goto error;
				}
				/* hack to avoid re-copying rpl, possible because both 
				 * replace_lst & lumps use pkg_malloc */
				if (insert_new_lump_after(l, rpl->rpl.s, rpl->rpl.len, 0)==0)
				{
					LM_ERR("%s could not insert new lump\n",
						exports.name);
					ret=-1;
					goto error;
				}
				/* hack continued: set rpl.s to 0 so that replace_lst_free will
				 * not free it */
				rpl->rpl.s=0;
				rpl->rpl.len=0;
			}
		} else {
			hfl = hf;
		}
		/* if flags set for first header, then all done */
		if(flags!=NULL && *flags=='f')
			return ret;
	}
	if(hfl!=NULL)
	{
		hf= hfl;
		body = hf->body;
		c = body.s[body.len];
		body.s[body.len] = '\0';

		begin=body.s;

		off=begin-msg->buf;
		lst=subst_run(se, begin, msg, &nmatches);
		body.s[body.len] = c;
		if(lst==0)
			goto error; /* not found */
		ret=1;
		for (rpl=lst; rpl; rpl=rpl->next)
		{
			LM_DBG("%s replacing at offset %d [%.*s] with [%.*s]\n",
					exports.name, rpl->offset+off,
					rpl->size, rpl->offset+off+msg->buf,
					rpl->rpl.len, rpl->rpl.s);
			if ((l=del_lump(msg, rpl->offset+off, rpl->size, 0))==0)
			{
				ret=-1;
				goto error;
			}
			/* hack to avoid re-copying rpl, possible because both 
			 * replace_lst & lumps use pkg_malloc */
			if (insert_new_lump_after(l, rpl->rpl.s, rpl->rpl.len, 0)==0)
			{
				LM_ERR("%s could not insert new lump\n",
					exports.name);
				ret=-1;
				goto error;
			}
			/* hack continued: set rpl.s to 0 so that replace_lst_free will
			 * not free it */
			rpl->rpl.s=0;
			rpl->rpl.len=0;
		}
	}
error:
	LM_DBG("lst was %p\n", lst);
	if (lst) replace_lst_free(lst);
	if (nmatches<0)
		LM_ERR("%s subst_run failed\n", exports.name);
	return ret;
}

/*
 * Convert header name, substexp and flags
 */
static int fixup_subst_hf(void** param, int param_no)
{
	if(param_no==1)
		return hname_fixup(param, param_no);
	if(param_no==2)
		return fixup_substre(param, 1);
	return 0;
}

