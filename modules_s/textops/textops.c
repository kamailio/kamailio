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
 * 2003-01-29: - rewriting actions (replace, search_append) now begin
 *               at the second line -- previously, they could affect
 *               first line too, which resulted in wrong calculation of
 *               forwarded requests and an error consequently
 *             - replace_all introduced
 * 2003-01-28 scratchpad removed (jiri)
 * 2003-01-18: append_urihf introduced (jiri)
 */




#include "../../comp_defs.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* for regex */
#include <regex.h>

static int search_f(struct sip_msg*, char*, char*);
static int replace_f(struct sip_msg*, char*, char*);
static int replace_all_f(struct sip_msg* msg, char* key, char* str);
static int search_append_f(struct sip_msg*, char*, char*);
static int append_to_reply_f(struct sip_msg* msg, char* key, char* str);
static int append_hf(struct sip_msg* msg, char* str1, char* str2);
static int append_urihf(struct sip_msg* msg, char* str1, char* str2);

static int fixup_regex(void**, int);
static int str_fixup(void** param, int param_no);

static int mod_init(void);


struct module_exports exports= {
	"textops",
	(char*[])	{
			"search",
			"search_append",
			"replace",
			"replace_all",
			"append_to_reply",
			"append_hf",
			"append_urihf"
	},
	(cmd_function[]) {
			search_f,
			search_append_f,
			replace_f,
			replace_all_f,
			append_to_reply_f,
			append_hf,
			append_urihf
	},
	(int[]) {
			1,
			2,
			2,
			2,
			1,
			1,
			2
	},
	(fixup_function[]){
			fixup_regex,
			fixup_regex,
			fixup_regex,
			fixup_regex,
			0,
			str_fixup,
			str_fixup
	},
	7,

	0,      /* Module parameter names */
	0,      /* Module parameter types */
	0,      /* Module parameter variable pointers */
	0,      /* Number of module paramers */

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
#ifdef SCRATCH
	return msg->orig+msg->first_line.len;
#else
	return msg->buf+msg->first_line.len;
#endif
}



static int search_f(struct sip_msg* msg, char* key, char* str2)
{
	/*we registered only 1 param, so we ignore str2*/
	regmatch_t pmatch;

#ifdef SCRATCH
	if (regexec((regex_t*) key, msg->orig, 1, &pmatch, 0)!=0) return -1;
#else
	if (regexec((regex_t*) key, msg->buf, 1, &pmatch, 0)!=0) return -1;
#endif
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
		if ((l=anchor_lump(&msg->add_rm, off+pmatch.rm_eo, 0, 0))==0)
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

	begin=get_header(msg); /* msg->orig previously .. uri problems */
	ret=-1; /* pessimist: we will not find any */
	len=strlen(str);

	while (begin<msg->buf+msg->len 
				&& regexec((regex_t*) key, begin, 1, &pmatch, 0)==0) {
		off=begin-msg->buf;
		if (pmatch.rm_so==-1){
			LOG(L_ERR, "ERROR: replace_all_f: offset unknown\n");
			return -1;
		}
		if ((l=del_lump(&msg->add_rm, pmatch.rm_so+off,
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
		if ((l=del_lump(&msg->add_rm, pmatch.rm_so+off,
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



static int fixup_regex(void** param, int param_no)
{
	regex_t* re;

	DBG("module - fixing %s\n", (char*)(*param));
	if (param_no!=1) return 0;
	if ((re=malloc(sizeof(regex_t)))==0) return E_OUT_OF_MEM;
	if (regcomp(re, *param, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ){
		free(re);
		LOG(L_ERR, "ERROR: %s : bad re %s\n", exports.name, (char*)*param);
		return E_BAD_RE;
	}
	/* free string */
	free(*param);
	/* replace it with the compiled re */
	*param=re;
	return 0;
}



static int append_to_reply_f(struct sip_msg* msg, char* key, char* str)
{
	struct lump_rpl *lump;

	lump = build_lump_rpl( key, strlen(key) );
	if (!lump)
	{
		LOG(L_ERR,"ERROR:append_to_reply : unable to create lump_rl\n");
		return -1;
	}
	add_lump_rpl( msg , lump );

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

	anchor = anchor_lump(&msg->add_rm, msg->unparsed - msg->buf, 0, 0);
	if (anchor == 0) {
		LOG(L_ERR, "append_hf(): Can't get anchor\n");
		return -1;
	}

	len=str1->len;
	if (str2) len+= str2->len + REQ_LINE(msg).uri.len;

	s = (char*)pkg_malloc(len);
	if (!s) {
		LOG(L_ERR, "append_hf(): No memory left\n");
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

	s = (str*)malloc(sizeof(str));
	if (!s) {
		LOG(L_ERR, "str_fixup(): No memory left\n");
		return E_UNSPEC;
	}

	s->s = (char*)*param;
	s->len = strlen(s->s);
	*param = (void*)s;

	return 0;
}
