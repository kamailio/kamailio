/*$Id$
 *
 * Example ser module, it implements the following commands:
 * search_append("key", "txt") - insert a "txt" after "key"
 * replace("txt1", "txt2") - replaces txt1 with txt2 (txt1 can be a re)
 * search("txt") - searches for txt (txt can be a regular expression)
 * append_to_reply("txt") - appends txt to the reply?
 * append_hf("P-foo: bar\r\n");
 *
 * 
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
 *
 * History:
 * -------
 *  2003-02-28  scratchpad compatibility abandoned (jiri)
 *  2003-01-29: - rewriting actions (replace, search_append) now begin
 *                at the second line -- previously, they could affect
 *                first line too, which resulted in wrong calculation of
 *                forwarded requests and an error consequently
 *              - replace_all introduced
 *  2003-01-28  scratchpad removed (jiri)
 *  2003-01-18  append_urihf introduced (jiri)
 *  2003-03-10  module export interface updated to the new format (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-97  actions permitted to be used from failure/reply routes (jiri)
 *  2003-04-21  remove_hf and is_present_hf introduced (jiri)
 *  2003-08-19  subst added (support for sed like res:s/re/repl/flags) (andrei)
 *  2003-08-20  subst_uri added (like above for uris) (andrei)
 *  2003-09-11  updated to new build_lump_rpl() interface (bogdan)
 *  2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 *  2004-05-09: append_time introduced (jiri)
 */




#include "../../comp_defs.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../re.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* for regex */
#include <regex.h>
#include <time.h>
#include <sys/time.h>

MODULE_VERSION


/* RFC822-conformant dates format:

   %a -- abbreviated week of day name (locale), %d day of month
   as decimal number, %b abbreviated month name (locale), %Y
   year with century, %T time in 24h notation
*/
#define TIME_FORMAT "Date: %a, %d %b %Y %H:%M:%S GMT"
#define MAX_TIME 64


static int search_f(struct sip_msg*, char*, char*);
static int replace_f(struct sip_msg*, char*, char*);
static int subst_f(struct sip_msg*, char*, char*);
static int subst_uri_f(struct sip_msg*, char*, char*);
static int remove_hf_f(struct sip_msg* msg, char* str_hf, char* foo);
static int is_present_hf_f(struct sip_msg* msg, char* str_hf, char* foo);
static int replace_all_f(struct sip_msg* msg, char* key, char* str);
static int search_append_f(struct sip_msg*, char*, char*);
static int append_to_reply_f(struct sip_msg* msg, char* key, char* str);
static int append_hf(struct sip_msg* msg, char* str1, char* str2);
static int append_urihf(struct sip_msg* msg, char* str1, char* str2);
static int append_time_f(struct sip_msg* msg, char* , char *);

static int fixup_regex(void**, int);
static int fixup_substre(void**, int);
static int str_fixup(void** param, int param_no);

static int mod_init(void);


static cmd_export_t cmds[]={
	{"search",           search_f,          1, fixup_regex, 
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE}, 
	{"search_append",    search_append_f,   2, fixup_regex, 
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE}, 
	{"replace",          replace_f,         2, fixup_regex, 
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE}, 
	{"replace_all",      replace_all_f,     2, fixup_regex, 
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE}, 
	{"append_to_reply",  append_to_reply_f, 1, 0, 
			REQUEST_ROUTE},
	{"append_hf",        append_hf,         1, str_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE}, 
	{"append_urihf",     append_urihf,      2, str_fixup,   
			REQUEST_ROUTE|FAILURE_ROUTE},
	{"remove_hf",        remove_hf_f,         1, str_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE}, 
	{"is_present_hf",        is_present_hf_f,         1, str_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE}, 
	{"subst",            subst_f,             1, fixup_substre,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE}, 
	{"subst_uri",            subst_uri_f,     1, fixup_substre,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE}, 
	{"append_time",		append_time_f,		0, 0,
		REQUEST_ROUTE },
	{0,0,0,0,0}
};

static param_export_t params[]={ {0,0,0} }; /* no params */

struct module_exports exports= {
	"textops",
	cmds,
	params,
	mod_init, /* module initialization function */
	0, /* response function */
	0,  /* destroy function */
	0, /* on_cancel function */
	0, /* per-child init function */
};


static int mod_init(void)
{
	fprintf(stderr, "%s - initializing\n", exports.name);
	return 0;
}

static char *get_header(struct sip_msg *msg)
{
	return msg->buf+msg->first_line.len;
}



static int search_f(struct sip_msg* msg, char* key, char* str2)
{
	/*we registered only 1 param, so we ignore str2*/
	regmatch_t pmatch;

	if (regexec((regex_t*) key, msg->buf, 1, &pmatch, 0)!=0) return -1;
	return 1;
}



