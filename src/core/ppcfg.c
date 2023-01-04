/*
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*!
 * \file
 * \brief Kamailio core :: ppcfg.c - config preprocessor directives
 * \ingroup core
 * Module: \ref core
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "mem/mem.h"
#include "ut.h"
#include "trim.h"
#include "re.h"
#include "pvar.h"
#include "pvapi.h"
#include "str_list.h"
#include "dprint.h"
#include "utils/snexpr.h"

#include "ppcfg.h"
#include "fmsg.h"

typedef struct _pp_subst_rule {
	char *indata;
	void *ppdata;
	struct _pp_subst_rule *next;
} pp_subst_rule_t;

static pp_subst_rule_t *pp_subst_rules_head = NULL;
static pp_subst_rule_t *pp_subst_rules_tail = NULL;
static int _pp_ifdef_level = 0;
static str_list_t *_ksr_substdef_strlist = NULL;

int pp_def_qvalue(str *defval, str *outval)
{
	str newval;
	str_list_t *sb;

	if(pv_get_buffer_size() < defval->len + 4) {
		LM_ERR("defined value is too large %d < %d\n", pv_get_buffer_size(), defval->len + 4);
		return -1;
	}
	newval.s = pv_get_buffer();
	newval.s[0] = '"';
	memcpy(newval.s + 1, defval->s, defval->len);
	newval.s[defval->len + 1] = '"';
	newval.s[defval->len + 2] = '\0';
	newval.len = defval->len + 2;
	sb = str_list_block_add(&_ksr_substdef_strlist, newval.s, newval.len);
	if(sb==NULL) {
		LM_ERR("failed to link quoted value [%.*s]\n", defval->len, defval->s);
		return -1;
	}
	*outval = sb->s;

	return 0;
}

int pp_subst_add(char *data)
{
	struct subst_expr* se;
	str subst;
	pp_subst_rule_t *pr;

	subst.s = data;
	subst.len = strlen(subst.s);
	/* check for early invalid rule */
	if(subst.len<=0)
		return -1;
	pr = (pp_subst_rule_t*)pkg_malloc(sizeof(pp_subst_rule_t));
	if(pr==NULL)
	{
		PKG_MEM_ERROR;
		return -1;
	}
	memset(pr, 0, sizeof(pp_subst_rule_t));

	se=subst_parser(&subst);
	if (se==0)
	{
		LM_ERR("bad subst expression: %s\n", data);
		pkg_free(pr);
		return -2;
	}
	pr->indata = data;
	pr->ppdata = (void*)se;
	if(pp_subst_rules_head==NULL)
	{
		pp_subst_rules_head = pr;
	} else {
		pp_subst_rules_tail->next = pr;
	}
	pp_subst_rules_tail = pr;

	LM_DBG("### added subst expression: [%s]\n", data);

	return 0;
}

