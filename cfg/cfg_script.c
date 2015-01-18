/*
 * Copyright (C) 2008 iptelorg GmbH
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

#include <string.h>

#include "../mem/mem.h"
#include "../ut.h"
#include "cfg_struct.h"
#include "cfg.h"
#include "cfg_ctx.h"
#include "cfg_script.h"

/* allocates memory for a new config script variable
 * The value of the variable is not set!
 */
cfg_script_var_t *new_cfg_script_var(char *gname, char *vname, unsigned int type,
					char *descr)
{
	cfg_group_t	*group;
	cfg_script_var_t	*var;
	int	gname_len, vname_len, descr_len;

	LOG(L_DBG, "DEBUG: new_cfg_script_var(): declaring %s.%s\n", gname, vname);

	if (cfg_shmized) {
		LOG(L_ERR, "ERROR: new_cfg_script_var(): too late variable declaration, "
			"the config has been already shmized\n");
		return NULL;
	}

	gname_len = strlen(gname);
	vname_len = strlen(vname);
	/* the group may have been already declared */
	group = cfg_lookup_group(gname, gname_len);
	if (group) {
		if (group->dynamic == CFG_GROUP_STATIC) {
			/* the group has been already declared by a module or by the core */
			LOG(L_ERR, "ERROR: new_cfg_script_var(): "
				"configuration group has been already declared: %s\n",
				gname);
			return NULL;
		}
		/* the dynamic or empty group is found */
		/* verify that the variable does not exist */
		for (	var = (cfg_script_var_t *)group->vars;
			var;
			var = var->next
		) {
			if ((var->name_len == vname_len) &&
			(memcmp(var->name, vname, vname_len) == 0)) {
				LOG(L_ERR, "ERROR: new_cfg_script_var(): variable already exists: %s.%s\n",
						gname, vname);
				return NULL;
			}
		}
		if (group->dynamic == CFG_GROUP_UNKNOWN)
			group->dynamic = CFG_GROUP_DYNAMIC;

	} else {
		/* create a new group with NULL values, we will fix it later,
		when all the variables are known */
		group = cfg_new_group(gname, gname_len,
					0 /* num */, NULL /* mapping */,
					NULL /* vars */, 0 /* size */, NULL /* handle */);
					
		if (!group) goto error;
		group->dynamic = CFG_GROUP_DYNAMIC;
	}

	switch (type) {
	case CFG_VAR_INT:
		group->size = ROUND_INT(group->size);
		group->size += sizeof(int);
		break;

	case CFG_VAR_STR:
		group->size = ROUND_POINTER(group->size);
		group->size += sizeof(str);
		break;

	default:
		LOG(L_ERR, "ERROR: new_cfg_script_var(): unsupported variable type\n");
		return NULL;
	}

	group->num++;
	if (group->num > CFG_MAX_VAR_NUM) {
		LOG(L_ERR, "ERROR: new_cfg_script_var(): too many variables (%d) within a single group,"
			" the limit is %d. Increase CFG_MAX_VAR_NUM, or split the group into multiple"
			" definitions.\n",
			group->num, CFG_MAX_VAR_NUM);
		return NULL;
	}

	var = (cfg_script_var_t *)pkg_malloc(sizeof(cfg_script_var_t));
	if (!var) goto error;
	memset(var, 0, sizeof(cfg_script_var_t));
	var->type = type;

	/* add the variable to the group */
	var->next = (cfg_script_var_t *)(void *)group->vars;
	group->vars = (char *)(void *)var;

	/* clone the name of the variable */
	var->name = (char *)pkg_malloc(sizeof(char) * (vname_len + 1));
	if (!var->name) goto error;
	memcpy(var->name, vname, vname_len + 1);
	var->name_len = vname_len;

	if (descr) {
		/* save the description */
		descr_len = strlen(descr);
		var->descr = (char *)pkg_malloc(sizeof(char) * (descr_len + 1));
		if (!var->descr) goto error;
		memcpy(var->descr, descr, descr_len + 1);
	}

	return var;

error:
	LOG(L_ERR, "ERROR: new_cfg_script_var(): not enough memory\n");
	return NULL;
}

/* Rewrite the value of an already declared script variable before forking.
 * Return value:
 * 	 0: success
 *	-1: error
 *	 1: variable not found
 */
