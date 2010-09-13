/* 
 * $Id$
 * 
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
/*
 * ppcfg.c - config preprocessor directives
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
		LM_ERR("bad subst expression:: %s\n", data);
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

	return 0;
}

int pp_subst_run(char **data)
{
	str* result;
	pp_subst_rule_t *pr;

	if(pp_subst_rules_head==NULL)
		return 0;
	if(data==NULL || *data==NULL)
		return 0;

	if(strlen(*data)==0)
		return 0;
	pr = pp_subst_rules_head;

	while(pr)
	{
		result=subst_str(*data, 0,
				(struct subst_expr*)pr->ppdata, 0); /* pkg malloc'ed result */
		if(result!=NULL)
		{
			LM_DBG("### preprocess subst applied to [%s]"
					" - returning new string [%s]\n", *data, result->s);
			pkg_free(*data);
			*data = result->s;
			pkg_free(result);
			return 1;
		}
		pr = pr->next;
	}

	return 0;
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