int pp_substdef_add(char *data, int mode)
{
	char c;
	char *p;
	str defname;
	str defvalue;
	str newval;
	sip_msg_t *fmsg;
	str_list_t *sb;

	if(pp_subst_add(data)<0) {
		LM_ERR("subst rule cannot be added\n");
		goto error;
	}

	p=data;
	c=*p;
	if (c=='\\') {
		LM_ERR("invalid separator char [%c] in [%s]\n", c, data);
		goto error;
	}
	p++;
	/* find regexp */
	defname.s=p;
	for ( ; *p; p++) {
		/* if unescaped sep. char */
		if ((*p==c) && (*(p-1)!='\\'))
			goto found_regexp;
	}
	LM_ERR("separator [%c] not found after regexp: [%s]\n", c, data);
	goto error;

found_regexp:
	defname.len = p - defname.s;
	if(defname.len==0) {
		LM_ERR("define name too short\n");
		goto error;
	}

	p++;
	defvalue.s = p;
	/* find replacement */
	for ( ; *p; p++) {
		/* if unescaped sep. char */
		if ((*p==c) && (*(p-1)!='\\'))
			goto found_repl;
	}
	LM_ERR("separator [%c] not found after replacement: [%s]\n", c, data);
	goto error;

found_repl:
	defvalue.len = p - defvalue.s;

	pp_define_set_type(KSR_PPDEF_DEFINE);
	if(pp_define(defname.len, defname.s)<0) {
		LM_ERR("cannot set define name\n");
		goto error;
	}
	if(memchr(defvalue.s, '$', defvalue.len) != NULL) {
		fmsg = faked_msg_get_next();
		if(pv_eval_str(fmsg, &newval, &defvalue)>=0) {
			if(mode!=KSR_PPDEF_QUOTED) {
				sb = str_list_block_add(&_ksr_substdef_strlist, newval.s, newval.len);
				if(sb==NULL) {
					LM_ERR("failed to handle substdef: [%s]\n", data);
					return -1;
				}
				defvalue = sb->s;
			} else {
				defvalue = newval;
			}
		}
	}
	if(mode==KSR_PPDEF_QUOTED) {
		if(pp_def_qvalue(&defvalue, &newval) < 0) {
			LM_ERR("failed to enclose in quotes the value\n");
			return -1;
		}
		defvalue = newval;
	}
	if(pp_define_set(defvalue.len, defvalue.s, KSR_PPDEF_QUOTED)<0) {
		LM_ERR("cannot set define value\n");
		goto error;
	}

	LM_DBG("### added substdef: [%.*s]=[%.*s] (%d)\n", defname.len, defname.s,
			defvalue.len, defvalue.s, mode);

	return 0;

error:
	return -1;
}

int pp_subst_run(char **data)
{
	str* result;
	pp_subst_rule_t *pr;
	int i;

	if(pp_subst_rules_head==NULL)
		return 0;
	if(data==NULL || *data==NULL)
		return 0;

	if(strlen(*data)==0)
		return 0;
	pr = pp_subst_rules_head;

	i = 0;
	while(pr)
	{
		sip_msg_t *fmsg = faked_msg_get_next();
		result=subst_str(*data, fmsg,
				(struct subst_expr*)pr->ppdata, 0); /* pkg malloc'ed result */
		if(result!=NULL)
		{
			i++;
			LM_DBG("preprocess subst applied [#%d] to [%s]"
					" - returning new string [%s]\n", i, *data, result->s);
			pkg_free(*data);
			*data = result->s;
			pkg_free(result);
		}
		pr = pr->next;
	}

	if(i!=0)
		return 1;
	return 0;
}

/**
 *
 */
void pp_ifdef_level_update(int val)
{
	_pp_ifdef_level += val;
}

/**
 *
 */
int pp_ifdef_level_check(void)
{
	if(_pp_ifdef_level!=0) {
		return -1;
	} else {
		LM_DBG("same number of pairing preprocessor directives"
			" #!IF[N]DEF - #!ENDIF\n");
	}
	return 0;
}

void pp_ifdef_level_error(void)
{
	if(_pp_ifdef_level!=0) {
		if (_pp_ifdef_level > 0) {
			LM_ERR("different number of preprocessor directives:"
				" %d more #!if[n]def as #!endif\n", _pp_ifdef_level);
		} else {
			LM_ERR("different number of preprocessor directives:"
				" %d more #!endif as #!if[n]def\n", (_pp_ifdef_level)*-1);
		}
	}
}

/**
 *
 */
