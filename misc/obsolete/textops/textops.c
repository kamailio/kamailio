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
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
 *  2004-07-06  subst_user added (like subst_uri but only for user) (sobomax)
 *  2004-11-12  subst_user changes (old serdev mails) (andrei)
 *  2006-02-23  xl_lib formating, multi-value support (tma)
 *  2006-08-30  added static buffer support (tma)
 */


#include "../../comp_defs.h"
#include "../../action.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../re.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_hname2.h"
#include "../../onsend.h"
#include "../../ut.h"
#include "../../select.h"
#include "../../modules/xprint/xp_lib.h"
#include "../../script_cb.h"
#include "../../select_buf.h"
#include "../../ser_time.h"
#include "../../dset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* for regex */
#include <regex.h>
#include <time.h>
#include <sys/time.h>

MODULE_VERSION

/* RFC822-conforming dates format:

   %a -- abbreviated week of day name (locale), %d day of month
   as decimal number, %b abbreviated month name (locale), %Y
   year with century, %T time in 24h notation
*/
#define TIME_FORMAT "Date: %a, %d %b %Y %H:%M:%S GMT"
#define MAX_TIME 64

static int xlbuf_size = 4096;


static int search_f(struct sip_msg*, char*, char*);
static int replace_f(struct sip_msg*, char*, char*);
static int subst_f(struct sip_msg*, char*, char*);
static int subst_uri_f(struct sip_msg*, char*, char*);
static int subst_user_f(struct sip_msg*, char*, char*);
static int remove_hf_f(struct sip_msg* msg, char* str_hf, char* foo);
static int remove_hf_re_f(struct sip_msg* msg, char* str_hf, char* foo);
static int is_present_hf_f(struct sip_msg* msg, char* str_hf, char* foo);
static int replace_all_f(struct sip_msg* msg, char* key, char* str);
static int search_append_f(struct sip_msg*, char*, char*);
static int append_to_reply_f(struct sip_msg* msg, char* key, char* str);
static int append_hf(struct sip_msg* msg, char* str1, char* str2);
static int append_urihf(struct sip_msg* msg, char* str1, char* str2);
static int append_time_f(struct sip_msg* msg, char* , char *);

static int incexc_hf_value_f(struct sip_msg* msg, char* , char *);
static int include_hf_value_fixup(void**, int);
static int exclude_hf_value_fixup(void**, int);
static int hf_value_exists_fixup(void**, int);

static int insupddel_hf_value_f(struct sip_msg* msg, char* _hname, char* _val);
static int append_hf_value_fixup(void** param, int param_no);
static int insert_hf_value_fixup(void** param, int param_no);
static int remove_hf_value_fixup(void** param, int param_no);
static int assign_hf_value_fixup(void** param, int param_no);
static int remove_hf_value2_fixup(void** param, int param_no);
static int assign_hf_value2_fixup(void** param, int param_no);
static int fixup_xlstr(void** param, int param_no);
static int fixup_regex_xlstr(void** param, int param_no);

static int fixup_substre(void**, int);

extern select_row_t sel_declaration[];

static int mod_init(void);


