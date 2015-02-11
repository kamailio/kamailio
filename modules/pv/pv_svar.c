/*
 * Copyright (C) 2006 voice-system.ro
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
 * \brief Script variables
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "../../dprint.h"
#include "../../mem/mem.h"

#include "pv_svar.h"

static script_var_t *script_vars = 0;
static script_var_t *script_vars_null = 0;

script_var_t* add_var(str *name, int vtype)
{
	script_var_t *it;

	if(name==0 || name->s==0 || name->len<=0)
		return 0;

	if(vtype==VAR_TYPE_NULL) {
		it=script_vars_null;
	} else {
		it=script_vars;
	}
	for(; it; it=it->next)
	{
		if(it->name.len==name->len
				&& strncmp(name->s, it->name.s, name->len)==0)
			return it;
	}
	it = (script_var_t*)pkg_malloc(sizeof(script_var_t));
	if(it==0)
	{
		LM_ERR("out of pkg mem\n");
		return 0;
	}
	memset(it, 0, sizeof(script_var_t));
	it->name.s = (char*)pkg_malloc((name->len+1)*sizeof(char));

	if(it->name.s==0)
	{
		LM_ERR("out of pkg mem!\n");
		return 0;
	}
	it->name.len = name->len;
	strncpy(it->name.s, name->s, name->len);
	it->name.s[it->name.len] = '\0';

	if(vtype==VAR_TYPE_NULL) {
		it->v.flags = VAR_VAL_NULL|VAR_TYPE_NULL;
		it->next = script_vars_null;
		script_vars_null = it;
	} else {
		it->next = script_vars;
		script_vars = it;
	}

	return it;
}

script_var_t* set_var_value(script_var_t* var, int_str *value, int flags)
{
	if(var==0)
		return 0;
	if(value==NULL)
	{
		if(var->v.flags&VAR_VAL_STR)
		{
			pkg_free(var->v.value.s.s);
			var->v.flags &= ~VAR_VAL_STR;
		}

		if(var->v.flags&VAR_TYPE_NULL)
			var->v.flags |= VAR_VAL_NULL;

		memset(&var->v.value, 0, sizeof(int_str));

		return var;
	}

	var->v.flags &= ~VAR_VAL_NULL;
	if(flags&VAR_VAL_STR)
	{
		if(var->v.flags&VAR_VAL_STR)
		{ /* old and new value is str */
			if(value->s.len>var->v.value.s.len)
			{ /* not enough space to copy */
				pkg_free(var->v.value.s.s);
				memset(&var->v.value, 0, sizeof(int_str));
				var->v.value.s.s =
					(char*)pkg_malloc((value->s.len+1)*sizeof(char));
				if(var->v.value.s.s==0)
				{
					LM_ERR("out of pkg mem\n");
					goto error;
				}
			}
		} else {
			memset(&var->v.value, 0, sizeof(int_str));
			var->v.value.s.s =
					(char*)pkg_malloc((value->s.len+1)*sizeof(char));
			if(var->v.value.s.s==0)
			{
				LM_ERR("out of pkg mem!\n");
				goto error;
			}
			var->v.flags |= VAR_VAL_STR;
		}
		strncpy(var->v.value.s.s, value->s.s, value->s.len);
		var->v.value.s.len = value->s.len;
		var->v.value.s.s[value->s.len] = '\0';
	} else {
		if(var->v.flags&VAR_VAL_STR)
		{
			pkg_free(var->v.value.s.s);
			var->v.flags &= ~VAR_VAL_STR;
			memset(&var->v.value, 0, sizeof(int_str));
		}
		var->v.value.n = value->n;
	}

	return var;
error:
	/* set the var to init value */
	memset(&var->v.value, 0, sizeof(int_str));
	var->v.flags &= ~VAR_VAL_STR;
	return NULL;
}

script_var_t* get_var_by_name(str *name)
{
	script_var_t *it;

	if(name==0 || name->s==0 || name->len<=0)
		return 0;

	for(it=script_vars; it; it=it->next)
	{
		if(it->name.len==name->len
				&& strncmp(name->s, it->name.s, name->len)==0)
			return it;
	}
	return 0;
}

script_var_t* get_varnull_by_name(str *name)
{
	script_var_t *it;

	if(name==0 || name->s==0 || name->len<=0)
		return 0;

	for(it=script_vars_null; it; it=it->next)
	{
		if(it->name.len==name->len
				&& strncmp(name->s, it->name.s, name->len)==0)
			return it;
	}
	return 0;
}

void reset_vars(void)
{
	script_var_t *it;
	for(it=script_vars; it; it=it->next)
	{
		if(it->v.flags&VAR_VAL_STR)
		{
			pkg_free(it->v.value.s.s);
			it->v.flags &= ~VAR_VAL_STR;
		}
		memset(&it->v.value, 0, sizeof(int_str));
	}
	for(it=script_vars_null; it; it=it->next)
	{
		if(it->v.flags&VAR_VAL_STR)
		{
			pkg_free(it->v.value.s.s);
			it->v.flags &= ~VAR_VAL_STR;
		}
		it->v.flags |= VAR_VAL_NULL;
		memset(&it->v.value, 0, sizeof(int_str));
	}
}

void destroy_vars_list(script_var_t *svl)
{
	script_var_t *it;
	script_var_t *it0;

	it = svl;
	while(it)
	{
		it0 = it;
		it = it->next;
		pkg_free(it0->name.s);
		if(it0->v.flags&VAR_VAL_STR)
			pkg_free(it0->v.value.s.s);
		pkg_free(it0);
	}

	svl = 0;
}

void destroy_vars(void)
{
	destroy_vars_list(script_vars);
	destroy_vars_list(script_vars_null);
}