void pp_define_core(void)
{
	char defval[64];
	char *p;
	int n;
	str_list_t *sb;

	strcpy(defval, NAME);
	p = defval;
	while(*p) {
		*p = (char)toupper(*p);
		p++;
	}

	n = snprintf(p, 64 - (int)(p-defval), "_%u", VERSIONVAL/1000000);
	if(n<0 || n>=64 - (int)(p-defval)) {
		LM_ERR("failed to build define token\n");
		return;
	}
	pp_define_set_type(KSR_PPDEF_DEFINE);
	if(pp_define(strlen(defval), defval)<0) {
		LM_ERR("unable to set cfg define: %s\n", defval);
		return;
	}

	n = snprintf(p, 64 - (int)(p-defval), "_%u_%u", VERSIONVAL/1000000,
			(VERSIONVAL%1000000)/1000);
	if(n<0 || n>=64 - (int)(p-defval)) {
		LM_ERR("failed to build define token\n");
		return;
	}
	pp_define_set_type(KSR_PPDEF_DEFINE);
	if(pp_define(strlen(defval), defval)<0) {
		LM_ERR("unable to set cfg define: %s\n", defval);
		return;
	}

	n = snprintf(p, 64 - (int)(p-defval), "_%u_%u_%u", VERSIONVAL/1000000,
			(VERSIONVAL%1000000)/1000, VERSIONVAL%1000);
	if(n<0 || n>=64 - (int)(p-defval)) {
		LM_ERR("failed to build define token\n");
		return;
	}
	pp_define_set_type(KSR_PPDEF_DEFINE);
	if(pp_define(strlen(defval), defval)<0) {
		LM_ERR("unable to set cfg define: %s\n", defval);
		return;
	}

	strcpy(p, "_VERSION");
	pp_define_set_type(KSR_PPDEF_DEFINE);
	if(pp_define(strlen(defval), defval)<0) {
		LM_ERR("unable to set cfg define: %s\n", defval);
		return;
	}

	n = snprintf(defval, 64, "%u", VERSIONVAL);
	if(n<0 || n>=64) {
		LM_ERR("failed to build version define value\n");
		return;
	}
	sb = str_list_block_add(&_ksr_substdef_strlist, defval, strlen(defval));
	if(sb==NULL) {
		LM_ERR("failed to store version define value\n");
		return;
	}
	if(pp_define_set(sb->s.len, sb->s.s, KSR_PPDEF_NORMAL)<0) {
		LM_ERR("error setting version define value\n");
		return;
	}

	if(pp_define(strlen("OS_NAME"), "OS_NAME")<0) {
		LM_ERR("unable to set cfg define OS_NAME\n");
		return;
	}
	if(pp_define_set(strlen(OS_QUOTED), OS_QUOTED, KSR_PPDEF_NORMAL)<0) {
		LM_ERR("error setting OS_NAME define value\n");
		return;
	}
}

static struct snexpr* pp_snexpr_defval(char *vname)
{
	int idx = 0;
	ksr_ppdefine_t *pd = NULL;

	if(vname==NULL) {
		return NULL;
	}

	idx = pp_lookup(strlen(vname), vname);
	if(idx < 0) {
		LM_DBG("define id [%s] not found - return 0\n", vname);
		return snexpr_convert_num(0, SNE_OP_CONSTNUM);
	}
	pd = pp_get_define(idx);
	if(pd == NULL) {
		LM_DBG("define id [%s] at index [%d] not found - return 0\n", vname, idx);
		return snexpr_convert_num(0, SNE_OP_CONSTNUM);
	}

	if(pd->value.s != NULL) {
		LM_DBG("define id [%s] at index [%d] found with value - return [%.*s]\n",
				vname, idx, pd->value.len, pd->value.s);
		if(pd->value.len>=2 && (pd->value.s[0]=='"' || pd->value.s[0]=='\'')
				&& pd->value.s[0]==pd->value.s[pd->value.len-1]) {
			/* strip enclosing quotes for string value */
			return snexpr_convert_stzl(pd->value.s+1, pd->value.len-2, SNE_OP_CONSTSTZ);
		} else {
			return snexpr_convert_stzl(pd->value.s, pd->value.len, SNE_OP_CONSTSTZ);
		}
	} else {
		LM_DBG("define id [%s] at index [%d] found without value - return 1\n",
				vname, idx);
		return snexpr_convert_num(1, SNE_OP_CONSTNUM);
	}
}