static int search_append_f(struct sip_msg* msg, char* key, char* str)
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
		len=strlen(str);
		s=pkg_malloc(len);
		if (s==0){
			LOG(L_ERR, "ERROR: search_append_f: mem. allocation failure\n");
			return -1;
		}
		memcpy(s, str, len); 
		if (insert_new_lump_after(l, s, len, 0)==0){
			LOG(L_ERR, "ERROR: could not insert new lump\n");
			pkg_free(s);
			return -1;
		}
		return 1;
	}
	return -1;
}


static int replace_all_f(struct sip_msg* msg, char* key, char* str)
{


	struct lump* l;
	regmatch_t pmatch;
	char* s;
	int len;
	char* begin;
	int off;
	int ret;
	int eflags;

	begin=get_header(msg); /* msg->orig previously .. uri problems */
	ret=-1; /* pessimist: we will not find any */
	len=strlen(str);
	eflags=0; /* match ^ at the beginning of the string*/

	while (begin<msg->buf+msg->len 
				&& regexec((regex_t*) key, begin, 1, &pmatch, eflags)==0) {
		off=begin-msg->buf;
		/* change eflags, not to match any more at string start */
		eflags|=REG_NOTBOL;
		if (pmatch.rm_so==-1){
			LOG(L_ERR, "ERROR: replace_all_f: offset unknown\n");
			return -1;
		}
		if ((l=del_lump(msg, pmatch.rm_so+off,
						pmatch.rm_eo-pmatch.rm_so, 0))==0) {
			LOG(L_ERR, "ERROR: replace_all_f: del_lump failed\n");
			return -1;
		}
		s=pkg_malloc(len);
		if (s==0){
			LOG(L_ERR, "ERROR: replace_f: mem. allocation failure\n");
			return -1;
		}
		memcpy(s, str, len); 
		if (insert_new_lump_after(l, s, len, 0)==0){
			LOG(L_ERR, "ERROR: could not insert new lump\n");
			pkg_free(s);
			return -1;
		}
		/* new cycle */
		begin=begin+pmatch.rm_eo;
		ret=1;
	} /* while found ... */
	return ret;
}