static cmd_export_t cmds[]={
	{"search",           search_f,          1, fixup_regex_1,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{"search_append",    search_append_f,   2, fixup_regex_xlstr,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE},
	{"replace",          replace_f,         2, fixup_regex_xlstr,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE},
	{"replace_all",      replace_all_f,     2, fixup_regex_xlstr,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE},
	{"append_to_reply",  append_to_reply_f, 1, fixup_xlstr,
			REQUEST_ROUTE},
	{"append_hf",        append_hf,         1, fixup_xlstr,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE | BRANCH_ROUTE},
	{"append_urihf",     append_urihf,      2, fixup_xlstr,
			REQUEST_ROUTE|FAILURE_ROUTE},
	/* obsolete: use remove_hf_value(), does not support compact headers */
	{"remove_hf",        remove_hf_f,         1, fixup_var_str_1,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE},
	{"remove_hf_re",     remove_hf_re_f,      1, fixup_regex_12,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE},
	/* obsolete: use @msg.HFNAME, , does not support compact headers */
	{"is_present_hf",        is_present_hf_f,         1, fixup_var_str_1,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE},
	{"subst",            subst_f,             1, fixup_substre,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE},
	{"subst_uri",            subst_uri_f,     1, fixup_substre,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE},
	{"subst_user",           subst_user_f,    1, fixup_substre,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE},
	{"append_time",		append_time_f,		0, 0,
			REQUEST_ROUTE },


	{"append_hf_value",        insupddel_hf_value_f,         2, append_hf_value_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"insert_hf_value",        insupddel_hf_value_f,         2, insert_hf_value_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"remove_hf_value",        insupddel_hf_value_f,         1, remove_hf_value_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"assign_hf_value",        insupddel_hf_value_f,         2, assign_hf_value_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"remove_hf_value2",       insupddel_hf_value_f,         1, remove_hf_value2_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"assign_hf_value2",       insupddel_hf_value_f,         2, assign_hf_value2_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"include_hf_value", incexc_hf_value_f,      2, include_hf_value_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"exclude_hf_value", incexc_hf_value_f,      2, exclude_hf_value_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"hf_value_exists",  incexc_hf_value_f,      2, hf_value_exists_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},

	{0,0,0,0,0}
};

static param_export_t params[]={
	{"xlbuf_size", PARAM_INT, &xlbuf_size},

	{0,0,0}
	}; /* no params */

struct module_exports exports= {
	"textops",
	cmds,
	0,        /* RPC methods */
	params,
	mod_init, /* module initialization function */
	0, /* response function */
	0, /* destroy function */
	0, /* on_cancel function */
	0, /* per-child init function */
};

static int mod_init(void)
{
	DBG("%s - initializing\n", exports.name);
	register_select_table(sel_declaration);
	return 0;
}

struct xlstr {
	str s;
	xl_elog_t* xlfmt;
};

static xl_print_log_f* xl_print = NULL;
static xl_parse_format_f* xl_parse = NULL;

#define NO_SCRIPT -1

static int fixup_xlstr(void** param, int param_no) {
	struct xlstr* s;
	s = pkg_malloc(sizeof(*s));
	if (!s) return E_OUT_OF_MEM;
	s->s.s = *param;
	s->s.len = strlen(s->s.s);
	s->xlfmt = 0;
	if (strchr(s->s.s, '%')) {
		if (!xl_print) {
			xl_print=(xl_print_log_f*)find_export("xprint", NO_SCRIPT, 0);
			if (!xl_print) {
				LOG(L_CRIT,"ERROR: textops: cannot find \"xprint\", is module xprint loaded?\n");
				return E_UNSPEC;
			}
		}

		if (!xl_parse) {
			xl_parse=(xl_parse_format_f*)find_export("xparse", NO_SCRIPT, 0);
			if (!xl_parse) {
				LOG(L_CRIT,"ERROR: textops: cannot find \"xparse\", is module xprint loaded?\n");
				return E_UNSPEC;
			}
		}
		if(xl_parse(s->s.s, &s->xlfmt) < 0) {
			LOG(L_ERR, "ERROR: textops: wrong format '%s'\n", s->s.s);
			return E_UNSPEC;
		}
	}
	*param = s;
	return 0;
}

static int eval_xlstr(struct sip_msg* msg, struct xlstr* val, str* s) {
	static char *xlbuf = NULL;
	if (val) {
		if (val->xlfmt) {
			if (!xlbuf) {
				xlbuf = pkg_malloc(xlbuf_size);
				if (!xlbuf) {
					LOG(L_ERR, "ERROR: out of memory\n");
					return E_OUT_OF_MEM;
				}
			}
			s->len = xlbuf_size-1;
			if (xl_print(msg, val->xlfmt, xlbuf, &s->len) < 0) {
				LOG(L_ERR, "ERROR: textops: eval_xlstr: Error while formating result '%.*s'\n", val->s.len, val->s.s);
				s->len = 0;
				return E_UNSPEC;
			}
			s->s = xlbuf;
		}
		else {
			*s = val->s;
		}
	}
	else
		s->len = 0;
	return 1;
}

static int fixup_regex_xlstr(void** param, int param_no) {
	if (param_no == 1)
		return fixup_regex_1(param, param_no);
	else if (param_no == 2)
		return fixup_xlstr(param, param_no);
	else
		return 0;
}

static char *get_header(struct sip_msg *msg)
{
	return SIP_MSG_START(msg)+msg->first_line.len;
}

static int search_f(struct sip_msg* msg, char* key, char* str2)
{
	/*we registered only 1 param, so we ignore str2*/
	regmatch_t pmatch;
	char* buf;
	struct onsend_info* snd_inf;

	if ((snd_inf=get_onsend_info())!=0)
		buf=snd_inf->buf;
	else
		buf=msg->buf;

	if (regexec(((fparam_t*)key)->v.regex, buf, 1, &pmatch, 0)!=0) return -1;
	return 1;
}

static int search_append_f(struct sip_msg* msg, char* key, char* _str)
{
	struct lump* l;
	regmatch_t pmatch;
	str str;
	char* s;
	char *begin;
	int off;

	begin=get_header(msg); /* msg->orig/buf previously .. uri problems */
	off=begin-msg->buf;

	if (regexec(((fparam_t*)key)->v.regex, begin, 1, &pmatch, 0)!=0) return -1;
	if (pmatch.rm_so!=-1){
		if (eval_xlstr(msg, (void*) _str, &str) < 0) return -1;

		if ((l=anchor_lump(msg, off+pmatch.rm_eo, 0, 0))==0)
			return -1;
		s=pkg_malloc(str.len);
		if (s==0){
			LOG(L_ERR, "ERROR: search_append_f: mem. allocation failure\n");
			return -1;
		}
		memcpy(s, str.s, str.len);
		if (insert_new_lump_after(l, s, str.len, 0)==0){
			LOG(L_ERR, "ERROR: could not insert new lump\n");
			pkg_free(s);
			return -1;
		}
		return 1;
	}
	return -1;
}


static int replace_all_f(struct sip_msg* msg, char* key, char* _str)
{
	struct lump* l;
	regmatch_t pmatch;
	char* s;
	str str;
	char* begin;
	int off;
	int ret;
	int eflags;

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);

	begin=get_header(msg); /* msg->orig previously .. uri problems */
	ret=-1; /* pessimist: we will not find any */
	eflags=0; /* match ^ at the beginning of the string*/

	if (eval_xlstr(msg, (void*) _str, &str) < 0) return -1;
	while (begin<msg->buf+msg->len
				&& regexec(((fparam_t*)key)->v.regex, begin, 1, &pmatch, eflags)==0) {
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
		s=pkg_malloc(str.len);
		if (s==0){
			LOG(L_ERR, "ERROR: replace_all_f: mem. allocation failure\n");
			return -1;
		}
		memcpy(s, str.s, str.len);
		if (insert_new_lump_after(l, s, str.len, 0)==0){
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

static int replace_f(struct sip_msg* msg, char* key, char* _str)
{
	struct lump* l;
	regmatch_t pmatch;
	char* s;
	str str;
	char* begin;
	int off;

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);

	begin=get_header(msg); /* msg->orig previously .. uri problems */

	if (regexec(((fparam_t*)key)->v.regex, begin, 1, &pmatch, 0)!=0) return -1;
	off=begin-msg->buf;

	if (pmatch.rm_so!=-1){
		if (eval_xlstr(msg, (void*) _str, &str) < 0) return -1;

		if ((l=del_lump(msg, pmatch.rm_so+off,
						pmatch.rm_eo-pmatch.rm_so, 0))==0)
			return -1;
		s=pkg_malloc(str.len);
		if (s==0){
			LOG(L_ERR, "ERROR: replace_f: mem. allocation failure\n");
			return -1;
		}
		memcpy(s, str.s, str.len);
		if (insert_new_lump_after(l, s, str.len, 0)==0){
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
	int nmatches;

	se=(struct subst_expr*)subst;
	begin=get_header(msg);  /* start after first line to avoid replacing
							   the uri */
	off=begin-msg->buf;
	ret=-1;
	if ((lst=subst_run(se, begin, msg, &nmatches))==0)
		goto error; /* not found */
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
	if (nmatches<0)
		LOG(L_ERR, "ERROR: %s: subst_run failed\n", exports.name);
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
		DBG("%s: subst_uri_f: match - old uri= [%.*s], new uri= [%.*s]\n",
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
	str user;
	char c;
	int nmatches;
	struct run_act_ctx ra_ctx;

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
			LOG(L_ERR, "subst_user(): subst_str() failed\n");
		return -1;
	}
	/* result->s[result->len] = '\0';  --subst_str returns 0-term strings */
	memset(&act, 0, sizeof(act)); /* be on the safe side */
	act.type = SET_USER_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = result->s;
	init_run_actions_ctx(&ra_ctx);
	rval = do_action(&ra_ctx, &act, msg);
	if (result->s) pkg_free(result->s); /* SET_USER_T doesn't consume s */
	pkg_free(result);
	return rval;
}


static inline int remove_hf(struct sip_msg* msg, char* p1, int by_re)
{
	struct hdr_field *hf;
	struct lump* l;
	int cnt;
	str hfn;
	regex_t regexp;
	regmatch_t matches;
	char bkup;
	int no_match;

	if (by_re) {
		if (get_regex_fparam(&regexp, msg, (fparam_t*)p1) < 0) {
			ERR("remove_hf: Error while obtaining parameter value\n");
			return -1;
		}
	} else {
		if (get_str_fparam(&hfn, msg, (fparam_t*)p1) < 0) {
			ERR("remove_hf: Error while obtaining parameter value\n");
			return -1;
		}
	}

	cnt=0;
	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);
	for (hf=msg->headers; hf; hf=hf->next) {
		if (by_re) {
			/* 0-term. it for regexp run; should always be a safe op. */
			bkup = hf->name.s[hf->name.len];
			hf->name.s[hf->name.len] = 0;
			no_match = regexec(&regexp, hf->name.s, /*# matches */1, &matches, 
					/*flags*/0);
			hf->name.s[hf->name.len] = bkup;
			if (no_match)
				continue;
		} else {
			if (hf->name.len!=hfn.len)
				continue;
			if (strncasecmp(hf->name.s, hfn.s, hf->name.len)!=0)
				continue;
		}
		l=del_lump(msg, hf->name.s-msg->buf, hf->len, 0);
		if (l==0) {
			ERR("no memory\n");
			return -1;
		}
		cnt++;
	}
	return cnt==0 ? -1 : 1;
}

static int remove_hf_f(struct sip_msg* msg, char* p1, char* foo)
{
	return remove_hf(msg, p1, /*use string comparison*/0);
}

static int remove_hf_re_f(struct sip_msg* msg, char* p1, char* foo)
{
	return remove_hf(msg, p1, /*use regexp comparison*/1);
}

static int is_present_hf_f(struct sip_msg* msg, char* p1, char* foo)
{
	struct hdr_field *hf;
	str hfn;

	if (get_str_fparam(&hfn, msg, (fparam_t*)p1) < 0) {
	    ERR("is_present_hf: Error while obtaining parameter value\n");
	    return -1;
	}

	/* we need to be sure we have seen all HFs */
	parse_headers(msg, HDR_EOH_F, 0);
	for (hf=msg->headers; hf; hf=hf->next) {
		if (hf->name.len!=hfn.len)
			continue;
		if (strncasecmp(hf->name.s, hfn.s, hf->name.len)!=0)
			continue;
		return 1;
	}
	return -1;
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

	now=ser_time(0);

	bd_time=gmtime(&now);
	if (bd_time==NULL) {
		LOG(L_ERR, "ERROR: append_time: gmtime failed\n");
		return -1;
	}

	len=strftime(time_str, MAX_TIME, TIME_FORMAT, bd_time);
	if (len>MAX_TIME-2 || len==0) {
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

static int append_to_reply_f(struct sip_msg* msg, char* _str, char* dummy)
{
	str str;
	if (eval_xlstr(msg, (void*) _str, &str) < 0) return -1;

	if ( add_lump_rpl( msg, str.s, str.len, LUMP_RPL_HDR)==0 )
	{
		LOG(L_ERR,"ERROR:append_to_reply : unable to add lump_rl\n");
		return -1;
	}

	return 1;
}


/* add str1 to end of header or str1.r-uri.str2 */

static int append_hf_helper(struct sip_msg* msg, struct xlstr *_str1, struct xlstr *_str2)
{
	struct lump* anchor;
	char *s;
	str str1, str2;
	int len;

	if (eval_xlstr(msg, _str1, &str1) < 0) return -1;
	if (_str2) {
		if (eval_xlstr(msg, _str2, &str2) < 0) return -1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LOG(L_ERR, "append_hf(): Error while parsing message\n");
		return -1;
	}

	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if (anchor == 0) {
		LOG(L_ERR, "append_hf(): Can't get anchor\n");
		return -1;
	}

	len=str1.len;
	if (_str2) len+= str2.len + REQ_LINE(msg).uri.len;

	s = (char*)pkg_malloc(len);
	if (!s) {
		LOG(L_ERR, "append_hf(): No memory left\n");
		return -1;
	}

	memcpy(s, str1.s, str1.len);
	if (_str2) {
		memcpy(s+str1.len, REQ_LINE(msg).uri.s, REQ_LINE(msg).uri.len);
		memcpy(s+str1.len+REQ_LINE(msg).uri.len, str2.s, str2.len );
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
	return append_hf_helper(msg, (struct xlstr *) str1, (struct xlstr *) 0);
}

static int append_urihf(struct sip_msg *msg, char *str1, char *str2 )
{
	return append_hf_helper(msg, (struct xlstr *) str1, (struct xlstr *) str2);
}

#define HNF_ALL 0x01
#define HNF_IDX 0x02

#define MAX_HF_VALUE_STACK 10

enum {hnoInsert, hnoAppend, hnoAssign, hnoRemove, hnoInclude, hnoExclude, hnoIsIncluded, hnoGetValue, hnoGetValueUri, hnoGetValueName, hnoRemove2, hnoAssign2, hnoGetValue2};

struct hname_data {
	int oper;
	int htype;
	str hname;
	int flags;
	int idx;
	str param;
};

#define is_space(_p) ((_p) == '\t' || (_p) == '\n' || (_p) == '\r' || (_p) == ' ')

#define eat_spaces(_p) \
	while( is_space(*(_p)) ){\
	(_p)++;}

#define is_alphanum(_p) (((_p) >= 'a' && (_p) <= 'z') || ((_p) >= 'A' && (_p) <= 'Z') || ((_p) >= '0' && (_p) <= '9') || (_p) == '_' || (_p) == '-')

#define eat_while_alphanum(_p) \
	while ( is_alphanum(*(_p)) ) {\
		(_p)++; }

/* parse:  hname [ ([] | [*] | [number]) ] [ "." param ] */
static int fixup_hname_param(char *hname, struct hname_data** h) {
	struct hdr_field hdr;
	char *savep, savec;

	*h = pkg_malloc(sizeof(**h));
	if (!*h) return E_OUT_OF_MEM;
	memset(*h, 0, sizeof(**h));

	memset(&hdr, 0, sizeof(hdr));
	eat_spaces(hname);
	(*h)->hname.s = hname;
	savep = hname;
	eat_while_alphanum(hname);
	(*h)->hname.len = hname - (*h)->hname.s;
	savec = *hname;
	*hname = ':';
	parse_hname2((*h)->hname.s, (*h)->hname.s+(*h)->hname.len+3, &hdr);
	*hname = savec;

	if (hdr.type == HDR_ERROR_T) goto err;
	(*h)->htype = hdr.type;

	eat_spaces(hname);
	savep = hname;
	if (*hname == '[') {
		hname++;
		eat_spaces(hname);
		savep = hname;
		(*h)->flags |= HNF_IDX;
		if (*hname == '*') {
			(*h)->flags |= HNF_ALL;
			hname++;
		}
		else if (*hname != ']') {
			char* c;
			(*h)->idx = strtol(hname, &c, 10);
			if (hname == c) goto err;
			hname = c;
		}
		eat_spaces(hname);
		savep = hname;
		if (*hname != ']') goto err;
		hname++;
	}
	eat_spaces(hname);
	savep = hname;
	if (*hname == '.') {
		hname++;
		eat_spaces(hname);
		savep = hname;
		(*h)->param.s = hname;
		eat_while_alphanum(hname);
		(*h)->param.len = hname-(*h)->param.s;
		if ((*h)->param.len == 0) goto err;
	}
	else {
		(*h)->param.s = hname;
	}
	savep = hname;
	if (*hname != '\0') goto err;
	(*h)->hname.s[(*h)->hname.len] = '\0';
	(*h)->param.s[(*h)->param.len] = '\0';
	return 0;
err:
	pkg_free(*h);
	LOG(L_ERR, "ERROR: textops: cannot parse header near '%s'\n", savep);
	return E_CFG;
}

static int fixup_hname_str(void** param, int param_no) {
	if (param_no == 1) {
		struct hname_data* h;
		int res = fixup_hname_param(*param, &h);
		if (res < 0) return res;
		*param = h;
	}
	else if (param_no == 2) {
		return fixup_xlstr(param, param_no);
	}
	return 0;
}

static int find_next_hf(struct sip_msg* msg, struct hname_data* hname, struct hdr_field** hf) {
	if (!*hf) {
		if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
			LOG(L_ERR, "ERROR: textops: find_next_hf: Error while parsing message\n");
			return -1;
		}
		*hf = msg->headers;
	}
	else {
		*hf = (*hf)->next;
	}
	for (; *hf; *hf = (*hf)->next) {
		if (hname->htype == HDR_OTHER_T) {
			if ((*hf)->name.len==hname->hname.len && strncasecmp((*hf)->name.s, hname->hname.s, (*hf)->name.len)==0)
				return 1;
		}
		else if (hname->htype == (*hf)->type) {
			return 1;
		}
	}
	return 0;
}

static int find_next_value(char** start, char* end, str* val, str* lump_val) {
	int quoted = 0;
	lump_val->s = *start;
	while (*start < end && is_space(**start) ) (*start)++;
	val->s = *start;
	while ( *start < end && (**start != ',' || quoted) ) {
		if (**start == '\"' && (!quoted || (*start)[-1]!='\\') )
			quoted = ~quoted;
		(*start)++;
	}
	val->len = *start - val->s;
	while (val->len > 0 && is_space(val->s[val->len-1])) val->len--;
/* we cannot automatically strip quotes!!! an example why: "name" <sip:ssss>;param="bar" 
	if (val->len >= 2 && val->s[0] == '\"' && val->s[val->len-1] == '\"') {
		val->s++;
		val->len -= 2;
	}
*/
	while (*start < end && **start != ',') (*start)++;
	if (*start < end) {
		(*start)++;
	}
	lump_val->len = *start - lump_val->s;
	return (*start < end);
}

static void adjust_lump_val_for_delete(struct hdr_field* hf, str* lump_val) {
	if ( lump_val->s+lump_val->len == hf->body.s+hf->body.len ) {
		if (lump_val->s > hf->body.s) {
		/* in case if is it last value in header save position of last delimiter to remove it with rightmost value */
			lump_val->s--;
			lump_val->len++;
		}
	}
}

static int find_hf_value_idx(struct sip_msg* msg, struct hname_data* hname, struct hdr_field** hf, str* val, str* lump_val) {
	int res;
	char *p;
	if ( hname->flags & HNF_ALL || hname->idx == 0) return -1;
	*hf = 0;
	if (hname->idx > 0) {
		int idx;
		idx = hname->idx;
		do {
			res = find_next_hf(msg, hname, hf);
			if (res < 0) return -1;
			if (*hf) {
				if (val) {
					lump_val->len = 0;
					p = (*hf)->body.s;
					do {
						res = find_next_value(&p, (*hf)->body.s+(*hf)->body.len, val, lump_val);
						idx--;
					} while (res && idx);
				}
				else {
					idx--;
				}
			}
		} while (*hf && idx);
	}
	else if (hname->idx < 0) {  /* search from the bottom */
		struct hf_value_stack {
			str val, lump_val;
			struct hdr_field* hf;
		} stack[MAX_HF_VALUE_STACK];
		int stack_pos, stack_num;

		if ( -hname->idx > MAX_HF_VALUE_STACK ) return -1;
		stack_pos = stack_num = 0;
		do {
			res = find_next_hf(msg, hname, hf);
			if (res < 0) return -1;
			if (*hf) {
				stack[stack_pos].lump_val.len = 0;
				p = (*hf)->body.s;
				do {
					stack[stack_pos].hf = *hf;
					if (val)
						res = find_next_value(&p, (*hf)->body.s+(*hf)->body.len, &stack[stack_pos].val, &stack[stack_pos].lump_val);
					else
						res = 0;
					stack_pos++;
					if (stack_pos >= MAX_HF_VALUE_STACK)
						stack_pos = 0;
					if (stack_num < MAX_HF_VALUE_STACK)
						stack_num++;

				} while (res);
			}
		} while (*hf);

		if (-hname->idx <= stack_num) {
			stack_pos += hname->idx;
			if (stack_pos < 0)
				stack_pos += MAX_HF_VALUE_STACK;
			*hf = stack[stack_pos].hf;
			if (val) {
				*val = stack[stack_pos].val;
				*lump_val = stack[stack_pos].lump_val;
			}
		}
		else {
			*hf = 0;
		}
	}
	else
		return -1;
	return *hf?1:0;
}

static int find_hf_value_param(struct hname_data* hname, str* param_area, str* value, str* lump_upd, str* lump_del) {
	int i, j, found;

	i = 0;
	while (1) {
		lump_del->s = param_area->s + i;
		for (; i < param_area->len && is_space(param_area->s[i]); i++);
		if (i < param_area->len && param_area->s[i] == ';') {	/* found a param ? */
			i++;
			for (; i < param_area->len && is_space(param_area->s[i]); i++);
			j = i;
			for (; i < param_area->len && !is_space(param_area->s[i]) && param_area->s[i]!='=' && param_area->s[i]!=';'; i++);

			found = hname->param.len == i-j && !strncasecmp(hname->param.s, param_area->s+j, i-j);
			lump_upd->s = param_area->s+i;
			value->s = param_area->s+i;
			value->len = 0;
			for (; i < param_area->len && is_space(param_area->s[i]); i++);
			if (i < param_area->len && param_area->s[i]=='=') {
				i++;
				for (; i < param_area->len && is_space(param_area->s[i]); i++);
				value->s = param_area->s+i;
				if (i < param_area->len) {
					if (param_area->s[i]=='\"') {
						i++;
						value->s++;
						for (; i<param_area->len; i++) {
							if (param_area->s[i]=='\"') {
								i++;
								break;
							}
							value->len++;
						}
					}
					else {
						for (; i<param_area->len && !is_space(param_area->s[i]) && param_area->s[i]!=';'; i++, value->len++);
					}
				}
			}
			if (found) {
				lump_del->len = param_area->s+i - lump_del->s;
				lump_upd->len = param_area->s+i - lump_upd->s;
				return 1;
			}
		}
		else { /* not found, return last correct position, should be end of param area */
			lump_del->len = 0;
			return 0;
		}
	}
}

/* parse:  something param_name=param_value something [ "," something param_name="param_value" ....]
 * 'something' is required by Authenticate
 */
static int find_hf_value2_param(struct hname_data* hname, str* param_area, str* value, str* lump_upd, str* lump_del, char* delim) {
	int i, j, k, found, comma_flag;

	i = 0;
	*delim = 0;
	lump_del->len = 0;
	while (i < param_area->len) {

		lump_del->s = param_area->s + i;
		while (i<param_area->len && is_space(param_area->s[i])) i++;
		comma_flag = i < param_area->len && param_area->s[i] == ',';
		if (comma_flag) i++;
		while (i<param_area->len && is_space(param_area->s[i])) i++;

		if (i < param_area->len && is_alphanum(param_area->s[i])) {	/* found a param name ? */
			j = i;
			if (!*delim) *delim = ' ';
			while (i<param_area->len && is_alphanum(param_area->s[i])) i++;

			k = i;
			while (i<param_area->len && is_space(param_area->s[i])) i++;
			lump_upd->s = param_area->s + i;
			if (i < param_area->len && param_area->s[i] == '=') {	/* if equal then it's the param */
				*delim = ',';
				i++;
				found = hname->param.len == k-j && !strncasecmp(hname->param.s, param_area->s+j, k-j);
				while (i<param_area->len && is_space(param_area->s[i])) i++;

				value->s = param_area->s+i;
				value->len = 0;
				if (i < param_area->len) {
					if (param_area->s[i]=='\"') {
						i++;
						value->s++;
						for (; i<param_area->len; i++) {
							if (param_area->s[i]=='\"') {
								i++;
								break;
							}
							value->len++;
						}
					}
					else {
						for (; i<param_area->len && !is_space(param_area->s[i]) && param_area->s[i]!=','; i++, value->len++);
					}
				}
				if (found) {
					lump_upd->len = param_area->s+i - lump_upd->s;
					lump_del->len = param_area->s+i - lump_del->s;

					while (i<param_area->len && is_space(param_area->s[i])) i++;

					if (!comma_flag && i < param_area->len && param_area->s[i]==',') {
						i++;
						lump_del->len = param_area->s+i - lump_del->s;
					}
					return 1;
				}
			}
			while (i<param_area->len && is_space(param_area->s[i])) i++;
		}
		else {
			while (i<param_area->len && !is_space(param_area->s[i]) && !param_area->s[i]!=',') i++;
		}
	}
	lump_del->s = param_area->s + i;
	return 0;
}

static int insert_header_lump(struct sip_msg* msg, char* msg_position, int lump_before, str* hname, str *val) {
	struct lump* anchor;
	char *s;
	int len;

	anchor = anchor_lump(msg, msg_position - msg->buf, 0, 0);
	if (anchor == 0) {
		LOG(L_ERR, "ERROR: textops: insert_header_lump(): Can't get anchor\n");
		return -1;
	}

	len=hname->len+2+val->len+2;

	s = (char*)pkg_malloc(len);
	if (!s) {
		LOG(L_ERR, "ERROR: textops: insert_header_lump(): not enough memory\n");
		return -1;
	}

	memcpy(s, hname->s, hname->len);
	s[hname->len] = ':';
	s[hname->len+1] = ' ';
	memcpy(s+hname->len+2, val->s, val->len);
	s[hname->len+2+val->len] = '\r';
	s[hname->len+2+val->len+1] = '\n';

	if ( (lump_before?insert_new_lump_before(anchor, s, len, 0):insert_new_lump_after(anchor, s, len, 0)) == 0) {
		LOG(L_ERR, "ERROR: textops: insert_header_lump(): Can't insert lump\n");
		pkg_free(s);
		return -1;
	}
	return 1;
}

static int insert_value_lump(struct sip_msg* msg, struct hdr_field* hf, char* msg_position, int lump_before, str *val) {
	struct lump* anchor;
	char *s;
	int len;

	anchor = anchor_lump(msg, msg_position - msg->buf, 0, 0);
	if (anchor == 0) {
		LOG(L_ERR, "ERROR: textops: insert_value_lump(): Can't get anchor\n");
		return -1;
	}

	len=val->len+1;

	s = (char*)pkg_malloc(len);
	if (!s) {
		LOG(L_ERR, "ERROR: textops: insert_value_lump(): not enough memory\n");
		return -1;
	}

	if (!hf) {
		memcpy(s, val->s, val->len);
		len--;
	}
	else if (msg_position == hf->body.s+hf->body.len) {
		s[0] = ',';
		memcpy(s+1, val->s, val->len);
	}
	else {
		memcpy(s, val->s, val->len);
		s[val->len] = ',';
	}
	if ( (lump_before?insert_new_lump_before(anchor, s, len, 0):insert_new_lump_after(anchor, s, len, 0)) == 0) {
		LOG(L_ERR, "ERROR: textops: insert_value_lump(): Can't insert lump\n");
		pkg_free(s);
		return -1;
	}
	return 1;
}

static int delete_value_lump(struct sip_msg* msg, struct hdr_field* hf, str *val) {
	struct lump* l;
	/* TODO: check already existing lumps */
	if (hf && val->s == hf->body.s && val->len == hf->body.len) 	/* check if remove whole haeder? */
		l=del_lump(msg, hf->name.s-msg->buf, hf->len, 0);
	else
		l=del_lump(msg, val->s-msg->buf, val->len, 0);
	if (l==0) {
		LOG(L_ERR, "ERROR: textops: delete_value_lump: not enough memory\n");
		return -1;
	}
	return 1;
}

static int incexc_hf_value_f(struct sip_msg* msg, char* _hname, char* _val) {
	struct hname_data* hname = (void*) _hname;
	struct hdr_field* hf, *lump_hf;
	str val, hval1, hval2;
	char *p;
	int res = eval_xlstr(msg, (void*) _val, &val);
	if (res < 0) return res;
	if (!val.len) return -1;
	hf = 0;
	lump_hf = 0;
	while (1) {
		if (find_next_hf(msg, hname, &hf) < 0) return -1;
		if (!hf) break;
		hval2.len = 0;
		p = hf->body.s;
		do {
			res = find_next_value(&p, hf->body.s+hf->body.len, &hval1, &hval2);
			if (hval1.len && val.len == hval1.len && strncasecmp(val.s, hval1.s, val.len) == 0) {
				switch (hname->oper) {
					case hnoIsIncluded:
					case hnoInclude:
						return 1;
					case hnoExclude:
						adjust_lump_val_for_delete(hf, &hval2);
						delete_value_lump(msg, hf, &hval2);
					default:
						break;
				}
			}
		} while (res);
		switch (hname->oper) {
			case hnoInclude:
				if (!lump_hf) {
					lump_hf = hf;
				}
				break;
			default:
				break;
		}
	}
	switch (hname->oper) {
		case hnoIsIncluded:
			return -1;
		case hnoInclude:
			if (lump_hf)
				return insert_value_lump(msg, lump_hf, lump_hf->body.s+lump_hf->body.len, 1, &val);
			else
				return insert_header_lump(msg, msg->unparsed, 1, &hname->hname, &val);
		default:
			return 1;
	}
}

#define INCEXC_HF_VALUE_FIXUP(_func,_oper) \
static int _func (void** param, int param_no) {\
	char* p = *param; \
	int res=fixup_hname_str(param, param_no); \
	if (res < 0) return res; \
	if (param_no == 1) {\
		if ( ((struct hname_data*)*param)->flags & HNF_IDX || ((struct hname_data*)*param)->param.len ) { \
			LOG(L_ERR, "ERROR: textops: neither index nor param may be specified in '%s'\n", p);\
			return E_CFG;\
		}\
		((struct hname_data*)*param)->oper = _oper;\
	}\
	return 0;\
}

INCEXC_HF_VALUE_FIXUP(include_hf_value_fixup, hnoInclude)
INCEXC_HF_VALUE_FIXUP(exclude_hf_value_fixup, hnoExclude)
INCEXC_HF_VALUE_FIXUP(hf_value_exists_fixup, hnoIsIncluded)

static void get_uri_and_skip_until_params(str *param_area, str *name, str *uri) {
	int i, quoted, uri_pos, uri_done;

	name->len = 0;
	uri->len = 0;
	uri_done = 0;
        name->s = param_area->s;
	for (i=0; i<param_area->len && param_area->s[i]!=';'; ) {	/* [ *(token LSW)/quoted-string ] "<" addr-spec ">" | addr-spec */
		/* skip name */
		for (quoted=0, uri_pos=i; i<param_area->len; i++) {
			if (!quoted) {
				if (param_area->s[i] == '\"') {
					quoted = 1;
					uri_pos = -1;
				}
				else if (param_area->s[i] == '<' || param_area->s[i] == ';' || is_space(param_area->s[i])) break;
			}
			else if (param_area->s[i] == '\"' && param_area->s[i-1] != '\\') quoted = 0;
		}
		if (!name->len)
			name->len = param_area->s+i-name->s;
		if (uri_pos >= 0 && !uri_done) {
			uri->s = param_area->s+uri_pos;
			uri->len = param_area->s+i-uri->s;
		}
		/* skip uri */
		while (i<param_area->len && is_space(param_area->s[i])) i++;
		if (i<param_area->len && param_area->s[i]=='<') {
			uri->s = param_area->s+i;
			uri->len = 0;
			for (quoted=0; i<param_area->len; i++) {
				if (!quoted) {
					if (param_area->s[i] == '\"') quoted = 1;
					else if (param_area->s[i] == '>') {
						uri->len = param_area->s+i-uri->s+1;
						uri_done = 1;
						break;
					}
				}
				else if (param_area->s[i] == '\"' && param_area->s[i-1] != '\\') quoted = 0;
			}
		}
	}
        param_area->s+= i;
	param_area->len-= i;
	if (uri->s == name->s)
		name->len = 0;
}

static int assign_hf_do_lumping(struct sip_msg* msg,struct hdr_field* hf, struct hname_data* hname, str* value, int upd_del_fl, str* lump_upd, str* lump_del, char delim) {
	int len, i;
	char *s;
	struct lump* anchor;

	if (upd_del_fl) {
		len = value?lump_upd->len:lump_del->len;
		if (len > 0) {
			if (!del_lump(msg, (value?lump_upd->s:lump_del->s)-msg->buf, len, 0)) {
				LOG(L_ERR, "ERROR: textops: assign_hf_do_lumping: not enough memory\n");
				return -1;
			}
		}
		if (value && value->len) {
			anchor = anchor_lump(msg, lump_upd->s - msg->buf, 0, 0);
			if (anchor == 0) {
				LOG(L_ERR, "ERROR: textops: assign_hf_do_lumping: Can't get anchor\n");
				return -1;
			}

			len = 1+value->len;
			s = pkg_malloc(len);
			if (!s) {
				LOG(L_ERR, "ERROR: textops: assign_hf_do_lumping: not enough memory\n");
				return -1;
			}
			s[0]='=';
			memcpy(s+1, value->s, value->len);
			if ( (insert_new_lump_before(anchor, s, len, 0)) == 0) {
				LOG(L_ERR, "ERROR: textops: assign_hf_do_lumping: Can't insert lump\n");
				pkg_free(s);
				return -1;
			}
		}
	}
	else {
		if (!value) return -1;

		anchor = anchor_lump(msg, lump_del->s - msg->buf, 0, 0);
		if (anchor == 0) {
			LOG(L_ERR, "ERROR: textops: assign_hf_do_lumping: Can't get anchor\n");
			return -1;
		}

		len = 1+hname->param.len+(value->len?value->len+1:0);
		s = pkg_malloc(len);
		if (!s) {
			LOG(L_ERR, "ERROR: textops: assign_hf_do_lumping: not enough memory\n");
			return -1;
		}
		if (delim) {
			s[0] = delim;
			i = 1;
		}
		else {
			i = 0;
			len--;
		}
		memcpy(s+i, hname->param.s, hname->param.len);
		if (value->len) {
			s[hname->param.len+i]='=';
			memcpy(s+i+hname->param.len+1, value->s, value->len);
		}

		if ( (insert_new_lump_before(anchor, s, len, 0)) == 0) {
			LOG(L_ERR, "ERROR: textops: assign_hf_do_lumping: Can't insert lump\n");
			pkg_free(s);
			return -1;
		}
	}
	return 1;
}


static int assign_hf_process_params(struct sip_msg* msg, struct hdr_field* hf, struct hname_data* hname, str* value, str* value_area) {
	int r, r2, res=0;
	str param_area, lump_upd, lump_del, dummy_val, dummy_name, dummy_uri;
	param_area = *value_area;
	get_uri_and_skip_until_params(&param_area, &dummy_name, &dummy_uri);
	do {
		r = find_hf_value_param(hname, &param_area, &dummy_val, &lump_upd, &lump_del);
		r2 = assign_hf_do_lumping(msg, hf, hname, value, r, &lump_upd, &lump_del, ';');
		if (res == 0)
			res = r2;
		if (r && !value) {   /* remove all parameters */
			param_area.len -= lump_del.s+lump_del.len-param_area.s;
			param_area.s = lump_del.s+lump_del.len;
		}
	} while (!value && r);
	return res;
}

static int assign_hf_process2_params(struct sip_msg* msg, struct hdr_field* hf, struct hname_data* hname, str* value) {
	int r, r2, res = 0;
	str param_area, lump_upd, lump_del, dummy_val;
	char delim;

	param_area = hf->body;

	do {
		r = find_hf_value2_param(hname, &param_area, &dummy_val, &lump_upd, &lump_del, &delim);
		r2 = assign_hf_do_lumping(msg, hf, hname, value, r, &lump_upd, &lump_del, delim);
		if (res == 0)
			res = r2;
		if (r && !value) {   /* remove all parameters */
			param_area.len -= lump_del.s+lump_del.len-param_area.s;
			param_area.s = lump_del.s+lump_del.len;
		}
	} while (!value && r);
	return res;

}

static int insupddel_hf_value_f(struct sip_msg* msg, char* _hname, char* _val) {
	struct hname_data* hname = (void*) _hname;
	struct hdr_field* hf;
	str val, hval1, hval2;
	int res;

	if (_val) {
		res = eval_xlstr(msg, (void*) _val, &val);
		if (res < 0) return res;
	}
	switch (hname->oper) {
		case hnoAppend:
			if ((hname->flags & HNF_IDX) == 0) {
				if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
					LOG(L_ERR, "ERROR: textops: Error while parsing message\n");
					return -1;
				}
				return insert_header_lump(msg, msg->unparsed, 1, &hname->hname, &val);
			}
			else {
				res = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if (res < 0) return res;
				if (hf) {
					return insert_value_lump(msg, hf, hval2.s+hval2.len, res /* insert after, except it is last value in header */, &val);
				}
				else {
					return insert_header_lump(msg, msg->unparsed, 1, &hname->hname, &val);
				}
			}
		case hnoInsert:
			/* if !HNF_IDX is possible parse only until first hname header but not trivial for HDR_OTHER_T header, not implemented */
			res = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
			if (res < 0) return res;
			if (hf && (hname->flags & HNF_IDX) == 0) {
				return insert_header_lump(msg, hf->name.s, 1, &hname->hname, &val);
			}
			else if (!hf && hname->idx == 1) {
				return insert_header_lump(msg, msg->unparsed, 1, &hname->hname, &val);
			}
			else if (hf) {
				return insert_value_lump(msg, hf, hval2.s, 1, &val);
			}
			else
				return -1;

		case hnoRemove:
		case hnoAssign:
			if (hname->flags & HNF_ALL) {
				struct hdr_field* hf = 0;
				int fl = -1;
				do {
					res = find_next_hf(msg, hname, &hf);
					if (res < 0) return res;
					if (hf) {
						if (!hname->param.len) {
							fl = 1;
							delete_value_lump(msg, hf, &hf->body);
						}
						else {
							char *p;
							hval2.len = 0;
							p = hf->body.s;
							do {
								res = find_next_value(&p, hf->body.s+hf->body.len, &hval1, &hval2);
								if (assign_hf_process_params(msg, hf, hname, _val?&val:0, &hval1) > 0)
									fl = 1;
							} while (res);
						}
					}
				} while (hf);
				return fl;
			}
			else {
				res = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if (res < 0) return res;
				if (hf) {
					if (!hname->param.len) {
						if (hname->oper == hnoRemove) {
							adjust_lump_val_for_delete(hf, &hval2);
							return delete_value_lump(msg, hf, &hval2);
						}
						else {
							res = delete_value_lump(msg, 0 /* delete only value part */, &hval1);
							if (res < 0) return res;
							if (val.len) {
								return insert_value_lump(msg, 0 /* do not add delims */, hval1.s, 1, &val);
							}
							return 1;
						}
					}
					else {
						return assign_hf_process_params(msg, hf, hname, _val?&val:0, &hval1);
					}
				}
			}
			break;
		case hnoRemove2:
		case hnoAssign2:
			if (hname->flags & HNF_ALL) {
				struct hdr_field* hf = 0;
				int fl = -1;
				do {
					res = find_next_hf(msg, hname, &hf);
					if (res < 0) return res;
					if (hf) {
						if (!hname->param.len) {  /* the same as hnoRemove/hnoAssign */
							fl = 1;
							delete_value_lump(msg, hf, &hf->body);
						}
						else {

							if (assign_hf_process2_params(msg, hf, hname, _val?&val:0) > 0)
								fl = 1;
						}
					}
				} while (hf);
				return fl;
			}
			else {
				res = find_hf_value_idx(msg, hname, &hf, 0, 0);
				if (res < 0) return res;
				if (hf) {
					if (!hname->param.len) {
						if (hname->oper == hnoRemove2) {
							return delete_value_lump(msg, hf, &hf->body);
						}
						else {
							res = delete_value_lump(msg, 0 /* delete only value part */, &hf->body);
							if (res < 0) return res;
							if (val.len) {
								return insert_value_lump(msg, 0 /* do not add delims */, hf->body.s, 1, &val);
							}
							return 1;
						}
					}
					else {
						return assign_hf_process2_params(msg, hf, hname, _val?&val:0);
					}
				}
			}
			break;
	}
	return -1;
}

static int append_hf_value_fixup(void** param, int param_no) {
	int res=fixup_hname_str(param, param_no);
	if (res < 0) return res;
	if (param_no == 1) {
		if ( ((struct hname_data*)*param)->flags & HNF_ALL ) {
			LOG(L_ERR, "ERROR: textops: asterisk not supported\n");
			return E_CFG;
		} else if ( (((struct hname_data*)*param)->flags & HNF_IDX) == 0 || !((struct hname_data*)*param)->idx ) {
			((struct hname_data*)*param)->idx = -1;
		}
		if (((struct hname_data*)*param)->idx < -MAX_HF_VALUE_STACK) {
			LOG(L_ERR, "ERROR: textops: index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
			return E_CFG;
		}
		if ( ((struct hname_data*)*param)->param.len ) {
			LOG(L_ERR, "ERROR: textops: param not supported\n");
			return E_CFG;
		}
		((struct hname_data*)*param)->oper = hnoAppend;
	}
	return 0;
}

static int insert_hf_value_fixup(void** param, int param_no) {
	int res=fixup_hname_str(param, param_no);
	if (res < 0) return res;
	if (param_no == 1) {
		if ( ((struct hname_data*)*param)->flags & HNF_ALL ) {
			LOG(L_ERR, "ERROR: textops: asterisk not supported\n");
			return E_CFG;
		} else if ( (((struct hname_data*)*param)->flags & HNF_IDX) == 0 || !((struct hname_data*)*param)->idx ) {
			((struct hname_data*)*param)->idx = 1;
		}
		if (((struct hname_data*)*param)->idx < -MAX_HF_VALUE_STACK) {
			LOG(L_ERR, "ERROR: textops: index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
			return E_CFG;
		}
		if ( ((struct hname_data*)*param)->param.len ) {
			LOG(L_ERR, "ERROR: textops: param not supported\n");
			return E_CFG;
		}
		((struct hname_data*)*param)->oper = hnoInsert;
	}
	return 0;
}

static int remove_hf_value_fixup(void** param, int param_no) {
	int res=fixup_hname_str(param, param_no);
	if (res < 0) return res;
	if (param_no == 1) {
		if ( (((struct hname_data*)*param)->flags & HNF_IDX) == 0 || !((struct hname_data*)*param)->idx ) {
			((struct hname_data*)*param)->idx = 1;
			((struct hname_data*)*param)->flags |= HNF_IDX;
		}
		if (((struct hname_data*)*param)->idx < -MAX_HF_VALUE_STACK) {
			LOG(L_ERR, "ERROR: textops: index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
			return E_CFG;
		}
		((struct hname_data*)*param)->oper = hnoRemove;
	}
	return 0;
}

static int assign_hf_value_fixup(void** param, int param_no) {
	int res=fixup_hname_str(param, param_no);
	if (res < 0) return res;
	if (param_no == 1) {
		if ( (((struct hname_data*)*param)->flags & HNF_ALL) && !((struct hname_data*)*param)->param.len) {
			LOG(L_ERR, "ERROR: textops: asterisk not supported without param\n");
			return E_CFG;
		} else if ( (((struct hname_data*)*param)->flags & HNF_IDX) == 0 || !((struct hname_data*)*param)->idx ) {
			((struct hname_data*)*param)->idx = 1;
			((struct hname_data*)*param)->flags |= HNF_IDX;
		}
		if (((struct hname_data*)*param)->idx < -MAX_HF_VALUE_STACK) {
			LOG(L_ERR, "ERROR: textops: index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
			return E_CFG;
		}
		((struct hname_data*)*param)->oper = hnoAssign;
	}
	return 0;
}

static int remove_hf_value2_fixup(void** param, int param_no) {
	int res=remove_hf_value_fixup(param, param_no);
	if (res < 0) return res;
	if (param_no == 1) {
		((struct hname_data*)*param)->oper = hnoRemove2;
	}
	return 0;
}

static int assign_hf_value2_fixup(void** param, int param_no) {
	int res=assign_hf_value_fixup(param, param_no);
	if (res < 0) return res;
	if (param_no == 1) {
		((struct hname_data*)*param)->oper = hnoAssign2;
	}
	return 0;
}

static int sel_hf_value(str* res, select_t* s, struct sip_msg* msg) {  /* dummy */
	return 0;
}

#define _ALLOC_INC_SIZE 1024

static int sel_hf_value_name(str* res, select_t* s, struct sip_msg* msg) {
	struct hname_data* hname;
	struct hdr_field* hf;
	str val, hval1, hval2, huri, dummy_name;
	int r;
	if (!msg) {
		struct hdr_field hdr;
		char buf[50];
		int i, n;

		if (s->params[1].type == SEL_PARAM_STR) {
			hname = pkg_malloc(sizeof(*hname));
			if (!hname) return E_OUT_OF_MEM;
			memset(hname, 0, sizeof(*hname));

			for (i=s->params[1].v.s.len-1; i>0; i--) {
				if (s->params[1].v.s.s[i]=='_')
					s->params[1].v.s.s[i]='-';
			}
			i = snprintf(buf, sizeof(buf)-1, "%.*s: X\n", s->params[1].v.s.len, s->params[1].v.s.s);
			buf[i] = 0;

			hname->hname = s->params[1].v.s;
			parse_hname2(buf, buf+i, &hdr);

			if (hdr.type == HDR_ERROR_T) return E_CFG;
			hname->htype = hdr.type;

			s->params[1].v.p = hname;
			s->params[1].type = SEL_PARAM_PTR;
		}
		else {
			hname = s->params[1].v.p;
		}
		n = s->param_offset[select_level+1] - s->param_offset[select_level];  /* number of values before NESTED */
		if (n > 2 && s->params[2].type == SEL_PARAM_INT) {
			hname->idx = s->params[2].v.i;
			hname->flags |= HNF_IDX;
			if (hname->idx < -MAX_HF_VALUE_STACK) {
				LOG(L_ERR, "ERROR: textops: index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
				return E_CFG;
			}
			if (hname->idx == 0)
				hname->idx = 1;
			i = 3;
		}
		else {
			i = 2;
			hname->idx = 1;
		}
		if (n > i && s->params[i].type == SEL_PARAM_STR) {
			hname->param = s->params[i].v.s;
			for (i=hname->param.len-1; i>0; i--) {
				if (hname->param.s[i]=='_')
					hname->param.s[i]='-';
			}

		}
		s->params[1].v.p = hname;
		s->params[1].type = SEL_PARAM_PTR;
		hname->oper = hnoGetValue;

		return 0;
	}

	res->len = 0;
	res->s = 0;
	hname = s->params[1].v.p;

	switch (hname->oper) {
		case hnoGetValueUri:
			if (hname->flags & HNF_ALL || (hname->flags & HNF_IDX) == 0) {
				char *buf = NULL;
				int buf_len = 0;
				
				hf = 0;
				do {
					r = find_next_hf(msg, hname, &hf);
					if (r < 0) break;
					if (hf) {
						char *p;
						str huri;
						hval2.len = 0;
						p = hf->body.s;
						do {
							r = find_next_value(&p, hf->body.s+hf->body.len, &hval1, &hval2);
							get_uri_and_skip_until_params(&hval1, &dummy_name, &huri);
							if (huri.len) {
							/* TODO: normalize uri, lowercase except quoted params, add/strip < > */
								if (*huri.s == '<') {
									huri.s++;
									huri.len -= 2;
								}
							}							
							if (res->len == 0) {  
								*res = huri; /* first value, if is also last value then we don't need any buffer */
							}
							else {
								if (buf) {
									if (res->len+huri.len+1 > buf_len) {
										buf_len = res->len+huri.len+1+_ALLOC_INC_SIZE;
										res->s = pkg_realloc(buf, buf_len);
										if (!res->s) {
											pkg_free(buf);
											LOG(L_ERR, "ERROR: textops: cannot realloc buffer\n");
											res->len = 0;
											return E_OUT_OF_MEM;
										}
										buf = res->s;
									}
								}
								else {
									/* 2nd value */
									buf_len = res->len+huri.len+1+_ALLOC_INC_SIZE;
									buf = pkg_malloc(buf_len);
									if (!buf) { 
										LOG(L_ERR, "ERROR: testops: out of memory\n");
										res->len = 0;
										return E_OUT_OF_MEM;
									}
									/* copy 1st value */
									memcpy(buf, res->s, res->len);								
									res->s = buf;
								}
								res->s[res->len] = ',';
								res->len++;
								if (huri.len) {
									memcpy(res->s+res->len, huri.s, huri.len);
									res->len += huri.len;
								}
							}
						
						} while (r);
					}
				} while (hf);
				if (buf) {
					res->s = get_static_buffer(res->len);
					if (!res->s) {
						pkg_free(buf);
						res->len = 0;
						LOG(L_ERR, "ERROR: testops: cannot allocate static buffer\n");
						return E_OUT_OF_MEM;
					}
					memcpy(res->s, buf, res->len);
					pkg_free(buf);
				}
			}
			else {
				r = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if (r > 0) {
					get_uri_and_skip_until_params(&hval1, &dummy_name, res);
					if (res->len && *res->s == '<') {
						res->s++;   	/* strip < & > */
						res->len-=2;
					}
				}
			}
			break;
		case hnoGetValueName:
			if ((hname->flags & HNF_ALL) == 0) {
				r = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if (r > 0) {
					get_uri_and_skip_until_params(&hval1, res, &dummy_name);
					if (res->len >= 2 && res->s[0] == '\"' && res->s[res->len-1]=='\"' ) {
						res->s++;   	/* strip quotes */
						res->len-=2;
					}
				}
			}
			break;
		case hnoGetValue:
			if (hname->flags & HNF_ALL || (hname->flags & HNF_IDX) == 0) {
				char *buf = NULL;
				int buf_len = 0;

				hf = 0;
				do {
					r = find_next_hf(msg, hname, &hf);
					
					if (r < 0) break;
					if (hf) {
						char *p;
						hval2.len = 0;
						p = hf->body.s;
						do {
							r = find_next_value(&p, hf->body.s+hf->body.len, &hval1, &hval2);
							if (res->len == 0) {  
								*res = hval1; /* first value, if is also last value then we don't need any buffer */
							}
							else {
								if (buf) {
									if (res->len+hval1.len+1 > buf_len) {
										buf_len = res->len+hval1.len+1+_ALLOC_INC_SIZE;
										res->s = pkg_realloc(buf, buf_len);
										if (!res->s) {
											pkg_free(buf);
											LOG(L_ERR, "ERROR: textops: cannot realloc buffer\n");
											res->len = 0;
											return E_OUT_OF_MEM;
										}
										buf = res->s;
									}
								}
								else {
									/* 2nd value */
									buf_len = res->len+hval1.len+1+_ALLOC_INC_SIZE;
									buf = pkg_malloc(buf_len);
									if (!buf) { 
										LOG(L_ERR, "ERROR: testops: out of memory\n");
										res->len = 0;
										return E_OUT_OF_MEM;
									}
									/* copy 1st value */
									memcpy(buf, res->s, res->len);								
									res->s = buf;
								}
								res->s[res->len] = ',';
								res->len++;
								if (hval1.len) {
									memcpy(res->s+res->len, hval1.s, hval1.len);
									res->len += hval1.len;
								}
							}
						} while (r);
					}
				} while (hf);
				if (buf) {
					res->s = get_static_buffer(res->len);
					if (!res->s) {
						pkg_free(buf);
						res->len = 0;
						LOG(L_ERR, "ERROR: testops: cannot allocate static buffer\n");
						return E_OUT_OF_MEM;
					}
					memcpy(res->s, buf, res->len);
					pkg_free(buf);
				}
			}
			else {
				r = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if (r > 0) {
					if (hname->param.len) {
						str d1, d2;
						get_uri_and_skip_until_params(&hval1, &dummy_name, &huri);
						if (find_hf_value_param(hname, &hval1, &val, &d1, &d2)) {
							*res = val;
						}
					}
					else {
						*res = hval1;
					}
				}
			}
			break;
		case hnoGetValue2:
			r = find_hf_value_idx(msg, hname, &hf, 0, 0);
			if (r > 0) {
				if (hname->param.len) {
					str d1, d2;
					char c;
					if (find_hf_value2_param(hname, &hf->body, &val, &d1, &d2, &c)) {
						*res = val;
					}
				}
				else {
					*res = hf->body;
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

static int sel_hf_value_name_param_name(str* res, select_t* s, struct sip_msg* msg) {
	return sel_hf_value_name(res, s, msg);
}

static int sel_hf_value_name_param_name2(str* res, select_t* s, struct sip_msg* msg) {
	if (!msg) { /* eliminate "param" level */
		int n;
		n = s->param_offset[select_level+1] - s->param_offset[select_level];
		s->params[n-2] = s->params[n-1];
	}
	return sel_hf_value_name(res, s, msg);
}

static int sel_hf_value_name_uri(str* res, select_t* s, struct sip_msg* msg) {
	int r;
	r = sel_hf_value_name(res, s, msg);
	if (!msg && r==0) {
		((struct hname_data*) s->params[1].v.p)->oper = hnoGetValueUri;
	}
	return r;
}

static int sel_hf_value_name_name(str* res, select_t* s, struct sip_msg* msg) {
	int r;
	r = sel_hf_value_name(res, s, msg);
	if (!msg && r==0) {
		((struct hname_data*) s->params[1].v.p)->oper = hnoGetValueName;
	}
	return r;
}

static int sel_hf_value_exists(str* res, select_t* s, struct sip_msg* msg) {  /* dummy */
	return 0;
}

static int sel_hf_value_exists_param(str* res, select_t* s, struct sip_msg* msg) {
	static char ret_val[] = "01";
	struct xlstr xlstr;
        int r;

	if (!msg) {
		r = sel_hf_value_name(res, s, msg);
		if (r == 0)
			((struct hname_data*) s->params[1].v.p)->oper = hnoIsIncluded;
		return r;
	}
	xlstr.s = s->params[2].v.s;
	xlstr.xlfmt = 0;
	r = incexc_hf_value_f(msg, s->params[1].v.p, (void*) &xlstr);
	res->s = &ret_val[r > 0];
	res->len = 1;

	return 0;
}

static int sel_hf_value2(str* res, select_t* s, struct sip_msg* msg) {  /* dummy */
	return 0;
}

static int sel_hf_value2_name(str* res, select_t* s, struct sip_msg* msg) {
	int r;
	r = sel_hf_value_name(res, s, msg);
	if (!msg && r==0) {
		((struct hname_data*) s->params[1].v.p)->oper = hnoGetValue2;
	}
	return r;
}

static int sel_hf_value2_name_param_name(str* res, select_t* s, struct sip_msg* msg) {
	return sel_hf_value2_name(res, s, msg);
}

SELECT_F(select_any_nameaddr)
SELECT_F(select_any_uri)
SELECT_F(select_anyheader_params)

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("hf_value"), sel_hf_value, SEL_PARAM_EXPECTED},

	{ sel_hf_value, SEL_PARAM_STR, STR_NULL, sel_hf_value_name, CONSUME_NEXT_INT | OPTIONAL | FIXUP_CALL},
	{ sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("param"), sel_hf_value_name_param_name2, CONSUME_NEXT_STR | FIXUP_CALL},
	{ sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("p"), sel_hf_value_name_param_name2, CONSUME_NEXT_STR | FIXUP_CALL},
	{ sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("uri"), sel_hf_value_name_uri, FIXUP_CALL},
	{ sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("name"), sel_hf_value_name_name, FIXUP_CALL},
	{ sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED | CONSUME_NEXT_STR}, /* it duplicates param,p,name,... */
	{ sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_anyheader_params, NESTED},

	{ sel_hf_value_name_uri, SEL_PARAM_INT, STR_NULL, select_any_uri, NESTED},
	{ sel_hf_value_name, SEL_PARAM_STR, STR_NULL, sel_hf_value_name_param_name, FIXUP_CALL},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("hf_value_exists"), sel_hf_value_exists, CONSUME_NEXT_STR | SEL_PARAM_EXPECTED},
	{ sel_hf_value_exists, SEL_PARAM_STR, STR_NULL, sel_hf_value_exists_param, FIXUP_CALL},

	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("hf_value2"), sel_hf_value2, SEL_PARAM_EXPECTED},
	{ sel_hf_value2, SEL_PARAM_STR, STR_NULL, sel_hf_value2_name, CONSUME_NEXT_INT | OPTIONAL | FIXUP_CALL},
	{ sel_hf_value2_name, SEL_PARAM_STR, STR_STATIC_INIT("params"), select_anyheader_params, NESTED},
	{ sel_hf_value2_name, SEL_PARAM_STR, STR_NULL, sel_hf_value2_name_param_name, FIXUP_CALL},
	{ sel_hf_value2_name_param_name, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED},
	{ sel_hf_value2_name_param_name, SEL_PARAM_STR, STR_STATIC_INIT("uri"), select_any_uri, NESTED},

	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};
