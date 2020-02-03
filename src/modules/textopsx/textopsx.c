/**
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/data_lump.h"
#include "../../core/msg_translator.h"
#include "../../core/tcp_options.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/parse_hname2.h"
#include "../../core/select.h"
#include "../../core/select_buf.h"
#include "../../core/kemi.h"


#include "api.h"

MODULE_VERSION

static int msg_apply_changes_f(sip_msg_t *msg, char *str1, char *str2);

static int change_reply_status_f(sip_msg_t *, char *, char *);
static int change_reply_status_fixup(void **param, int param_no);

static int w_keep_hf_f(sip_msg_t *, char *, char *);

static int w_fnmatch2_f(sip_msg_t *, char *, char *);
static int w_fnmatch3_f(sip_msg_t *, char *, char *, char *);
static int fixup_fnmatch(void **param, int param_no);

static int w_remove_body_f(struct sip_msg *, char *, char *);

static int incexc_hf_value_f(struct sip_msg *msg, char *, char *);
static int include_hf_value_fixup(void **, int);
static int exclude_hf_value_fixup(void **, int);
static int hf_value_exists_fixup(void **, int);

static int insupddel_hf_value_f(struct sip_msg *msg, char *_hname, char *_val);
static int append_hf_value_fixup(void **param, int param_no);
static int insert_hf_value_fixup(void **param, int param_no);
static int remove_hf_value_fixup(void **param, int param_no);
static int assign_hf_value_fixup(void **param, int param_no);
static int remove_hf_value2_fixup(void **param, int param_no);
static int assign_hf_value2_fixup(void **param, int param_no);

static int bind_textopsx(textopsx_api_t *tob);

static int mod_init(void);

extern select_row_t sel_declaration[];

/* cfg functions */
/* clag-format off */
static cmd_export_t cmds[] = {
	{"msg_apply_changes", (cmd_function)msg_apply_changes_f, 0, 0, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE},
	{"change_reply_status", change_reply_status_f, 2,
			change_reply_status_fixup, 0, ONREPLY_ROUTE},
	{"remove_body", (cmd_function)w_remove_body_f, 0, 0, 0, ANY_ROUTE},
	{"keep_hf", (cmd_function)w_keep_hf_f, 0, fixup_regexp_null, 0, ANY_ROUTE},
	{"keep_hf", (cmd_function)w_keep_hf_f, 1, fixup_regexp_null, 0, ANY_ROUTE},
	{"fnmatch", (cmd_function)w_fnmatch2_f, 2, fixup_fnmatch, 0, ANY_ROUTE},
	{"fnmatch", (cmd_function)w_fnmatch3_f, 3, fixup_fnmatch, 0, ANY_ROUTE},
	{"append_hf_value", insupddel_hf_value_f, 2, append_hf_value_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
	{"insert_hf_value", insupddel_hf_value_f, 2, insert_hf_value_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
	{"remove_hf_value", insupddel_hf_value_f, 1, remove_hf_value_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
	{"assign_hf_value", insupddel_hf_value_f, 2, assign_hf_value_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
	{"remove_hf_value2", insupddel_hf_value_f, 1, remove_hf_value2_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
	{"assign_hf_value2", insupddel_hf_value_f, 2, assign_hf_value2_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
	{"include_hf_value", incexc_hf_value_f, 2, include_hf_value_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
	{"exclude_hf_value", incexc_hf_value_f, 2, exclude_hf_value_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
	{"hf_value_exists", incexc_hf_value_f, 2, hf_value_exists_fixup, 0,
			REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},

	{"bind_textopsx", (cmd_function)bind_textopsx, 1, 0, 0, ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

/* module exports structure */
struct module_exports exports = {
	"textopsx",		/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* exported cfg functions */
	0,				/* exported cfg parameters */
	0,				/* exported RPC methods */
	0,				/* exported pseudo-variables */
	0,				/* response handling function */
	mod_init,		/* module init function */
	0,				/* per-child init function */
	0,				/* destroy function */
};
/* clag-format on */


/**
 * init module function
 */
static int mod_init(void)
{
#ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
#endif
	register_select_table(sel_declaration);
	return 0;
}

/**
 *
 */
static int ki_msg_update_buffer(sip_msg_t *msg, str *obuf)
{
	if(obuf==NULL || obuf->s==NULL || obuf->len<=0) {
		LM_ERR("invalid buffer parameter\n");
		return -1;
	}

	if(obuf->len >= BUF_SIZE) {
		LM_ERR("new buffer is too large (%d)\n", obuf->len);
		return -1;
	}

	return sip_msg_update_buffer(msg, obuf);
}

/**
 *
 */
static int ki_msg_set_buffer(sip_msg_t *msg, str *obuf)
{
	if(msg->first_line.type != SIP_REPLY && get_route_type() != REQUEST_ROUTE) {
		LM_ERR("invalid usage - not in request route or a reply\n");
		return -1;
	}

	return ki_msg_update_buffer(msg, obuf);
}

/**
 *
 */
static int ki_msg_apply_changes(sip_msg_t *msg)
{
	return sip_msg_apply_changes(msg);
}

/**
 *
 */
static int msg_apply_changes_f(sip_msg_t *msg, char *str1, char *str2)
{
	return sip_msg_apply_changes(msg);
}

/**
 *
 */
static int change_reply_status_fixup(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_var_int_12(param, param_no);
	} else if(param_no == 2)
		return fixup_var_pve_str_12(param, param_no);
	else
		return 0;
}

/**
 *
 */
static int ki_change_reply_status(sip_msg_t *msg, int code, str *reason)
{
	struct lump *l;
	char *ch;

	if(reason==NULL || (reason->len<=0)) {
		LM_ERR("invalid reason parameter\n");
		return -1;
	}

	if((code < 100) || (code > 699)) {
		LM_ERR("wrong status code: %d\n", code);
		return -1;
	}

	if(((code < 300) || (msg->REPLY_STATUS < 300))
			&& (code / 100 != msg->REPLY_STATUS / 100)) {
		LM_ERR("the class of provisional or "
				   "positive final replies cannot be changed\n");
		return -1;
	}

	/* rewrite the status code directly in the message buffer */
	msg->first_line.u.reply.statuscode = code;
	msg->first_line.u.reply.status.s[2] = code % 10 + '0';
	code /= 10;
	msg->first_line.u.reply.status.s[1] = code % 10 + '0';
	code /= 10;
	msg->first_line.u.reply.status.s[0] = code + '0';

	l = del_lump(msg, msg->first_line.u.reply.reason.s - msg->buf,
			msg->first_line.u.reply.reason.len, 0);
	if(!l) {
		LM_ERR("Failed to add del lump\n");
		return -1;
	}
	/* clone the reason phrase, the lumps need to be pkg allocated */
	ch = (char *)pkg_malloc(reason->len);
	if(!ch) {
		LM_ERR("Not enough memory\n");
		return -1;
	}
	memcpy(ch, reason->s, reason->len);
	if(insert_new_lump_after(l, ch, reason->len, 0) == 0) {
		LM_ERR("failed to add new lump: %.*s\n", reason->len, ch);
		pkg_free(ch);
		return -1;
	}

	return 1;
}


/**
 *
 */
static int change_reply_status_f(
		struct sip_msg *msg, char *_code, char *_reason)
{
	int code;
	str reason;

	if(get_int_fparam(&code, msg, (fparam_t *)_code)
			|| get_str_fparam(&reason, msg, (fparam_t *)_reason)) {
		LM_ERR("cannot get parameters\n");
		return -1;
	}
	return ki_change_reply_status(msg, code, &reason);
}


/**
 *
 */
static int ki_remove_body(struct sip_msg *msg)
{
	str body = {0, 0};

	body.len = 0;
	body.s = get_body(msg);
	if(body.s == 0) {
		LM_DBG("no body in the message\n");
		return 1;
	}
	body.len = msg->buf + msg->len - body.s;
	if(body.len <= 0) {
		LM_DBG("empty body in the message\n");
		return 1;
	}
	if(del_lump(msg, body.s - msg->buf, body.len, 0) == 0) {
		LM_ERR("cannot remove body\n");
		return -1;
	}
	return 1;
}


/**
 *
 */
static int w_remove_body_f(struct sip_msg *msg, char *p1, char *p2)
{
	return ki_remove_body(msg);
}


/**
 *
 */
static int keep_hf_helper(sip_msg_t *msg, regex_t *re)
{
	struct hdr_field *hf;
	regmatch_t pmatch;
	char c;
	struct lump *l;

	/* we need to be sure we have seen all HFs */
	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("Error while parsing message\n");
		return -1;
	}
	for(hf = msg->headers; hf; hf = hf->next) {
		switch(hf->type) {
			case HDR_FROM_T:
			case HDR_TO_T:
			case HDR_CALLID_T:
			case HDR_CSEQ_T:
			case HDR_VIA_T:
			case HDR_VIA2_T:
			case HDR_CONTACT_T:
			case HDR_CONTENTLENGTH_T:
			case HDR_CONTENTTYPE_T:
			case HDR_ROUTE_T:
			case HDR_RECORDROUTE_T:
			case HDR_MAXFORWARDS_T:
				continue;
			default:;
		}

		if(re == NULL) {
			/* no regex to match => remove all */
			l = del_lump(msg, hf->name.s - msg->buf, hf->len, 0);
			if(l == 0) {
				LM_ERR("cannot remove header [%.*s]\n", hf->name.len,
						hf->name.s);
				return -1;
			}
		} else {
			c = hf->name.s[hf->name.len];
			hf->name.s[hf->name.len] = '\0';
			if(regexec(re, hf->name.s, 1, &pmatch, 0) != 0) {
				/* no match => remove */
				hf->name.s[hf->name.len] = c;
				l = del_lump(msg, hf->name.s - msg->buf, hf->len, 0);
				if(l == 0) {
					LM_ERR("cannot remove header [%.*s]\n", hf->name.len,
							hf->name.s);
					return -1;
				}
			} else {
				hf->name.s[hf->name.len] = c;
			}
		}
	}

	return -1;
}


/**
 *
 */
static int w_keep_hf_f(struct sip_msg *msg, char *key, char *foo)
{
	regex_t *re;

	if(key) {
		re = (regex_t *)key;
	} else {
		re = NULL;
	}
	return keep_hf_helper(msg, re);
}


/**
 *
 */
static int ki_keep_hf(sip_msg_t *msg)
{
	return keep_hf_helper(msg, NULL);
}


/**
 *
 */
static int ki_keep_hf_re(sip_msg_t *msg, str *sre)
{
	regex_t re;
	int ret;

	if(sre==NULL || sre->len<=0)
		return keep_hf_helper(msg, NULL);

	memset(&re, 0, sizeof(regex_t));
	if (regcomp(&re, sre->s, REG_EXTENDED|REG_ICASE|REG_NEWLINE)!=0) {
		LM_ERR("failed to compile regex: %.*s\n", sre->len, sre->s);
		return -1;
	}
	ret = keep_hf_helper(msg, &re);
	regfree(&re);
	return ret;
}


/**
 *
 */
static int w_fnmatch_ex(str *val, str *match, str *flags)
{
	int i;
	i = 0;
#ifdef FNM_CASEFOLD
	if(flags && (flags->s[0] == 'i' || flags->s[0] == 'I'))
		i = FNM_CASEFOLD;
#endif
	if(fnmatch(match->s, val->s, i) == 0)
		return 0;
	return -1;
}

/**
 *
 */
static int w_fnmatch2_f(sip_msg_t *msg, char *val, char *match)
{
	str sval;
	str smatch;
	if(get_str_fparam(&sval, msg, (fparam_t *)val) < 0
			|| get_str_fparam(&smatch, msg, (fparam_t *)match) < 0) {
		LM_ERR("invalid parameters");
		return -1;
	}
	if(w_fnmatch_ex(&sval, &smatch, NULL) < 0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_fnmatch3_f(sip_msg_t *msg, char *val, char *match, char *flags)
{
	str sval;
	str smatch;
	str sflags;
	if(get_str_fparam(&sval, msg, (fparam_t *)val) < 0
			|| get_str_fparam(&smatch, msg, (fparam_t *)match) < 0
			|| get_str_fparam(&sflags, msg, (fparam_t *)flags) < 0) {
		LM_ERR("invalid parameters");
		return -1;
	}
	if(w_fnmatch_ex(&sval, &smatch, &sflags) < 0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_fnmatch(sip_msg_t *msg, str *val, str *match)
{
	return w_fnmatch_ex(val, match, NULL);
}

/**
 *
 */
static int ki_fnmatch_ex(sip_msg_t *msg, str *val, str *match, str *flags)
{
	return w_fnmatch_ex(val, match, flags);
}

/**
 *
 */
static int fixup_fnmatch(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_var_pve_12(param, param_no);
	} else if(param_no == 2) {
		return fixup_var_pve_12(param, param_no);
	} else if(param_no == 3) {
		return fixup_var_pve_12(param, param_no);
	} else {
		return 0;
	}
}

/*
 * Function to load the textops api.
 */
static int bind_textopsx(textopsx_api_t *tob)
{
	if(tob == NULL) {
		LM_WARN("textopsx_binds: Cannot load textopsx API into a NULL "
				"pointer\n");
		return -1;
	}
	tob->msg_apply_changes = msg_apply_changes_f;
	return 0;
}


/**
 * functions operating on header value
 */
#define HNF_ALL 0x01
#define HNF_IDX 0x02

#define MAX_HF_VALUE_STACK 10

enum
{
	hnoInsert,
	hnoAppend,
	hnoAssign,
	hnoRemove,
	hnoInclude,
	hnoExclude,
	hnoIsIncluded,
	hnoGetValue,
	hnoGetValueUri,
	hnoGetValueName,
	hnoRemove2,
	hnoAssign2,
	hnoGetValue2
};

struct hname_data
{
	int oper;
	int htype;
	str hname;
	int flags;
	int idx;
	str param;
};

#define is_space(_p) \
	((_p) == '\t' || (_p) == '\n' || (_p) == '\r' || (_p) == ' ')

#define eat_spaces(_p)       \
	while(is_space(*(_p))) { \
		(_p)++;              \
	}

#define is_alphanum(_p)                                           \
	(((_p) >= 'a' && (_p) <= 'z') || ((_p) >= 'A' && (_p) <= 'Z') \
			|| ((_p) >= '0' && (_p) <= '9') || (_p) == '_' || (_p) == '-')

#define eat_while_alphanum(_p)  \
	while(is_alphanum(*(_p))) { \
		(_p)++;                 \
	}

static int fixup_hvalue_param(void **param, int param_no)
{
	return fixup_spve_null(param, 1);
}

static int eval_hvalue_param(sip_msg_t *msg, gparam_t *val, str *s)
{
	if(fixup_get_svalue(msg, val, s) < 0) {
		LM_ERR("could not get string param value\n");
		return E_UNSPEC;
	}
	return 1;
}

/* parse:  hname [ ([] | [*] | [number]) ] [ "." param ] */
static int fixup_hname_param(char *hname, struct hname_data **h)
{
	struct hdr_field hdr;
	char *savep, savec;

	*h = pkg_malloc(sizeof(**h));
	if(!*h)
		return E_OUT_OF_MEM;
	memset(*h, 0, sizeof(**h));

	memset(&hdr, 0, sizeof(hdr));
	eat_spaces(hname);
	(*h)->hname.s = hname;
	savep = hname;
	eat_while_alphanum(hname);
	(*h)->hname.len = hname - (*h)->hname.s;
	savec = *hname;
	*hname = ':';
	parse_hname2_short(
			(*h)->hname.s, (*h)->hname.s + (*h)->hname.len + 1, &hdr);
	*hname = savec;

	if(hdr.type == HDR_ERROR_T)
		goto err;
	(*h)->htype = hdr.type;

	eat_spaces(hname);
	savep = hname;
	if(*hname == '[') {
		hname++;
		eat_spaces(hname);
		savep = hname;
		(*h)->flags |= HNF_IDX;
		if(*hname == '*') {
			(*h)->flags |= HNF_ALL;
			hname++;
		} else if(*hname != ']') {
			char *c;
			(*h)->idx = strtol(hname, &c, 10);
			if(hname == c)
				goto err;
			hname = c;
		}
		eat_spaces(hname);
		savep = hname;
		if(*hname != ']')
			goto err;
		hname++;
	}
	eat_spaces(hname);
	savep = hname;
	if(*hname == '.') {
		hname++;
		eat_spaces(hname);
		savep = hname;
		(*h)->param.s = hname;
		eat_while_alphanum(hname);
		(*h)->param.len = hname - (*h)->param.s;
		if((*h)->param.len == 0)
			goto err;
	} else {
		(*h)->param.s = hname;
	}
	savep = hname;
	if(*hname != '\0')
		goto err;
	(*h)->hname.s[(*h)->hname.len] = '\0';
	(*h)->param.s[(*h)->param.len] = '\0';
	return 0;
err:
	pkg_free(*h);
	LM_ERR("cannot parse header near '%s'\n", savep);
	return E_CFG;
}

static int fixup_hname_str(void **param, int param_no)
{
	if(param_no == 1) {
		struct hname_data *h;
		int res = fixup_hname_param(*param, &h);
		if(res < 0)
			return res;
		*param = h;
	} else if(param_no == 2) {
		return fixup_hvalue_param(param, param_no);
	}
	return 0;
}

static int fixup_free_hname_str(void **param, int param_no)
{
	if(param_no == 1) {
		struct hname_data *h;
		h = (struct hname_data *)(*param);
		pkg_free(h);
		return 0;
	} else if(param_no == 2) {
		return fixup_free_spve_null(param, 1);
	}
	return 0;
}

static int find_next_hf(
		struct sip_msg *msg, struct hname_data *hname, struct hdr_field **hf)
{
	if(!*hf) {
		if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
			LM_ERR("Error while parsing message\n");
			return -1;
		}
		*hf = msg->headers;
	} else {
		*hf = (*hf)->next;
	}
	for(; *hf; *hf = (*hf)->next) {
		if(hname->htype == HDR_OTHER_T) {
			if((*hf)->name.len == hname->hname.len
					&& strncasecmp(
							   (*hf)->name.s, hname->hname.s, (*hf)->name.len)
							   == 0)
				return 1;
		} else if(hname->htype == (*hf)->type) {
			return 1;
		}
	}
	return 0;
}

static int find_next_value(char **start, char *end, str *val, str *lump_val)
{
	int quoted = 0;
	lump_val->s = *start;
	while(*start < end && is_space(**start))
		(*start)++;
	val->s = *start;
	while(*start < end && (**start != ',' || quoted)) {
		if(**start == '\"' && (!quoted || (*start)[-1] != '\\'))
			quoted = ~quoted;
		(*start)++;
	}
	val->len = *start - val->s;
	while(val->len > 0 && is_space(val->s[val->len - 1]))
		val->len--;
	/* we cannot automatically strip quotes!!! an example why: "name" <sip:ssss>;param="bar"
	if (val->len >= 2 && val->s[0] == '\"' && val->s[val->len-1] == '\"') {
		val->s++;
		val->len -= 2;
	}
*/
	while(*start < end && **start != ',')
		(*start)++;
	if(*start < end) {
		(*start)++;
	}
	lump_val->len = *start - lump_val->s;
	return (*start < end);
}

static void adjust_lump_val_for_delete(struct hdr_field *hf, str *lump_val)
{
	if(lump_val->s + lump_val->len == hf->body.s + hf->body.len) {
		if(lump_val->s > hf->body.s) {
			/* in case if is it last value in header save position of last delimiter to remove it with rightmost value */
			lump_val->s--;
			lump_val->len++;
		}
	}
}

static int find_hf_value_idx(struct sip_msg *msg, struct hname_data *hname,
		struct hdr_field **hf, str *val, str *lump_val)
{
	int res;
	char *p;
	if(hname->flags & HNF_ALL || hname->idx == 0)
		return -1;
	*hf = 0;
	if(hname->idx > 0) {
		int idx;
		idx = hname->idx;
		do {
			res = find_next_hf(msg, hname, hf);
			if(res < 0)
				return -1;
			if(*hf) {
				if(val) {
					lump_val->len = 0;
					p = (*hf)->body.s;
					do {
						res = find_next_value(&p,
								(*hf)->body.s + (*hf)->body.len, val, lump_val);
						idx--;
					} while(res && idx);
				} else {
					idx--;
				}
			}
		} while(*hf && idx);
	} else if(hname->idx < 0) { /* search from the bottom */
		struct hf_value_stack
		{
			str val, lump_val;
			struct hdr_field *hf;
		} stack[MAX_HF_VALUE_STACK];
		int stack_pos, stack_num;

		if(-hname->idx > MAX_HF_VALUE_STACK)
			return -1;
		stack_pos = stack_num = 0;
		do {
			res = find_next_hf(msg, hname, hf);
			if(res < 0)
				return -1;
			if(*hf) {
				stack[stack_pos].lump_val.len = 0;
				p = (*hf)->body.s;
				do {
					stack[stack_pos].hf = *hf;
					if(val)
						res = find_next_value(&p,
								(*hf)->body.s + (*hf)->body.len,
								&stack[stack_pos].val,
								&stack[stack_pos].lump_val);
					else
						res = 0;
					stack_pos++;
					if(stack_pos >= MAX_HF_VALUE_STACK)
						stack_pos = 0;
					if(stack_num < MAX_HF_VALUE_STACK)
						stack_num++;

				} while(res);
			}
		} while(*hf);

		if(-hname->idx <= stack_num) {
			stack_pos += hname->idx;
			if(stack_pos < 0)
				stack_pos += MAX_HF_VALUE_STACK;
			*hf = stack[stack_pos].hf;
			if(val) {
				*val = stack[stack_pos].val;
				*lump_val = stack[stack_pos].lump_val;
			}
		} else {
			*hf = 0;
		}
	} else
		return -1;
	return *hf ? 1 : 0;
}

static int find_hf_value_param(struct hname_data *hname, str *param_area,
		str *value, str *lump_upd, str *lump_del)
{
	int i, j, found;

	i = 0;
	while(1) {
		lump_del->s = param_area->s + i;
		for(; i < param_area->len && is_space(param_area->s[i]); i++)
			;
		if(i < param_area->len
				&& param_area->s[i] == ';') { /* found a param ? */
			i++;
			for(; i < param_area->len && is_space(param_area->s[i]); i++)
				;
			j = i;
			for(; i < param_area->len && !is_space(param_area->s[i])
					&& param_area->s[i] != '=' && param_area->s[i] != ';';
					i++)
				;

			found = hname->param.len == i - j
					&& !strncasecmp(hname->param.s, param_area->s + j, i - j);
			lump_upd->s = param_area->s + i;
			value->s = param_area->s + i;
			value->len = 0;
			for(; i < param_area->len && is_space(param_area->s[i]); i++)
				;
			if(i < param_area->len && param_area->s[i] == '=') {
				i++;
				for(; i < param_area->len && is_space(param_area->s[i]); i++)
					;
				value->s = param_area->s + i;
				if(i < param_area->len) {
					if(param_area->s[i] == '\"') {
						i++;
						value->s++;
						for(; i < param_area->len; i++) {
							if(param_area->s[i] == '\"') {
								i++;
								break;
							}
							value->len++;
						}
					} else {
						for(; i < param_area->len && !is_space(param_area->s[i])
								&& param_area->s[i] != ';';
								i++, value->len++)
							;
					}
				}
			}
			if(found) {
				lump_del->len = param_area->s + i - lump_del->s;
				lump_upd->len = param_area->s + i - lump_upd->s;
				return 1;
			}
		} else { /* not found, return last correct position, should be end of param area */
			lump_del->len = 0;
			return 0;
		}
	}
}

/* parse:  something param_name=param_value something [ "," something param_name="param_value" ....]
 * 'something' is required by Authenticate
 */
static int find_hf_value2_param(struct hname_data *hname, str *param_area,
		str *value, str *lump_upd, str *lump_del, char *delim)
{
	int i, j, k, found, comma_flag;

	i = 0;
	*delim = 0;
	lump_del->len = 0;
	while(i < param_area->len) {

		lump_del->s = param_area->s + i;
		while(i < param_area->len && is_space(param_area->s[i]))
			i++;
		comma_flag = i < param_area->len && param_area->s[i] == ',';
		if(comma_flag)
			i++;
		while(i < param_area->len && is_space(param_area->s[i]))
			i++;

		if(i < param_area->len
				&& is_alphanum(param_area->s[i])) { /* found a param name ? */
			j = i;
			if(!*delim)
				*delim = ' ';
			while(i < param_area->len && is_alphanum(param_area->s[i]))
				i++;

			k = i;
			while(i < param_area->len && is_space(param_area->s[i]))
				i++;
			lump_upd->s = param_area->s + i;
			if(i < param_area->len
					&& param_area->s[i]
							   == '=') { /* if equal then it's the param */
				*delim = ',';
				i++;
				found = hname->param.len == k - j
						&& !strncasecmp(
								   hname->param.s, param_area->s + j, k - j);
				while(i < param_area->len && is_space(param_area->s[i]))
					i++;

				value->s = param_area->s + i;
				value->len = 0;
				if(i < param_area->len) {
					if(param_area->s[i] == '\"') {
						i++;
						value->s++;
						for(; i < param_area->len; i++) {
							if(param_area->s[i] == '\"') {
								i++;
								break;
							}
							value->len++;
						}
					} else {
						for(; i < param_area->len && !is_space(param_area->s[i])
								&& param_area->s[i] != ',';
								i++, value->len++)
							;
					}
				}
				if(found) {
					lump_upd->len = param_area->s + i - lump_upd->s;
					lump_del->len = param_area->s + i - lump_del->s;

					while(i < param_area->len && is_space(param_area->s[i]))
						i++;

					if(!comma_flag && i < param_area->len
							&& param_area->s[i] == ',') {
						i++;
						lump_del->len = param_area->s + i - lump_del->s;
					}
					return 1;
				}
			}
			while(i < param_area->len && is_space(param_area->s[i]))
				i++;
		} else {
			while(i < param_area->len && !is_space(param_area->s[i])
					&& !(param_area->s[i] != ','))
				i++;
		}
	}
	lump_del->s = param_area->s + i;
	return 0;
}

static int insert_header_lump(struct sip_msg *msg, char *msg_position,
		int lump_before, str *hname, str *val)
{
	struct lump *anchor;
	char *s;
	int len;

	anchor = anchor_lump(msg, msg_position - msg->buf, 0, 0);
	if(anchor == 0) {
		LM_ERR("Can't get anchor\n");
		return -1;
	}

	len = hname->len + 2 + val->len + 2;

	s = (char *)pkg_malloc(len);
	if(!s) {
		LM_ERR("not enough memory\n");
		return -1;
	}

	memcpy(s, hname->s, hname->len);
	s[hname->len] = ':';
	s[hname->len + 1] = ' ';
	memcpy(s + hname->len + 2, val->s, val->len);
	s[hname->len + 2 + val->len] = '\r';
	s[hname->len + 2 + val->len + 1] = '\n';

	if((lump_before ? insert_new_lump_before(anchor, s, len, 0)
					: insert_new_lump_after(anchor, s, len, 0))
			== 0) {
		LM_ERR("Can't insert lump\n");
		pkg_free(s);
		return -1;
	}
	return 1;
}

static int insert_value_lump(struct sip_msg *msg, struct hdr_field *hf,
		char *msg_position, int lump_before, str *val)
{
	struct lump *anchor;
	char *s;
	int len;

	anchor = anchor_lump(msg, msg_position - msg->buf, 0, 0);
	if(anchor == 0) {
		LM_ERR("Can't get anchor\n");
		return -1;
	}

	len = val->len + 1;

	s = (char *)pkg_malloc(len);
	if(!s) {
		LM_ERR("not enough memory\n");
		return -1;
	}

	if(!hf) {
		memcpy(s, val->s, val->len);
		len--;
	} else if(msg_position == hf->body.s + hf->body.len) {
		s[0] = ',';
		memcpy(s + 1, val->s, val->len);
	} else {
		memcpy(s, val->s, val->len);
		s[val->len] = ',';
	}
	if((lump_before ? insert_new_lump_before(anchor, s, len, 0)
					: insert_new_lump_after(anchor, s, len, 0))
			== 0) {
		LM_ERR("Can't insert lump\n");
		pkg_free(s);
		return -1;
	}
	return 1;
}

static int delete_value_lump(
		struct sip_msg *msg, struct hdr_field *hf, str *val)
{
	struct lump *l;
	/* TODO: check already existing lumps */
	if(hf && val->s == hf->body.s
			&& val->len == hf->body.len) /* check if remove whole haeder? */
		l = del_lump(msg, hf->name.s - msg->buf, hf->len, 0);
	else
		l = del_lump(msg, val->s - msg->buf, val->len, 0);
	if(l == 0) {
		LM_ERR("not enough memory\n");
		return -1;
	}
	return 1;
}

static int ki_modify_hf(sip_msg_t *msg, str *hexp, str *val,
	fixup_function fixf, cmd_function cmdf)
{
	int ret;
	char *s1 = NULL;
	char *s2 = NULL;
	void *p1 = NULL;
	void *p2 = NULL;

	s1 = as_asciiz(hexp);
	p1 = s1;
	if(fixf(&p1, 1)!=0) {
		LM_ERR("failed to fix first parameter\n");
		p1 = NULL;
		goto error;
	}
	if(val && val->s!=0 && val->len>0) {
		s2 = as_asciiz(val);
		p2 = s2;
		if(fixf(&p2, 2)!=0) {
			LM_ERR("failed to fix second parameter\n");
			p2 = NULL;
			goto error;
		}
	}

	ret = cmdf(msg, (char*)p1, (char*)p2);

	if(p2!=NULL) fixup_free_hname_str(&p2, 2);
	fixup_free_hname_str(&p1, 1);
	if(s2!=NULL) pkg_free(s2);
	pkg_free(s1);
	return ret;

error:
	if(p1!=NULL) fixup_free_hname_str(&p1, 1);
	if(s2!=NULL) pkg_free(s2);
	if(s1!=NULL) pkg_free(s1);
	return -1;
}

static int incexc_hf_value_str_f(struct sip_msg *msg, char *_hname, str *_pval)
{
	struct hname_data *hname = (void *)_hname;
	struct hdr_field *hf, *lump_hf;
	str val, hval1, hval2;
	char *p;
	int res;

	val = *_pval;
	if(!val.len)
		return -1;
	hf = 0;
	lump_hf = 0;
	while(1) {
		if(find_next_hf(msg, hname, &hf) < 0)
			return -1;
		if(!hf)
			break;
		hval2.len = 0;
		p = hf->body.s;
		do {
			res = find_next_value(
					&p, hf->body.s + hf->body.len, &hval1, &hval2);
			if(hval1.len && val.len == hval1.len
					&& strncasecmp(val.s, hval1.s, val.len) == 0) {
				switch(hname->oper) {
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
		} while(res);
		switch(hname->oper) {
			case hnoInclude:
				if(!lump_hf) {
					lump_hf = hf;
				}
				break;
			default:
				break;
		}
	}
	switch(hname->oper) {
		case hnoIsIncluded:
			return -1;
		case hnoInclude:
			if(lump_hf)
				return insert_value_lump(msg, lump_hf,
						lump_hf->body.s + lump_hf->body.len, 1, &val);
			else
				return insert_header_lump(
						msg, msg->unparsed, 1, &hname->hname, &val);
		default:
			return 1;
	}
}

static int incexc_hf_value_f(struct sip_msg *msg, char *_hname, char *_val)
{
	str val;
	int res;

	res = eval_hvalue_param(msg, (void *)_val, &val);

	if(res < 0)
		return res;
	if(!val.len)
		return -1;

	return incexc_hf_value_str_f(msg, _hname, &val);
}

#define INCEXC_HF_VALUE_FIXUP(_func, _oper)                                  \
	static int _func(void **param, int param_no)                             \
	{                                                                        \
		char *p = *param;                                                    \
		int res = fixup_hname_str(param, param_no);                          \
		if(res < 0)                                                          \
			return res;                                                      \
		if(param_no == 1) {                                                  \
			if(((struct hname_data *)*param)->flags & HNF_IDX                \
					|| ((struct hname_data *)*param)->param.len) {           \
				LM_ERR("neither index nor param may be specified in '%s'\n", \
						p);                                                  \
				return E_CFG;                                                \
			}                                                                \
			((struct hname_data *)*param)->oper = _oper;                     \
		}                                                                    \
		return 0;                                                            \
	}

INCEXC_HF_VALUE_FIXUP(include_hf_value_fixup, hnoInclude)
INCEXC_HF_VALUE_FIXUP(exclude_hf_value_fixup, hnoExclude)
INCEXC_HF_VALUE_FIXUP(hf_value_exists_fixup, hnoIsIncluded)

static int ki_include_hf_value(sip_msg_t *msg, str *hexp, str *val)
{
	return ki_modify_hf(msg, hexp, val, include_hf_value_fixup,
			incexc_hf_value_f);
}

static int ki_exclude_hf_value(sip_msg_t *msg, str *hexp, str *val)
{
	return ki_modify_hf(msg, hexp, val, exclude_hf_value_fixup,
			incexc_hf_value_f);
}

static int ki_hf_value_exists(sip_msg_t *msg, str *hexp, str *val)
{
	return ki_modify_hf(msg, hexp, val, hf_value_exists_fixup,
			incexc_hf_value_f);
}

static void get_uri_and_skip_until_params(str *param_area, str *name, str *uri)
{
	int i, quoted, uri_pos, uri_done;

	name->len = 0;
	uri->len = 0;
	uri->s = 0;
	uri_done = 0;
	name->s = param_area->s;
	for(i = 0; i < param_area->len && param_area->s[i] != ';';) {
		/* [ *(token LSW)/quoted-string ] "<" addr-spec ">" | addr-spec */
		/* skip name */
		for(quoted = 0, uri_pos = i; i < param_area->len; i++) {
			if(!quoted) {
				if(param_area->s[i] == '\"') {
					quoted = 1;
					uri_pos = -1;
				} else if(param_area->s[i] == '<' || param_area->s[i] == ';'
						  || is_space(param_area->s[i]))
					break;
			} else if(param_area->s[i] == '\"' && param_area->s[i - 1] != '\\')
				quoted = 0;
		}
		if(!name->len)
			name->len = param_area->s + i - name->s;
		if(uri_pos >= 0 && !uri_done) {
			uri->s = param_area->s + uri_pos;
			uri->len = param_area->s + i - uri->s;
		}
		/* skip uri */
		while(i < param_area->len && is_space(param_area->s[i]))
			i++;
		if(i < param_area->len && param_area->s[i] == '<') {
			uri->s = param_area->s + i;
			uri->len = 0;
			for(quoted = 0; i < param_area->len; i++) {
				if(!quoted) {
					if(param_area->s[i] == '\"')
						quoted = 1;
					else if(param_area->s[i] == '>') {
						uri->len = param_area->s + i - uri->s + 1;
						uri_done = 1;
						break;
					}
				} else if(param_area->s[i] == '\"'
						  && param_area->s[i - 1] != '\\')
					quoted = 0;
			}
		}
	}
	param_area->s += i;
	param_area->len -= i;
	if(uri->s == name->s)
		name->len = 0;
}

static int assign_hf_do_lumping(struct sip_msg *msg, struct hdr_field *hf,
		struct hname_data *hname, str *value, int upd_del_fl, str *lump_upd,
		str *lump_del, char delim)
{
	int len, i;
	char *s;
	struct lump *anchor;

	if(upd_del_fl) {
		len = value ? lump_upd->len : lump_del->len;
		if(len > 0) {
			if(!del_lump(msg, (value ? lump_upd->s : lump_del->s) - msg->buf,
					   len, 0)) {
				LM_ERR("not enough memory\n");
				return -1;
			}
		}
		if(value && value->len) {
			anchor = anchor_lump(msg, lump_upd->s - msg->buf, 0, 0);
			if(anchor == 0) {
				LM_ERR("Can't get anchor\n");
				return -1;
			}

			len = 1 + value->len;
			s = pkg_malloc(len);
			if(!s) {
				LM_ERR("not enough memory\n");
				return -1;
			}
			s[0] = '=';
			memcpy(s + 1, value->s, value->len);
			if((insert_new_lump_before(anchor, s, len, 0)) == 0) {
				LM_ERR("Can't insert lump\n");
				pkg_free(s);
				return -1;
			}
		}
	} else {
		if(!value)
			return -1;

		anchor = anchor_lump(msg, lump_del->s - msg->buf, 0, 0);
		if(anchor == 0) {
			LM_ERR("Can't get anchor\n");
			return -1;
		}

		len = 1 + hname->param.len + (value->len ? value->len + 1 : 0);
		s = pkg_malloc(len);
		if(!s) {
			LM_ERR("not enough memory\n");
			return -1;
		}
		if(delim) {
			s[0] = delim;
			i = 1;
		} else {
			i = 0;
			len--;
		}
		memcpy(s + i, hname->param.s, hname->param.len);
		if(value->len) {
			s[hname->param.len + i] = '=';
			memcpy(s + i + hname->param.len + 1, value->s, value->len);
		}

		if((insert_new_lump_before(anchor, s, len, 0)) == 0) {
			LM_ERR("Can't insert lump\n");
			pkg_free(s);
			return -1;
		}
	}
	return 1;
}


static int assign_hf_process_params(struct sip_msg *msg, struct hdr_field *hf,
		struct hname_data *hname, str *value, str *value_area)
{
	int r, r2, res = 0;
	str param_area, lump_upd, lump_del, dummy_val, dummy_name, dummy_uri;
	param_area = *value_area;
	get_uri_and_skip_until_params(&param_area, &dummy_name, &dummy_uri);
	do {
		r = find_hf_value_param(
				hname, &param_area, &dummy_val, &lump_upd, &lump_del);
		r2 = assign_hf_do_lumping(
				msg, hf, hname, value, r, &lump_upd, &lump_del, ';');
		if(res == 0)
			res = r2;
		if(r && !value) { /* remove all parameters */
			param_area.len -= lump_del.s + lump_del.len - param_area.s;
			param_area.s = lump_del.s + lump_del.len;
		}
	} while(!value && r);
	return res;
}

static int assign_hf_process2_params(struct sip_msg *msg, struct hdr_field *hf,
		struct hname_data *hname, str *value)
{
	int r, r2, res = 0;
	str param_area, lump_upd, lump_del, dummy_val;
	char delim;

	param_area = hf->body;

	do {
		r = find_hf_value2_param(
				hname, &param_area, &dummy_val, &lump_upd, &lump_del, &delim);
		r2 = assign_hf_do_lumping(
				msg, hf, hname, value, r, &lump_upd, &lump_del, delim);
		if(res == 0)
			res = r2;
		if(r && !value) { /* remove all parameters */
			param_area.len -= lump_del.s + lump_del.len - param_area.s;
			param_area.s = lump_del.s + lump_del.len;
		}
	} while(!value && r);
	return res;
}

static int insupddel_hf_value_f(struct sip_msg *msg, char *_hname, char *_val)
{
	struct hname_data *hname = (void *)_hname;
	struct hdr_field *hf;
	str val = {0};
	str hval1, hval2;
	int res;

	if(_val) {
		res = eval_hvalue_param(msg, (void *)_val, &val);
		if(res < 0)
			return res;
	}
	switch(hname->oper) {
		case hnoAppend:
			if((hname->flags & HNF_IDX) == 0) {
				if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
					LM_ERR("Error while parsing message\n");
					return -1;
				}
				return insert_header_lump(
						msg, msg->unparsed, 1, &hname->hname, &val);
			} else {
				res = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if(res < 0)
					return res;
				if(hf) {
					return insert_value_lump(msg, hf, hval2.s + hval2.len,
							res /* insert after, except it is last value in header */
							,
							&val);
				} else {
					return insert_header_lump(
							msg, msg->unparsed, 1, &hname->hname, &val);
				}
			}
		case hnoInsert:
			/* if !HNF_IDX is possible parse only until first hname header
			 * but not trivial for HDR_OTHER_T header, not implemented */
			res = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
			if(res < 0)
				return res;
			if(hf && (hname->flags & HNF_IDX) == 0) {
				return insert_header_lump(
						msg, hf->name.s, 1, &hname->hname, &val);
			} else if(!hf && hname->idx == 1) {
				return insert_header_lump(
						msg, msg->unparsed, 1, &hname->hname, &val);
			} else if(hf) {
				return insert_value_lump(msg, hf, hval2.s, 1, &val);
			} else
				return -1;

		case hnoRemove:
		case hnoAssign:
			if(hname->flags & HNF_ALL) {
				struct hdr_field *hf = 0;
				int fl = -1;
				do {
					res = find_next_hf(msg, hname, &hf);
					if(res < 0)
						return res;
					if(hf) {
						if(!hname->param.len) {
							fl = 1;
							delete_value_lump(msg, hf, &hf->body);
						} else {
							char *p;
							hval2.len = 0;
							p = hf->body.s;
							do {
								res = find_next_value(&p,
										hf->body.s + hf->body.len, &hval1,
										&hval2);
								if(assign_hf_process_params(msg, hf, hname,
										   _val ? &val : 0, &hval1)
										> 0)
									fl = 1;
							} while(res);
						}
					}
				} while(hf);
				return fl;
			} else {
				res = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if(res < 0)
					return res;
				if(hf) {
					if(!hname->param.len) {
						if(hname->oper == hnoRemove) {
							adjust_lump_val_for_delete(hf, &hval2);
							return delete_value_lump(msg, hf, &hval2);
						} else {
							res = delete_value_lump(msg,
									0 /* delete only value part */, &hval1);
							if(res < 0)
								return res;
							if(val.len) {
								return insert_value_lump(msg,
										0 /* do not add delims */, hval1.s, 1,
										&val);
							}
							return 1;
						}
					} else {
						return assign_hf_process_params(
								msg, hf, hname, _val ? &val : 0, &hval1);
					}
				}
			}
			break;
		case hnoRemove2:
		case hnoAssign2:
			if(hname->flags & HNF_ALL) {
				struct hdr_field *hf = 0;
				int fl = -1;
				do {
					res = find_next_hf(msg, hname, &hf);
					if(res < 0)
						return res;
					if(hf) {
						if(!hname->param.len) { /* the same as hnoRemove/hnoAssign */
							fl = 1;
							delete_value_lump(msg, hf, &hf->body);
						} else {

							if(assign_hf_process2_params(
									   msg, hf, hname, _val ? &val : 0)
									> 0)
								fl = 1;
						}
					}
				} while(hf);
				return fl;
			} else {
				res = find_hf_value_idx(msg, hname, &hf, 0, 0);
				if(res < 0)
					return res;
				if(hf) {
					if(!hname->param.len) {
						if(hname->oper == hnoRemove2) {
							return delete_value_lump(msg, hf, &hf->body);
						} else {
							res = delete_value_lump(msg,
									0 /* delete only value part */, &hf->body);
							if(res < 0)
								return res;
							if(val.len) {
								return insert_value_lump(msg,
										0 /* do not add delims */, hf->body.s,
										1, &val);
							}
							return 1;
						}
					} else {
						return assign_hf_process2_params(
								msg, hf, hname, _val ? &val : 0);
					}
				}
			}
			break;
	}
	return -1;
}

static int append_hf_value_fixup(void **param, int param_no)
{
	int res = fixup_hname_str(param, param_no);
	if(res < 0)
		return res;
	if(param_no == 1) {
		if(((struct hname_data *)*param)->flags & HNF_ALL) {
			LM_ERR("asterisk not supported\n");
			return E_CFG;
		} else if((((struct hname_data *)*param)->flags & HNF_IDX) == 0
				  || !((struct hname_data *)*param)->idx) {
			((struct hname_data *)*param)->idx = -1;
		}
		if(((struct hname_data *)*param)->idx < -MAX_HF_VALUE_STACK) {
			LM_ERR("index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
			return E_CFG;
		}
		if(((struct hname_data *)*param)->param.len) {
			LM_ERR("param not supported\n");
			return E_CFG;
		}
		((struct hname_data *)*param)->oper = hnoAppend;
	}
	return 0;
}

static int ki_append_hf_value(sip_msg_t *msg, str *hexp, str *val)
{
	return ki_modify_hf(msg, hexp, val, append_hf_value_fixup,
			insupddel_hf_value_f);
}

static int insert_hf_value_fixup(void **param, int param_no)
{
	int res = fixup_hname_str(param, param_no);
	if(res < 0)
		return res;
	if(param_no == 1) {
		if(((struct hname_data *)*param)->flags & HNF_ALL) {
			LM_ERR("asterisk not supported\n");
			return E_CFG;
		} else if((((struct hname_data *)*param)->flags & HNF_IDX) == 0
				  || !((struct hname_data *)*param)->idx) {
			((struct hname_data *)*param)->idx = 1;
		}
		if(((struct hname_data *)*param)->idx < -MAX_HF_VALUE_STACK) {
			LM_ERR("index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
			return E_CFG;
		}
		if(((struct hname_data *)*param)->param.len) {
			LM_ERR("param not supported\n");
			return E_CFG;
		}
		((struct hname_data *)*param)->oper = hnoInsert;
	}
	return 0;
}

static int ki_insert_hf_value(sip_msg_t *msg, str *hexp, str *val)
{
	return ki_modify_hf(msg, hexp, val, insert_hf_value_fixup,
			insupddel_hf_value_f);
}

static int remove_hf_value_fixup(void **param, int param_no)
{
	int res = fixup_hname_str(param, param_no);
	if(res < 0)
		return res;
	if(param_no == 1) {
		if((((struct hname_data *)*param)->flags & HNF_IDX) == 0
				|| !((struct hname_data *)*param)->idx) {
			((struct hname_data *)*param)->idx = 1;
			((struct hname_data *)*param)->flags |= HNF_IDX;
		}
		if(((struct hname_data *)*param)->idx < -MAX_HF_VALUE_STACK) {
			LM_ERR("index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
			return E_CFG;
		}
		((struct hname_data *)*param)->oper = hnoRemove;
	}
	return 0;
}

static int ki_remove_hf_value(sip_msg_t *msg, str *hexp)
{
	return ki_modify_hf(msg, hexp, NULL, remove_hf_value_fixup,
			insupddel_hf_value_f);
}

static int assign_hf_value_fixup(void **param, int param_no)
{
	int res = fixup_hname_str(param, param_no);
	if(res < 0)
		return res;
	if(param_no == 1) {
		if((((struct hname_data *)*param)->flags & HNF_ALL)
				&& !((struct hname_data *)*param)->param.len) {
			LM_ERR("asterisk not supported without param\n");
			return E_CFG;
		} else if((((struct hname_data *)*param)->flags & HNF_IDX) == 0
				  || !((struct hname_data *)*param)->idx) {
			((struct hname_data *)*param)->idx = 1;
			((struct hname_data *)*param)->flags |= HNF_IDX;
		}
		if(((struct hname_data *)*param)->idx < -MAX_HF_VALUE_STACK) {
			LM_ERR("index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
			return E_CFG;
		}
		((struct hname_data *)*param)->oper = hnoAssign;
	}
	return 0;
}

static int ki_assign_hf_value(sip_msg_t *msg, str *hexp, str *val)
{
	return ki_modify_hf(msg, hexp, val, assign_hf_value_fixup,
			insupddel_hf_value_f);
}

static int remove_hf_value2_fixup(void **param, int param_no)
{
	int res = remove_hf_value_fixup(param, param_no);
	if(res < 0)
		return res;
	if(param_no == 1) {
		((struct hname_data *)*param)->oper = hnoRemove2;
	}
	return 0;
}

static int ki_remove_hf_value2(sip_msg_t *msg, str *hexp, str *val)
{
	return ki_modify_hf(msg, hexp, val, remove_hf_value2_fixup,
			insupddel_hf_value_f);
}

static int assign_hf_value2_fixup(void **param, int param_no)
{
	int res = assign_hf_value_fixup(param, param_no);
	if(res < 0)
		return res;
	if(param_no == 1) {
		((struct hname_data *)*param)->oper = hnoAssign2;
	}
	return 0;
}

static int ki_assign_hf_value2(sip_msg_t *msg, str *hexp, str *val)
{
	return ki_modify_hf(msg, hexp, val, assign_hf_value2_fixup,
			insupddel_hf_value_f);
}

/* select implementation */
static int sel_hf_value(str *res, select_t *s, struct sip_msg *msg)
{ /* dummy */
	return 0;
}

#define _ALLOC_INC_SIZE 1024

static int sel_hf_value_name(str *res, select_t *s, struct sip_msg *msg)
{
	struct hname_data *hname;
	struct hdr_field *hf;
	str val, hval1, hval2, huri, dummy_name;
	int r;
	if(!msg) {
		struct hdr_field hdr;
		char buf[50];
		int i, n;

		if(s->params[1].type == SEL_PARAM_STR) {
			hname = pkg_malloc(sizeof(*hname));
			if(!hname)
				return E_OUT_OF_MEM;
			memset(hname, 0, sizeof(*hname));

			for(i = s->params[1].v.s.len - 1; i > 0; i--) {
				if(s->params[1].v.s.s[i] == '_')
					s->params[1].v.s.s[i] = '-';
			}
			i = snprintf(buf, sizeof(buf) - 1, "%.*s: X\n",
					s->params[1].v.s.len, s->params[1].v.s.s);
			buf[i] = 0;

			hname->hname = s->params[1].v.s;
			parse_hname2(buf, buf + i, &hdr);

			if(hdr.type == HDR_ERROR_T) {
				pkg_free(hname);
				return E_CFG;
			}
			hname->htype = hdr.type;

			s->params[1].v.p = hname;
			s->params[1].type = SEL_PARAM_PTR;
		} else {
			hname = s->params[1].v.p;
		}
		n = s->param_offset[select_level + 1]
			- s->param_offset
					  [select_level]; /* number of values before NESTED */
		if(n > 2 && s->params[2].type == SEL_PARAM_INT) {
			hname->idx = s->params[2].v.i;
			hname->flags |= HNF_IDX;
			if(hname->idx < -MAX_HF_VALUE_STACK) {
				LM_ERR("index cannot be lower than %d\n", -MAX_HF_VALUE_STACK);
				return E_CFG;
			}
			if(hname->idx == 0)
				hname->idx = 1;
			i = 3;
		} else {
			i = 2;
			hname->idx = 1;
		}
		if(n > i && s->params[i].type == SEL_PARAM_STR) {
			hname->param = s->params[i].v.s;
			for(i = hname->param.len - 1; i > 0; i--) {
				if(hname->param.s[i] == '_')
					hname->param.s[i] = '-';
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

	switch(hname->oper) {
		case hnoGetValueUri:
			if(hname->flags & HNF_ALL || (hname->flags & HNF_IDX) == 0) {
				char *buf = NULL;
				int buf_len = 0;

				hf = 0;
				do {
					r = find_next_hf(msg, hname, &hf);
					if(r < 0)
						break;
					if(hf) {
						char *p;
						str huri;
						hval2.len = 0;
						p = hf->body.s;
						do {
							r = find_next_value(&p, hf->body.s + hf->body.len,
									&hval1, &hval2);
							get_uri_and_skip_until_params(
									&hval1, &dummy_name, &huri);
							if(huri.len) {
								/* TODO: normalize uri, lowercase except quoted params, add/strip < > */
								if(*huri.s == '<') {
									huri.s++;
									huri.len -= 2;
								}
							}
							if(res->len == 0) {
								*res = huri; /* first value, if is also last value then we don't need any buffer */
							} else {
								if(buf) {
									if(res->len + huri.len + 1 > buf_len) {
										buf_len = res->len + huri.len + 1
												  + _ALLOC_INC_SIZE;
										res->s = pkg_realloc(buf, buf_len);
										if(!res->s) {
											pkg_free(buf);
											LM_ERR("cannot realloc buffer\n");
											res->len = 0;
											return E_OUT_OF_MEM;
										}
										buf = res->s;
									}
								} else {
									/* 2nd value */
									buf_len = res->len + huri.len + 1
											  + _ALLOC_INC_SIZE;
									buf = pkg_malloc(buf_len);
									if(!buf) {
										LM_ERR("out of memory\n");
										res->len = 0;
										return E_OUT_OF_MEM;
									}
									/* copy 1st value */
									memcpy(buf, res->s, res->len);
									res->s = buf;
								}
								res->s[res->len] = ',';
								res->len++;
								if(huri.len) {
									memcpy(res->s + res->len, huri.s, huri.len);
									res->len += huri.len;
								}
							}

						} while(r);
					}
				} while(hf);
				if(buf) {
					res->s = get_static_buffer(res->len);
					if(!res->s) {
						pkg_free(buf);
						res->len = 0;
						LM_ERR("cannot allocate static buffer\n");
						return E_OUT_OF_MEM;
					}
					memcpy(res->s, buf, res->len);
					pkg_free(buf);
				}
			} else {
				r = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if(r > 0) {
					get_uri_and_skip_until_params(&hval1, &dummy_name, res);
					if(res->len && *res->s == '<') {
						res->s++; /* strip < & > */
						res->len -= 2;
					}
				}
			}
			break;
		case hnoGetValueName:
			if((hname->flags & HNF_ALL) == 0) {
				r = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if(r > 0) {
					get_uri_and_skip_until_params(&hval1, res, &dummy_name);
					if(res->len >= 2 && res->s[0] == '\"'
							&& res->s[res->len - 1] == '\"') {
						res->s++; /* strip quotes */
						res->len -= 2;
					}
				}
			}
			break;
		case hnoGetValue:
			if(hname->flags & HNF_ALL || (hname->flags & HNF_IDX) == 0) {
				char *buf = NULL;
				int buf_len = 0;

				hf = 0;
				do {
					r = find_next_hf(msg, hname, &hf);

					if(r < 0)
						break;
					if(hf) {
						char *p;
						hval2.len = 0;
						p = hf->body.s;
						do {
							r = find_next_value(&p, hf->body.s + hf->body.len,
									&hval1, &hval2);
							if(res->len == 0) {
								*res = hval1; /* first value, if is also last value then we don't need any buffer */
							} else {
								if(buf) {
									if(res->len + hval1.len + 1 > buf_len) {
										buf_len = res->len + hval1.len + 1
												  + _ALLOC_INC_SIZE;
										res->s = pkg_realloc(buf, buf_len);
										if(!res->s) {
											pkg_free(buf);
											LM_ERR("cannot realloc buffer\n");
											res->len = 0;
											return E_OUT_OF_MEM;
										}
										buf = res->s;
									}
								} else {
									/* 2nd value */
									buf_len = res->len + hval1.len + 1
											  + _ALLOC_INC_SIZE;
									buf = pkg_malloc(buf_len);
									if(!buf) {
										LM_ERR("out of memory\n");
										res->len = 0;
										return E_OUT_OF_MEM;
									}
									/* copy 1st value */
									memcpy(buf, res->s, res->len);
									res->s = buf;
								}
								res->s[res->len] = ',';
								res->len++;
								if(hval1.len) {
									memcpy(res->s + res->len, hval1.s,
											hval1.len);
									res->len += hval1.len;
								}
							}
						} while(r);
					}
				} while(hf);
				if(buf) {
					res->s = get_static_buffer(res->len);
					if(!res->s) {
						pkg_free(buf);
						res->len = 0;
						LM_ERR("cannot allocate static buffer\n");
						return E_OUT_OF_MEM;
					}
					memcpy(res->s, buf, res->len);
					pkg_free(buf);
				}
			} else {
				r = find_hf_value_idx(msg, hname, &hf, &hval1, &hval2);
				if(r > 0) {
					if(hname->param.len) {
						str d1, d2;
						get_uri_and_skip_until_params(
								&hval1, &dummy_name, &huri);
						if(find_hf_value_param(hname, &hval1, &val, &d1, &d2)) {
							*res = val;
						}
					} else {
						*res = hval1;
					}
				}
			}
			break;
		case hnoGetValue2:
			r = find_hf_value_idx(msg, hname, &hf, 0, 0);
			if(r > 0) {
				if(hname->param.len) {
					str d1, d2;
					char c;
					if(find_hf_value2_param(
							   hname, &hf->body, &val, &d1, &d2, &c)) {
						*res = val;
					}
				} else {
					*res = hf->body;
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

static int sel_hf_value_name_param_name(
		str *res, select_t *s, struct sip_msg *msg)
{
	return sel_hf_value_name(res, s, msg);
}

static int sel_hf_value_name_param_name2(
		str *res, select_t *s, struct sip_msg *msg)
{
	if(!msg) { /* eliminate "param" level */
		int n;
		n = s->param_offset[select_level + 1] - s->param_offset[select_level];
		s->params[n - 2] = s->params[n - 1];
	}
	return sel_hf_value_name(res, s, msg);
}

static int sel_hf_value_name_uri(str *res, select_t *s, struct sip_msg *msg)
{
	int r;
	r = sel_hf_value_name(res, s, msg);
	if(!msg && r == 0) {
		((struct hname_data *)s->params[1].v.p)->oper = hnoGetValueUri;
	}
	return r;
}

static int sel_hf_value_name_name(str *res, select_t *s, struct sip_msg *msg)
{
	int r;
	r = sel_hf_value_name(res, s, msg);
	if(!msg && r == 0) {
		((struct hname_data *)s->params[1].v.p)->oper = hnoGetValueName;
	}
	return r;
}

static int sel_hf_value_exists(str *res, select_t *s, struct sip_msg *msg)
{ /* dummy */
	return 0;
}

static int sel_hf_value_exists_param(str *res, select_t *s, struct sip_msg *msg)
{
	static char ret_val[] = "01";
	int r;

	if(!msg) {
		r = sel_hf_value_name(res, s, msg);
		if(r == 0)
			((struct hname_data *)s->params[1].v.p)->oper = hnoIsIncluded;
		return r;
	}
	r = incexc_hf_value_str_f(msg, s->params[1].v.p, &s->params[2].v.s);
	res->s = &ret_val[r > 0];
	res->len = 1;

	return 0;
}

static int sel_hf_value2(str *res, select_t *s, struct sip_msg *msg)
{ /* dummy */
	return 0;
}

static int sel_hf_value2_name(str *res, select_t *s, struct sip_msg *msg)
{
	int r;
	r = sel_hf_value_name(res, s, msg);
	if(!msg && r == 0) {
		((struct hname_data *)s->params[1].v.p)->oper = hnoGetValue2;
	}
	return r;
}

static int sel_hf_value2_name_param_name(
		str *res, select_t *s, struct sip_msg *msg)
{
	return sel_hf_value2_name(res, s, msg);
}

SELECT_F(select_any_nameaddr)
SELECT_F(select_any_uri)
SELECT_F(select_anyheader_params)

select_row_t sel_declaration[] = {
		{NULL, SEL_PARAM_STR, STR_STATIC_INIT("hf_value"), sel_hf_value,
				SEL_PARAM_EXPECTED},

		{sel_hf_value, SEL_PARAM_STR, STR_NULL, sel_hf_value_name,
				CONSUME_NEXT_INT | OPTIONAL | FIXUP_CALL},
		{sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("param"),
				sel_hf_value_name_param_name2, CONSUME_NEXT_STR | FIXUP_CALL},
		{sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("p"),
				sel_hf_value_name_param_name2, CONSUME_NEXT_STR | FIXUP_CALL},
		{sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("uri"),
				sel_hf_value_name_uri, FIXUP_CALL},
		{sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("name"),
				sel_hf_value_name_name, FIXUP_CALL},
		{sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"),
				select_any_nameaddr,
				NESTED | CONSUME_NEXT_STR}, /* it duplicates param,p,name,... */
		{sel_hf_value_name, SEL_PARAM_STR, STR_STATIC_INIT("params"),
				select_anyheader_params, NESTED},

		{sel_hf_value_name_uri, SEL_PARAM_INT, STR_NULL, select_any_uri,
				NESTED},
		{sel_hf_value_name, SEL_PARAM_STR, STR_NULL,
				sel_hf_value_name_param_name, FIXUP_CALL},

		{NULL, SEL_PARAM_STR, STR_STATIC_INIT("hf_value_exists"),
				sel_hf_value_exists, CONSUME_NEXT_STR | SEL_PARAM_EXPECTED},
		{sel_hf_value_exists, SEL_PARAM_STR, STR_NULL,
				sel_hf_value_exists_param, FIXUP_CALL},

		{NULL, SEL_PARAM_STR, STR_STATIC_INIT("hf_value2"), sel_hf_value2,
				SEL_PARAM_EXPECTED},
		{sel_hf_value2, SEL_PARAM_STR, STR_NULL, sel_hf_value2_name,
				CONSUME_NEXT_INT | OPTIONAL | FIXUP_CALL},
		{sel_hf_value2_name, SEL_PARAM_STR, STR_STATIC_INIT("params"),
				select_anyheader_params, NESTED},
		{sel_hf_value2_name, SEL_PARAM_STR, STR_NULL,
				sel_hf_value2_name_param_name, FIXUP_CALL},
		{sel_hf_value2_name_param_name, SEL_PARAM_STR,
				STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED},
		{sel_hf_value2_name_param_name, SEL_PARAM_STR, STR_STATIC_INIT("uri"),
				select_any_uri, NESTED},

		{NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_textopsx_exports[] = {
	{ str_init("textopsx"), str_init("msg_apply_changes"),
		SR_KEMIP_INT, ki_msg_apply_changes,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("msg_set_buffer"),
		SR_KEMIP_INT, ki_msg_set_buffer,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("change_reply_status"),
		SR_KEMIP_INT, ki_change_reply_status,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("remove_body"),
		SR_KEMIP_INT, ki_remove_body,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("keep_hf"),
		SR_KEMIP_INT, ki_keep_hf,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("keep_hf_re"),
		SR_KEMIP_INT, ki_keep_hf_re,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("fnmatch"),
		SR_KEMIP_INT, ki_fnmatch,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("fnmatch_ex"),
		SR_KEMIP_INT, ki_fnmatch_ex,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("append_hf_value"),
		SR_KEMIP_INT, ki_append_hf_value,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("insert_hf_value"),
		SR_KEMIP_INT, ki_insert_hf_value,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("assign_hf_value"),
		SR_KEMIP_INT, ki_assign_hf_value,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("assign_hf_value2"),
		SR_KEMIP_INT, ki_assign_hf_value2,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("remove_hf_value"),
		SR_KEMIP_INT, ki_remove_hf_value,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("remove_hf_value2"),
		SR_KEMIP_INT, ki_remove_hf_value2,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("include_hf_value"),
		SR_KEMIP_INT, ki_include_hf_value,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("exclude_hf_value"),
		SR_KEMIP_INT, ki_exclude_hf_value,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("textopsx"), str_init("hf_value_exists"),
		SR_KEMIP_INT, ki_hf_value_exists,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_textopsx_exports);
	return 0;
}