void pp_ifexp_eval(char *exval, int exlen)
{
	str exstr;
	struct snexpr_var_list vars = {0};
	struct snexpr *e = NULL;
	struct snexpr *result = NULL;
	int b = 0;

	exstr.s = exval;
	exstr.len = exlen;
	trim(&exstr);

	LM_DBG("evaluating [%.*s]\n", exstr.len, exstr.s);

	e = snexpr_create(exstr.s, exstr.len, &vars, NULL, pp_snexpr_defval);
	if(e == NULL) {
		LM_ERR("failed to create expression [%.*s]\n", exstr.len, exstr.s);
		pp_ifexp_state(0);
		return;
	}

	result = snexpr_eval(e);

	if(result==NULL) {
		LM_ERR("expression evaluation [%.*s] is null\n", exstr.len, exstr.s);
		pp_ifexp_state(0);
		goto end;
	}

	if(result->type == SNE_OP_CONSTNUM) {
		LM_DBG("expression number result: %g\n", result->param.num.nval);
		if(result->param.num.nval) {
			b = 1;
		} else {
			b = 0;
		}
	} else if(result->type == SNE_OP_CONSTSTZ) {
		if(result->param.stz.sval==NULL || strlen(result->param.stz.sval)==0) {
			LM_DBG("expression string result: <%s>\n",
					(result->param.stz.sval)?"empty":"null");
			b = 0;
		} else {
			LM_DBG("expression string result: [%s]\n", result->param.stz.sval);
			b = 1;
		}
	}

	LM_DBG("expression evaluation [%.*s] is [%s]\n", exstr.len, exstr.s,
			(b)?"true":"false");

	pp_ifexp_state(b);

	snexpr_result_free(result);
end:
	snexpr_destroy(e, &vars);
}

char *pp_defexp_eval(char *exval, int exlen, int qmode)
{
	str exstr;
	struct snexpr_var_list vars = {0};
	struct snexpr *e = NULL;
	struct snexpr *result = NULL;
	str sval = STR_NULL;
	char *res = NULL;

	exstr.s = exval;
	exstr.len = exlen;
	trim(&exstr);

	LM_DBG("evaluating [%.*s]\n", exstr.len, exstr.s);

	e = snexpr_create(exstr.s, exstr.len, &vars, NULL, pp_snexpr_defval);
	if(e == NULL) {
		LM_ERR("failed to create expression [%.*s]\n", exstr.len, exstr.s);
		return NULL;
	}

	result = snexpr_eval(e);

	if(result==NULL) {
		LM_ERR("expression evaluation [%.*s] is null\n", exstr.len, exstr.s);
		goto end;
	}

	if(result->type == SNE_OP_CONSTNUM) {
		LM_DBG("expression number result: %g\n", result->param.num.nval);
		sval.s = int2str((long)result->param.num.nval, &sval.len);
		if(sval.s==NULL) {
			goto done;
		}
	} else if(result->type == SNE_OP_CONSTSTZ) {
		if(result->param.stz.sval==NULL) {
			LM_DBG("expression string result is null\n");
			goto done;
		}
		LM_DBG("expression string result: [%s]\n", result->param.stz.sval);
		sval.s = result->param.stz.sval;
		sval.len = strlen(result->param.stz.sval);
	}

	if(qmode==1) {
		res = (char*)pkg_malloc(sval.len + 3);
	} else {
		res = (char*)pkg_malloc(sval.len + 1);
	}
	if(res==NULL) {
		PKG_MEM_ERROR;
		goto done;
	}
	if(qmode==1) {
		res[0] = '"';
		memcpy(res, sval.s+1, sval.len);
		res[sval.len+1] = '"';
		res[sval.len+2] = '\0';
		LM_DBG("expression quoted string result: [%s]\n", res);
	} else {
		memcpy(res, sval.s, sval.len);
		res[sval.len] = '\0';
	}

done:
	snexpr_result_free(result);
end:
	snexpr_destroy(e, &vars);
	return res;
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
