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
#include "re.h"
#include "dprint.h"

#include "ppcfg.h"

typedef struct _pp_subst_rule {
	char *indata;
	void *ppdata;
	struct _pp_subst_rule *next;
} pp_subst_rule_t;

static pp_subst_rule_t *pp_subst_rules_head = NULL;
static pp_subst_rule_t *pp_subst_rules_tail = NULL;
static int _pp_ifdef_level = 0;

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
		LM_ERR("no more pkg\n");
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

	LM_INFO("### added subst expression: %s\n", data);

	return 0;
}

int pp_substdef_add(char *data, int mode)
{
	char c;
	char *p;
	str defname;
	str defvalue;

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

	pp_define_set_type(0);
	if(pp_define(defname.len, defname.s)<0) {
		LM_ERR("cannot set define name\n");
		goto error;
	}
	if(mode==1) {
		/* define the value enclosed in double quotes */
		*(defvalue.s-1) = '"';
		defvalue.s[defvalue.len] = '"';
		defvalue.s--;
		defvalue.len += 2;
	}
	if(pp_define_set(defvalue.len, defvalue.s)<0) {
		LM_ERR("cannot set define value\n");
		goto error;
	}
	if(mode==1) {
		defvalue.s++;
		defvalue.len -= 2;
		*(defvalue.s-1) = c;
		defvalue.s[defvalue.len] = c;
	}

	LM_DBG("### added substdef: [%.*s]=[%.*s] (%d)\n", defname.len, defname.s,
			defvalue.len, defvalue.s, mode);

	return 0;

error:
	return 1;
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
		result=subst_str(*data, 0,
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
void pp_ifdef_level_check(void)
{
	if(_pp_ifdef_level!=0) {
		LM_WARN("different number of preprocessor directives:"
				" N(#!IF[N]DEF) - N(#!ENDIF) = %d\n", _pp_ifdef_level);
	} else {
		LM_DBG("same number of pairing preprocessor directives"
			" #!IF[N]DEF - #!ENDIF\n");
	}
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