static int replace_f(struct sip_msg* msg, char* key, char* str)
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
		len=strlen(str);
		s=pkg_malloc(len);
		if (s==0){
			LOG(L_ERR, "ERROR: replace_f: mem. allocation failure\n");
			return -1;
		}
		memcpy(s, str, len); 
		if (insert_new_lump_after(l, s, len, 0)==0){
			LOG(L_ERR, "ERROR: could not insert new lump\n");
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
	
	se=(struct subst_expr*)subst;
	begin=get_header(msg);  /* start after first line to avoid replacing
							   the uri */
	off=begin-msg->buf;
	ret=-1;
	if ((lst=subst_run(se, begin, msg))==0) goto error; /* not found */
	for (rpl=lst; rpl; rpl=rpl->next){
		DBG(" %s: subst_f: replacing at offset %d [%.*s] with [%.*s]\n",
				exports.name, rpl->offset+off,
				rpl->size, rpl->offset+off+msg->buf,
				rpl->rpl.len, rpl->rpl.s);
		if ((l=del_lump(msg, rpl->offset+off, rpl->size, 0))==0)
			goto error;
		/* hack to avoid re-copying rpl, possible because both 
		 * replace_lst & lumps use pkg_malloc */
		if (insert_new_lump_after(l, rpl->rpl.s, rpl->rpl.len, 0)==0){
			LOG(L_ERR, "ERROR: %s: subst_f: could not insert new lump\n",
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
	DBG("subst_f: lst was %p\n", lst);
	if (lst) replace_lst_free(lst);
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
	/* ugly hack: 0 s[len], and restore it afterwards
	 * (our re functions require 0 term strings), we can do this
	 * because we always alloc len+1 (new_uri) and for first_line, the
	 * message will always be > uri.len */
	c=tmp[len];
	tmp[len]=0;
	result=subst_str(tmp, msg, se); /* pkg malloc'ed result */
	tmp[len]=c;
	if (result){
		DBG("%s: subst_uri_f: match - old uri= [%.*s], new uri= [%.*s]\n",
				exports.name, len, tmp,
				(result->len)?result->len:0,(result->s)?result->s:"");
		if (msg->new_uri.s) pkg_free(msg->new_uri.s);
		msg->new_uri=*result;
		msg->parsed_uri_ok=0; /* reset "use cached parsed uri" flag */
		pkg_free(result); /* free str* pointer */
		return 1; /* success */
	}
	return -1; /* false, no subst. made */
}
	
	

static int remove_hf_f(struct sip_msg* msg, char* str_hf, char* foo)
{
	struct hdr_field *hf;
	struct lump* l;
	int cnt;

	cnt=0;
	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH, 0);
	for (hf=msg->headers; hf; hf=hf->next) {
		if (hf->name.len!=((str *)str_hf)->len)
			continue;
		if (strncasecmp(hf->name.s, ((str *)str_hf)->s, hf->name.len)!=0)
			continue;
		l=del_lump(msg, hf->name.s-msg->buf, hf->len, 0);
		if (l==0) {
			LOG(L_ERR, "ERROR: remove_hf_f: no memory\n");
			return -1;
		}
		cnt++;
	}
	return cnt==0 ? -1 : 1;
}

static int is_present_hf_f(struct sip_msg* msg, char* str_hf, char* foo)
{
	struct hdr_field *hf;

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH, 0);
	for (hf=msg->headers; hf; hf=hf->next) {
		if (hf->name.len!=((str *)str_hf)->len)
			continue;
		if (strncasecmp(hf->name.s, ((str *)str_hf)->s, hf->name.len)!=0)
			continue;
		return 1;
	}
	return -1;
}



static int fixup_regex(void** param, int param_no)
{
	regex_t* re;

	DBG("module - fixing %s\n", (char*)(*param));
	if (param_no!=1) return 0;
	if ((re=pkg_malloc(sizeof(regex_t)))==0) return E_OUT_OF_MEM;
	if (regcomp(re, *param, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ){
		pkg_free(re);
		LOG(L_ERR, "ERROR: %s : bad re %s\n", exports.name, (char*)*param);
		return E_BAD_RE;
	}
	/* free string */
	pkg_free(*param);
	/* replace it with the compiled re */
	*param=re;
	return 0;
}



static int fixup_substre(void** param, int param_no)
{
	struct subst_expr* se;
	str subst;

	DBG("%s module -- fixing %s\n", exports.name, (char*)(*param));
	if (param_no!=1) return 0;
	subst.s=*param;
	subst.len=strlen(*param);
	se=subst_parser(&subst);
	if (se==0){
		LOG(L_ERR, "ERROR: %s: bad subst. re %s\n", exports.name, 
				(char*)*param);
		return E_BAD_RE;
	}
	/* free string */
	pkg_free(*param);
	/* replace it withj the compiled subst. re */
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
		LOG(L_ERR, "ERROR: append_time: gmtime failed\n");
		return -1;
	}

	len=strftime(time_str, MAX_TIME, TIME_FORMAT, bd_time);
	if (len>MAX_TIME+2 || len==0) {
		LOG(L_ERR, "ERROR: append_time: unexpected time length\n");
		return -1;
	}

	time_str[len]='\r';
	time_str[len+1]='\n';


	if (add_lump_rpl(msg, time_str, len+2, LUMP_RPL_HDR)==0)
	{
		LOG(L_ERR, "ERROR: append_time: unable to add lump\n");
		return -1;
	}

	return 1;
}

static int append_to_reply_f(struct sip_msg* msg, char* key, char* str)
{
	if ( add_lump_rpl( msg, key, strlen(key), LUMP_RPL_HDR)==0 )
	{
		LOG(L_ERR,"ERROR:append_to_reply : unable to add lump_rl\n");
		return -1;
	}

	return 1;
}


/* add str1 to end of header or str1.r-uri.str2 */

static int append_hf_helper(struct sip_msg* msg, str *str1, str *str2)
{
	struct lump* anchor;
	char *s;
	int len;

	if (parse_headers(msg, HDR_EOH, 0) == -1) {
		LOG(L_ERR, "append_hf(): Error while parsing message\n");
		return -1;
	}

	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if (anchor == 0) {
		LOG(L_ERR, "append_hf(): Can't get anchor\n");
		return -1;
	}

	len=str1->len;
	if (str2) len+= str2->len + REQ_LINE(msg).uri.len;

	s = (char*)pkg_malloc(len);
	if (!s) {
		LOG(L_ERR, "append_hf(): No memory left\n");
		return -1;
	}

	memcpy(s, str1->s, str1->len);
	if (str2) {
		memcpy(s+str1->len, REQ_LINE(msg).uri.s, REQ_LINE(msg).uri.len);
		memcpy(s+str1->len+REQ_LINE(msg).uri.len, str2->s, str2->len );
	}

	if (insert_new_lump_before(anchor, s, len, 0) == 0) {
		LOG(L_ERR, "append_hf(): Can't insert lump\n");
		pkg_free(s);
		return -1;
	}
	return 1;
}

static int append_hf(struct sip_msg *msg, char *str1, char *str2 )
{
	return append_hf_helper(msg, (str *) str1, (str *) 0);
}

static int append_urihf(struct sip_msg *msg, char *str1, char *str2 )
{
	return append_hf_helper(msg, (str *) str1, (str *) str2);
}



/*
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
	str* s;

	s = (str*)pkg_malloc(sizeof(str));
	if (!s) {
		LOG(L_ERR, "str_fixup(): No memory left\n");
		return E_UNSPEC;
	}

	s->s = (char*)*param;
	s->len = strlen(s->s);
	*param = (void*)s;

	return 0;
}