int cfg_set_script_var(cfg_group_t *group, str *var_name,
			void *val, unsigned int val_type)
{
	cfg_script_var_t	*var;
	void	*v;
	str	s;

	if (cfg_shmized || (group->dynamic != CFG_GROUP_DYNAMIC)) {
		LOG(L_ERR, "BUG: cfg_set_script_var(): Not a dynamic group before forking\n");
		return -1;
	}

	for (	var = (cfg_script_var_t *)(void *)group->vars;
		var;
		var = var->next
	) {
		if ((var->name_len == var_name->len)
			&& (memcmp(var->name, var_name->s, var_name->len) == 0)
		) {
			switch (var->type) {
			case CFG_VAR_INT:
				if (convert_val(val_type, val, CFG_INPUT_INT, &v))
					goto error;
				if ((var->min || var->max)
					&& ((var->min > (int)(long)v) || (var->max < (int)(long)v))
				) {
					LOG(L_ERR, "ERROR: cfg_set_script_var(): integer value is out of range\n");
					goto error;
				}
				var->val.i = (int)(long)v;
				break;

			case CFG_VAR_STR:
				if (convert_val(val_type, val, CFG_INPUT_STR, &v))
					goto error;
				if (((str *)v)->s) {
					s.len = ((str *)v)->len;
					s.s = pkg_malloc(sizeof(char) * (s.len + 1));
					if (!s.s) {
						LOG(L_ERR, "ERROR: cfg_set_script_var(): not enough memory\n");
						goto error;
					}
					memcpy(s.s, ((str *)v)->s, s.len);
					s.s[s.len] = '\0';
				} else {
					s.s = NULL;
					s.len = 0;
				}
				if (var->val.s.s)
					pkg_free(var->val.s.s);
				var->val.s = s;
				break;

			default:
				LOG(L_ERR, "ERROR: cfg_set_script_var(): unsupported variable type\n");
				goto error;
			}

			convert_val_cleanup();
			return 0;
		}
	}

	return 1;

error:
	LOG(L_ERR, "ERROR: cfg_set_script_var(): failed to set the script variable: %.*s.%.*s\n",
			group->name_len, group->name,
			var_name->len, var_name->s);
	return -1;
}

/* fix-up the dynamically declared group:
 *  - allocate memory for the arrays
 *  - set the values within the memory block
 */
int cfg_script_fixup(cfg_group_t *group, unsigned char *block)
{
	cfg_mapping_t		*mapping = NULL;
	cfg_def_t		*def = NULL;
	void			**handle = NULL;
	int			i, offset;
	cfg_script_var_t	*script_var, *script_var2;
	str			s;

	mapping = (cfg_mapping_t *)pkg_malloc(sizeof(cfg_mapping_t)*group->num);
	if (!mapping) goto error;
	memset(mapping, 0, sizeof(cfg_mapping_t)*group->num);

	/* The variable definition array must look like as if it was declared
	 * in C code, thus, add an additional slot at the end with NULL values */
	def = (cfg_def_t *)pkg_malloc(sizeof(cfg_def_t)*(group->num + 1));
	if (!def) goto error;
	memset(def, 0, sizeof(cfg_def_t)*(group->num + 1));

	/* fill the definition and the mapping arrays */
	offset = 0;
	for (	i = 0, script_var = (cfg_script_var_t *)group->vars;
		script_var;
		i++, script_var = script_var->next
	) {
		/* there has been already memory allocated for the name */
		def[i].name = script_var->name;
		def[i].type = script_var->type | (script_var->type << CFG_INPUT_SHIFT);
		def[i].descr = script_var->descr;
		def[i].min = script_var->min;
		def[i].max = script_var->max;

		mapping[i].def = &(def[i]);
		mapping[i].name_len = script_var->name_len;
		mapping[i].pos = i;

		switch (script_var->type) {
		case CFG_VAR_INT:
			offset = ROUND_INT(offset);
			mapping[i].offset = offset;

			*(int *)(block + offset) = script_var->val.i;

			offset += sizeof(int);
			break;

		case CFG_VAR_STR:
			offset = ROUND_POINTER(offset);
			mapping[i].offset = offset;

			if (cfg_clone_str(&(script_var->val.s), &s)) goto error;
			memcpy(block + offset, &s, sizeof(str));
			mapping[i].flag |= cfg_var_shmized;

			offset += sizeof(str);
			break;
		}
	}

	/* allocate a handle even if it will not be used to
	directly access the variable, like handle->variable
	cfg_get_* functions access the memory block via the handle
	to make sure that it is always safe, thus, it must be created */
	handle = (void **)pkg_malloc(sizeof(void *));
	if (!handle) goto error;
	*handle = NULL;
	group->handle = handle;

	group->mapping = mapping;

	/* everything went fine, we can free the temporary list */
	script_var = (cfg_script_var_t *)group->vars;
	group->vars = NULL;
	while (script_var) {
		script_var2 = script_var->next;
		if ((script_var->type == CFG_VAR_STR) && script_var->val.s.s)
			pkg_free(script_var->val.s.s);
		pkg_free(script_var);
		script_var = script_var2;
	}

	return 0;

error:
	if (mapping) pkg_free(mapping);
	if (def) pkg_free(def);
	if (handle) pkg_free(handle);

	LOG(L_ERR, "ERROR: cfg_script_fixup(): not enough memory\n");
	return -1;
}

/* destory a dynamically allocated group definition */
void cfg_script_destroy(cfg_group_t *group)
{
	int	i;
	cfg_script_var_t	*script_var, *script_var2;

	if (group->mapping && group->mapping->def) {
		for (i=0; i<group->num; i++) {
			if (group->mapping->def[i].name)
				pkg_free(group->mapping->def[i].name);
			if (group->mapping->def[i].descr)
				pkg_free(group->mapping->def[i].descr);
		}
		pkg_free(group->mapping->def);
	}
	if (group->mapping) pkg_free(group->mapping);
	if (group->handle) pkg_free(group->handle);

	/* it may happen that the temporary var list
	still exists because the fixup failed and did not complete */
	script_var = (cfg_script_var_t *)group->vars;
	while (script_var) {
		script_var2 = script_var->next;
		if ((script_var->type == CFG_VAR_STR) && script_var->val.s.s) 
			pkg_free(script_var->val.s.s);
		pkg_free(script_var);
		script_var = script_var2;
	}
}
