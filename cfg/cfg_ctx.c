/*
 * $Id$
 *
 * Copyright (C) 2007 iptelorg GmbH
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History
 * -------
 *  2007-12-03	Initial version (Miklos)
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../ut.h"
#include "cfg_struct.h"
#include "cfg_ctx.h"


/* linked list of all the registered cfg contexts */
static cfg_ctx_t	*cfg_ctx_list = NULL;

/* creates a new config context that is an interface to the
 * cfg variables with write permission
 */
int cfg_register_ctx(cfg_ctx_t **handle, cfg_on_declare on_declare_cb)
{
	cfg_ctx_t	*ctx;
	cfg_group_t	*group;
	str		gname;

	/* allocate memory for the new context
	Better to use shm mem, because 'changed' and 'lock'
	must be in shm mem anyway */
	ctx = (cfg_ctx_t *)shm_malloc(sizeof(cfg_ctx_t));
	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_register_ctx(): not enough shm memory\n");
		return -1;
	}
	memset(ctx, 0, sizeof(cfg_ctx_t));
	if (lock_init(&ctx->lock) == 0) {
		LOG(L_ERR, "ERROR: cfg_register_ctx(): failed to init lock\n");
		shm_free(ctx);
		return -1;
	}

	/* add the new ctx to the beginning of the list */
	ctx->next = cfg_ctx_list;
	cfg_ctx_list = ctx;

	/* let the driver know about the already registered groups
	 * The handle of the context must be set before calling the
	 * on_declare callbacks. */
	*handle = ctx;
	if (on_declare_cb) {
		ctx->on_declare_cb = on_declare_cb;

		for (	group = cfg_group;
			group;
			group = group->next
		) {
			/* dynamic groups are not ready, the callback
			will be called later when the group is fixed-up */
			if (group->dynamic) continue;

			gname.s = group->name;
			gname.len = group->name_len;
			on_declare_cb(&gname, group->mapping->def);
		}
	}

	return 0;
}

/* free the memory allocated for the contexts */
void cfg_ctx_destroy(void)
{
	cfg_ctx_t	*ctx, *ctx2;

	for (	ctx = cfg_ctx_list;
		ctx;
		ctx = ctx2
	) {
		ctx2 = ctx->next;
		shm_free(ctx);
	}
	cfg_ctx_list = NULL;
}

/* notify the drivers about the new config definition */
void cfg_notify_drivers(char *group_name, int group_name_len, cfg_def_t *def)
{
	cfg_ctx_t	*ctx;
	str		gname;

	gname.s = group_name;
	gname.len = group_name_len;

	for (	ctx = cfg_ctx_list;
		ctx;
		ctx = ctx->next
	)
		if (ctx->on_declare_cb)
			ctx->on_declare_cb(&gname, def);
}

/* placeholder for a temporary string */
static char	*temp_string = NULL;

/* convert the value to the requested type */
int convert_val(unsigned int val_type, void *val,
			unsigned int var_type, void **new_val)
{
	static str	s;
	char		*end;
	int		i;
	static char	buf[INT2STR_MAX_LEN];

	/* we have to convert from val_type to var_type */
	switch (CFG_INPUT_MASK(var_type)) {
	case CFG_INPUT_INT:
		if (val_type == CFG_VAR_INT) {
			*new_val = val;
			break;

		} else if (val_type == CFG_VAR_STRING) {
			if (!val || (((char *)val)[0] == '\0')) {
				LOG(L_ERR, "ERROR: convert_val(): "
					"cannot convert NULL string value to integer\n");
				return -1;
			}
			*new_val = (void *)(long)strtol((char *)val, &end, 10);
			if (*end != '\0') {
				LOG(L_ERR, "ERROR: convert_val(): "
					"cannot convert string to integer '%s'\n",
					(char *)val);
				return -1;
			}
			break;

		} else if (val_type == CFG_VAR_STR) {
			if (!((str *)val)->len || !((str *)val)->s) {
				LOG(L_ERR, "ERROR: convert_val(): "
					"cannot convert NULL str value to integer\n");
				return -1;
			}
			if (str2sint((str *)val, &i)) {
				LOG(L_ERR, "ERROR: convert_val(): "
					"cannot convert string to integer '%.*s'\n",
					((str *)val)->len, ((str *)val)->s);
				return -1;
			}
			*new_val = (void *)(long)i;
			break;
		}
		goto error;

	case CFG_INPUT_STRING:
		if (val_type == CFG_VAR_INT) {
			buf[snprintf(buf, sizeof(buf)-1, "%ld", (long)val)] = '\0';
			*new_val = buf;
			break;

		} else if (val_type == CFG_VAR_STRING) {
			*new_val = val;
			break;

		} else if (val_type == CFG_VAR_STR) {
			if (!((str *)val)->s) {
				*new_val = NULL;
				break;
			}
			/* the value may not be zero-terminated, thus,
			a new variable has to be allocated with larger memory space */
			if (temp_string) pkg_free(temp_string);
			temp_string = (char *)pkg_malloc(sizeof(char) * (((str *)val)->len + 1));
			if (!temp_string) {
				LOG(L_ERR, "ERROR: convert_val(): not enough memory\n");
				return -1;
			}
			memcpy(temp_string, ((str *)val)->s, ((str *)val)->len);
			temp_string[((str *)val)->len] = '\0';
			*new_val = (void *)temp_string;
			break;

		}
		goto error;

	case CFG_INPUT_STR:
		if (val_type == CFG_VAR_INT) {
			s.len = snprintf(buf, sizeof(buf)-1, "%ld", (long)val);
			buf[s.len] = '\0';
			s.s = buf;
			*new_val = (void *)&s;
			break;

		} else if (val_type == CFG_VAR_STRING) {
			s.s = (char *)val;
			s.len = (s.s) ? strlen(s.s) : 0;
			*new_val = (void *)&s;
			break;

		} else if (val_type == CFG_VAR_STR) {
			*new_val = val;
			break;			
		}
		goto error;
	}

	return 0;

error:
	LOG(L_ERR, "ERROR: convert_val(): got a value with type %u, but expected %u\n",
			val_type, CFG_INPUT_MASK(var_type));
	return -1;
}

#define convert_val_cleanup() \
	do { \
		if (temp_string) { \
			pkg_free(temp_string); \
			temp_string = NULL; \
		} \
	} while(0)

/* returns the size of the variable */
static int cfg_var_size(cfg_mapping_t *var)
{
	switch (CFG_VAR_TYPE(var)) {

	case CFG_VAR_INT:
		return sizeof(int);

	case CFG_VAR_STRING:
		return sizeof(char *);

	case CFG_VAR_STR:
		return sizeof(str);

	case CFG_VAR_POINTER:
		return sizeof(void *);

	default:
		LOG(L_CRIT, "BUG: cfg_var_size(): unknown type: %u\n",
			CFG_VAR_TYPE(var));
		return 0;
	}
}

/* Update the varibales of the array within the meta structure
 * with the new default value.
 * The array is cloned before a change if clone is set to 1.
 */
static int cfg_update_defaults(cfg_group_meta_t	*meta,
				cfg_group_t *group, cfg_mapping_t *var, char *new_val,
				int clone)
{
	int	i, clone_done=0;
	cfg_group_inst_t *array, *ginst;

	array = meta->array;
	for (i = 0; i < meta->num; i++) {
		ginst = (cfg_group_inst_t *)((char *)array
			+ (sizeof(cfg_group_meta_t) + group->size - 1) * i);

		if (!CFG_VAR_TEST(ginst, var)) {
			/* The variable uses the default value, it needs to be rewritten. */
			if (clone && !clone_done) {
				/* The array needs to be cloned before the modification */
				if (!(array = cfg_clone_array(meta, group)))
					return -1;
				ginst = translate_pointer(array, meta->array, ginst);
				/* re-link the array to the meta-data */
				meta->array = array;
				clone_done = 1;
			}
			memcpy(ginst->vars + var->offset, new_val, cfg_var_size(var)); 
		}
	}
	return 0;
}

/* sets the value of a variable without the need of commit
 *
 * return value:
 *   0: success
 *  -1: error
 *   1: variable has not been found
 */
int cfg_set_now(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			void *val, unsigned int val_type)
{
	int		i;
	cfg_group_t	*group;
	cfg_mapping_t	*var;
	void		*p, *v;
	cfg_block_t	*block = NULL;
	str		s, s2;
	char		*old_string = NULL;
	void		**replaced = NULL;
	cfg_child_cb_t	*child_cb = NULL;
	cfg_group_inst_t	*group_inst, *new_array = NULL;
	unsigned char		*var_block;

	/* verify the context even if we do not need it now
	to make sure that a cfg driver has called the function
	(very very weak security) */
	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_set_now(): context is undefined\n");
		return -1;
	}

	if (group_id && !cfg_shmized) {
		/* The config group has not been shmized yet,
		but an additional instance of a variable needs to be added to the group.
		Add this instance to the linked list of variables, they
		will be fixed later. */
		/* TODO */
		return -1;
	}

	/* look-up the group and the variable */
	if (cfg_lookup_var(group_name, var_name, &group, &var))
		return 1;
		
	/* check whether the variable is read-only */
	if (var->def->type & CFG_READONLY) {
		LOG(L_ERR, "ERROR: cfg_set_now(): variable is read-only\n");
		goto error0;
	}

	/* The additional variable instances having per-child process callback
	 * with CFG_CB_ONLY_ONCE flag cannot be rewritten.
	 * The reason is that such variables typically set global parameters
	 * as opposed to per-process variables. Hence, it is not possible to set
	 * the group handle temporary to another block, and then reset it back later. */
	if (group_id
		&& var->def->on_set_child_cb
		&& var->def->type & CFG_CB_ONLY_ONCE
	) {
		LOG(L_ERR, "ERROR: cfg_set_now(): This variable does not support muliple values.\n");
		goto error0;
	}

	/* check whether we have to convert the type */
	if (convert_val(val_type, val, CFG_INPUT_TYPE(var), &v))
		goto error0;
	
	if ((CFG_INPUT_TYPE(var) == CFG_INPUT_INT) 
	&& (var->def->min || var->def->max)) {
		/* perform a simple min-max check for integers */
		if (((int)(long)v < var->def->min)
		|| ((int)(long)v > var->def->max)) {
			LOG(L_ERR, "ERROR: cfg_set_now(): integer value is out of range\n");
			goto error0;
		}
	}

	if (var->def->on_change_cb) {
		/* Call the fixup function.
		There is no need to set a temporary cfg handle,
		becaue a single variable is changed */
		if (!group_id) {
			var_block = *(group->handle);
		} else {
			if (!cfg_local) {
				LOG(L_ERR, "ERROR: cfg_set_now(): Local configuration is missing\n");
				goto error0;
			}
			group_inst = cfg_find_group(CFG_GROUP_META(cfg_local, group),
							group->size,
							*group_id);
			if (!group_inst) {
				LOG(L_ERR, "ERROR: cfg_set_now(): local group instance %.*s[%u] is not found\n",
					group_name->len, group_name->s, *group_id);
				goto error0;
			}
			var_block = group_inst->vars;
		}

		if (var->def->on_change_cb(var_block,
						group_name,
						var_name,
						&v) < 0) {
			LOG(L_ERR, "ERROR: cfg_set_now(): fixup failed\n");
			goto error0;
		}

	}

	/* Set the per-child process callback only if the default value is changed.
	 * The callback of other instances will be called when the config is
	 * switched to that instance. */
	if (!group_id && var->def->on_set_child_cb) {
		/* get the name of the variable from the internal struct,
		because var_name may be freed before the callback needs it */
		s.s = group->name;
		s.len = group->name_len;
		s2.s = var->def->name;
		s2.len = var->name_len;
		child_cb = cfg_child_cb_new(&s, &s2,
					var->def->on_set_child_cb,
					var->def->type);
		if (!child_cb) {
			LOG(L_ERR, "ERROR: cfg_set_now(): not enough shm memory\n");
			goto error0;
		}
	}

	if (cfg_shmized) {
		/* make sure that nobody else replaces the global config
		while the new one is prepared */
		CFG_WRITER_LOCK();

		if (group_id) {
			group_inst = cfg_find_group(CFG_GROUP_META(*cfg_global, group),
							group->size,
							*group_id);
			if (!group_inst) {
				LOG(L_ERR, "ERROR: cfg_set_now(): global group instance %.*s[%u] is not found\n",
					group_name->len, group_name->s, *group_id);
				goto error;
			}
			var_block = group_inst->vars;
		} else {
			group_inst = NULL;
			var_block = CFG_GROUP_DATA(*cfg_global, group);
		}

		if (var->def->type & CFG_ATOMIC) {
			/* atomic change is allowed, we can rewrite the value
			directly in the global config */
			p = var_block + var->offset;

		} else {
			/* clone the memory block, and prepare the modification */
			if (!(block = cfg_clone_global())) goto error;

			if (group_inst) {
				/* The additional array of the group needs to be also cloned.
				 * When any of the variables within this array is changed, then
				 * the complete config block and this array is replaced. */
				if (!(new_array = cfg_clone_array(CFG_GROUP_META(*cfg_global, group), group)))
					goto error;
				group_inst = translate_pointer(new_array,
					CFG_GROUP_META(*cfg_global, group)->array,
					group_inst);
				var_block = group_inst->vars;
				CFG_GROUP_META(block, group)->array = new_array;
			} else {
				/* The additional array may need to be replaced depending
				 * on whether or not there is any variable in the array set
				 * to the default value which is changed now. If this is the case,
				 * then the array will be replaced later when the variables are
				 * updated.
				 */
				var_block = CFG_GROUP_DATA(block, group);
			}
			p = var_block + var->offset;
		}
	} else {
		/* we are allowed to rewrite the value on-the-fly
		The handle either points to group->vars, or to the
		shared memory block (dynamic group) */
		p = *(group->handle) + var->offset;
	}

	/* set the new value */
	switch (CFG_VAR_TYPE(var)) {
	case CFG_VAR_INT:
		*(int *)p = (int)(long)v;
		break;

	case CFG_VAR_STRING:
		/* clone the string to shm mem */
		s.s = v;
		s.len = (s.s) ? strlen(s.s) : 0;
		if (cfg_clone_str(&s, &s)) goto error;
		old_string = *(char **)p;
		*(char **)p = s.s;
		break;

	case CFG_VAR_STR:
		/* clone the string to shm mem */
		s = *(str *)v;
		if (cfg_clone_str(&s, &s)) goto error;
		old_string = *(char **)p;
		memcpy(p, &s, sizeof(str));
		break;

	case CFG_VAR_POINTER:
		*(void **)p = v;
		break;

	}
	if (group_inst && !CFG_VAR_TEST_AND_SET(group_inst, var))
		old_string = NULL; /* the string is the same as the default one,
					it cannot be freed */

	if (cfg_shmized) {
		if (!group_inst && block && CFG_GROUP_META(block, group)->array) {
			if (cfg_update_defaults(CFG_GROUP_META(block, group),
						group, var, p,
						((var->def->type & CFG_ATOMIC) == 0)) /* clone if needed */
			) {
				LOG(L_ERR, "ERROR: cfg_set_now(): not enough shm memory\n");
				goto error;
			}
			if (CFG_GROUP_META(block, group)->array != CFG_GROUP_META(*cfg_global, group)->array)
				new_array = CFG_GROUP_META(block, group)->array;
		}

		if (old_string || new_array) {
			/* prepare the array of the replaced strings,
			and replaced group instances,
			they will be freed when the old block is freed */
			replaced = (void **)shm_malloc(sizeof(void *)
					* ((old_string?1:0) + (new_array?1:0) + 1));
			if (!replaced) {
				LOG(L_ERR, "ERROR: cfg_set_now(): not enough shm memory\n");
				goto error;
			}
			i = 0;
			if (old_string) {
				replaced[i] = old_string;
				i++;
			}
			if (new_array) {	
				replaced[i] = CFG_GROUP_META(*cfg_global, group)->array;
				i++;
			}
			replaced[i] = NULL;
		}
		/* replace the global config with the new one */
		if (block) cfg_install_global(block, replaced, child_cb, child_cb);
		CFG_WRITER_UNLOCK();
	} else {
		/* cfg_set() may be called more than once before forking */
		if (old_string && (var->flag & cfg_var_shmized))
			shm_free(old_string);

		/* flag the variable because there is no need
		to shmize it again */
		var->flag |= cfg_var_shmized;

		/* the global config does not have to be replaced,
		but the child callback has to be installed, otherwise the
		child processes will miss the change */
		if (child_cb)
			cfg_install_child_cb(child_cb, child_cb);
	}

	if (val_type == CFG_VAR_INT)
		LOG(L_INFO, "INFO: cfg_set_now(): %.*s.%.*s "
			"has been changed to %d\n",
			group_name->len, group_name->s,
			var_name->len, var_name->s,
			(int)(long)val);

	else if (val_type == CFG_VAR_STRING)
		LOG(L_INFO, "INFO: cfg_set_now(): %.*s.%.*s "
			"has been changed to \"%s\"\n",
			group_name->len, group_name->s,
			var_name->len, var_name->s,
			(char *)val);

	else /* str type */
		LOG(L_INFO, "INFO: cfg_set_now(): %.*s.%.*s "
			"has been changed to \"%.*s\"\n",
			group_name->len, group_name->s,
			var_name->len, var_name->s,
			((str *)val)->len, ((str *)val)->s);
	if (group_id)
		LOG(L_INFO, "INFO: cfg_set_now(): group id = %u\n",
			*group_id);

	convert_val_cleanup();
	return 0;

error:
	if (cfg_shmized) CFG_WRITER_UNLOCK();
	if (block) cfg_block_free(block);
	if (new_array) shm_free(new_array);
	if (child_cb) cfg_child_cb_free(child_cb);
	if (replaced) shm_free(replaced);

error0:
	LOG(L_ERR, "ERROR: cfg_set_now(): failed to set the variable: %.*s.%.*s\n",
			group_name->len, group_name->s,
			var_name->len, var_name->s);


	convert_val_cleanup();
	return -1;
}

/* wrapper function for cfg_set_now */
int cfg_set_now_int(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			int val)
{
	return cfg_set_now(ctx, group_name, group_id, var_name,
				(void *)(long)val, CFG_VAR_INT);
}

/* wrapper function for cfg_set_now */
int cfg_set_now_string(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			char *val)
{
	return cfg_set_now(ctx, group_name, group_id, var_name,
				(void *)val, CFG_VAR_STRING);
}

/* wrapper function for cfg_set_now */
int cfg_set_now_str(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			str *val)
{
	return cfg_set_now(ctx, group_name, group_id, var_name,
				(void *)val, CFG_VAR_STR);
}

/* sets the value of a variable but does not commit the change
 *
 * return value:
 *   0: success
 *  -1: error
 *   1: variable has not been found
 */
int cfg_set_delayed(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			void *val, unsigned int val_type)
{
	cfg_group_t	*group;
	cfg_mapping_t	*var;
	void		*v;
	char		*temp_handle;
	int		temp_handle_created;
	cfg_changed_var_t	*changed = NULL;
	int		size;
	str		s;

	if (!cfg_shmized)
		/* the cfg has not been shmized yet, there is no
		point in registering the change and committing it later */
		return cfg_set_now(ctx, group_name, group_id, var_name,
					val, val_type);

	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_set_delayed(): context is undefined\n");
		return -1;
	}

	/* look-up the group and the variable */
	if (cfg_lookup_var(group_name, var_name, &group, &var))
		return 1;

	/* check whether the variable is read-only */
	if (var->def->type & CFG_READONLY) {
		LOG(L_ERR, "ERROR: cfg_set_delayed(): variable is read-only\n");
		goto error0;
	}

	/* check whether we have to convert the type */
	if (convert_val(val_type, val, CFG_INPUT_TYPE(var), &v))
		goto error0;

	if ((CFG_INPUT_TYPE(var) == CFG_INPUT_INT) 
	&& (var->def->min || var->def->max)) {
		/* perform a simple min-max check for integers */
		if (((int)(long)v < var->def->min)
		|| ((int)(long)v > var->def->max)) {
			LOG(L_ERR, "ERROR: cfg_set_delayed(): integer value is out of range\n");
			goto error0;
		}
	}

	/* the ctx must be locked while reading and writing
	the list of changed variables */
	CFG_CTX_LOCK(ctx);

	if (var->def->on_change_cb) {
		/* The fixup function must see also the
		not yet committed values, so a temporary handle
		must be prepared that points to the new config.
		Only the values within the group are applied,
		other modifications are not visible to the callback.
		The local config is the base. */

		if (ctx->changed_first) {
			temp_handle = (char *)pkg_malloc(group->size);
			if (!temp_handle) {
				LOG(L_ERR, "ERROR: cfg_set_delayed(): "
					"not enough memory\n");
				goto error;
			}
			temp_handle_created = 1;
			memcpy(temp_handle, *(group->handle), group->size);

			/* apply the changes */
			for (	changed = ctx->changed_first;
				changed;
				changed = changed->next
			) {
				if (changed->group != group) continue;

				memcpy(	temp_handle + changed->var->offset,
					changed->new_val.vraw,
					cfg_var_size(changed->var));
			}
		} else {
			/* there is not any change */
			temp_handle = *(group->handle);
			temp_handle_created = 0;
		}
			
		if (var->def->on_change_cb(temp_handle,
						group_name,
						var_name,
						&v) < 0) {
			LOG(L_ERR, "ERROR: cfg_set_delayed(): fixup failed\n");
			if (temp_handle_created) pkg_free(temp_handle);
			goto error;
		}
		if (temp_handle_created) pkg_free(temp_handle);

	}

	/* everything went ok, we can add the new value to the list */
	size = sizeof(cfg_changed_var_t) -
			sizeof(((cfg_changed_var_t*)0)->new_val) + cfg_var_size(var);
	changed = (cfg_changed_var_t *)shm_malloc(size);
	if (!changed) {
		LOG(L_ERR, "ERROR: cfg_set_delayed(): not enough shm memory\n");
		goto error;
	}
	memset(changed, 0, size);
	changed->group = group;
	changed->var = var;

	switch (CFG_VAR_TYPE(var)) {

	case CFG_VAR_INT:
		changed->new_val.vint = (int)(long)v;
		break;

	case CFG_VAR_STRING:
		/* clone the string to shm mem */
		s.s = v;
		s.len = (s.s) ? strlen(s.s) : 0;
		if (cfg_clone_str(&s, &s)) goto error;
		changed->new_val.vp = s.s;
		break;

	case CFG_VAR_STR:
		/* clone the string to shm mem */
		s = *(str *)v;
		if (cfg_clone_str(&s, &s)) goto error;
		changed->new_val.vstr=s;
		break;

	case CFG_VAR_POINTER:
		changed->new_val.vp=v;
		break;

	}

	/* Add the new item to the end of the linked list,
	The commit will go though the list from the first item,
	so the list is kept in order */
	if (ctx->changed_first)
		ctx->changed_last->next = changed;
	else
		ctx->changed_first = changed;

	ctx->changed_last = changed;

	CFG_CTX_UNLOCK(ctx);

	if (val_type == CFG_VAR_INT)
		LOG(L_INFO, "INFO: cfg_set_delayed(): %.*s.%.*s "
			"is going to be changed to %d "
			"[context=%p]\n",
			group_name->len, group_name->s,
			var_name->len, var_name->s,
			(int)(long)val,
			ctx);

	else if (val_type == CFG_VAR_STRING)
		LOG(L_INFO, "INFO: cfg_set_delayed(): %.*s.%.*s "
			"is going to be changed to \"%s\" "
			"[context=%p]\n",
			group_name->len, group_name->s,
			var_name->len, var_name->s,
			(char *)val,
			ctx);

	else /* str type */
		LOG(L_INFO, "INFO: cfg_set_delayed(): %.*s.%.*s "
			"is going to be changed to \"%.*s\" "
			"[context=%p]\n",
			group_name->len, group_name->s,
			var_name->len, var_name->s,
			((str *)val)->len, ((str *)val)->s,
			ctx);

	convert_val_cleanup();
	return 0;

error:
	CFG_CTX_UNLOCK(ctx);
	if (changed) shm_free(changed);
error0:
	LOG(L_ERR, "ERROR: cfg_set_delayed(): failed to set the variable: %.*s.%.*s\n",
			group_name->len, group_name->s,
			var_name->len, var_name->s);

	convert_val_cleanup();
	return -1;
}

/* wrapper function for cfg_set_delayed */
int cfg_set_delayed_int(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
				int val)
{
	return cfg_set_delayed(ctx, group_name, group_id, var_name,
				(void *)(long)val, CFG_VAR_INT);
}

/* wrapper function for cfg_set_delayed */
int cfg_set_delayed_string(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
				char *val)
{
	return cfg_set_delayed(ctx, group_name, group_id, var_name,
				(void *)val, CFG_VAR_STRING);
}

/* wrapper function for cfg_set_delayed */
int cfg_set_delayed_str(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
				str *val)
{
	return cfg_set_delayed(ctx, group_name, group_id, var_name,
				(void *)val, CFG_VAR_STR);
}

/* commits the previously prepared changes within the context */
int cfg_commit(cfg_ctx_t *ctx)
{
	int	replaced_num = 0;
	cfg_changed_var_t	*changed, *changed2;
	cfg_block_t	*block;
	void	**replaced = NULL;
	cfg_child_cb_t	*child_cb;
	cfg_child_cb_t	*child_cb_first = NULL;
	cfg_child_cb_t	*child_cb_last = NULL;
	int	size;
	void	*p;
	str	s, s2;

	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_commit(): context is undefined\n");
		return -1;
	}

	if (!cfg_shmized) return 0; /* nothing to do */

	/* the ctx must be locked while reading and writing
	the list of changed variables */
	CFG_CTX_LOCK(ctx);

	/* is there any change? */
	if (!ctx->changed_first) goto done;

	/* count the number of replaced strings,
	and prepare the linked list of per-child process
	callbacks, that will be added to the global list */
	for (	changed = ctx->changed_first;
		changed;
		changed = changed->next
	) {
		if ((CFG_VAR_TYPE(changed->var) == CFG_VAR_STRING)
		|| (CFG_VAR_TYPE(changed->var) == CFG_VAR_STR))
			replaced_num++;


		if (changed->var->def->on_set_child_cb) {
			s.s = changed->group->name;
			s.len = changed->group->name_len;
			s2.s = changed->var->def->name;
			s2.len = changed->var->name_len;
			child_cb = cfg_child_cb_new(&s, &s2,
					changed->var->def->on_set_child_cb,
					changed->var->def->type);
			if (!child_cb) goto error0;

			if (child_cb_last)
				child_cb_last->next = child_cb;
			else
				child_cb_first = child_cb;
			child_cb_last = child_cb;
		}
	}

	if (replaced_num) {
		/* allocate memory for the replaced string array */
		size = sizeof(void *)*(replaced_num + 1);
		replaced = (void **)shm_malloc(size);
		if (!replaced) {
			LOG(L_ERR, "ERROR: cfg_commit(): not enough shm memory\n");
			goto error;
		}
		memset(replaced, 0 , size);
	}

	/* make sure that nobody else replaces the global config
	while the new one is prepared */
	CFG_WRITER_LOCK();

	/* clone the memory block, and prepare the modification */
	if (!(block = cfg_clone_global())) {
		CFG_WRITER_UNLOCK();
		goto error;
	}

	/* apply the modifications to the buffer */
	replaced_num = 0;
	for (	changed = ctx->changed_first;
		changed;
		changed = changed->next
	) {
		p = CFG_GROUP_DATA(block, changed->group)
			+ changed->var->offset;

		if ((CFG_VAR_TYPE(changed->var) == CFG_VAR_STRING)
		|| (CFG_VAR_TYPE(changed->var) == CFG_VAR_STR)) {
			replaced[replaced_num] = *(char **)p;
			if (replaced[replaced_num])
				replaced_num++;
			/* else do not increase replaced_num, because
			the cfg_block_free() will stop at the first
			NULL value */
		}

		memcpy(	p,
			changed->new_val.vraw,
			cfg_var_size(changed->var));
	}

	/* replace the global config with the new one */
	cfg_install_global(block, replaced, child_cb_first, child_cb_last);
	CFG_WRITER_UNLOCK();

	/* free the changed list */	
	for (	changed = ctx->changed_first;
		changed;
		changed = changed2
	) {
		changed2 = changed->next;
		shm_free(changed);
	}
	ctx->changed_first = NULL;
	ctx->changed_last = NULL;

done:
	LOG(L_INFO, "INFO: cfg_commit(): config changes have been applied "
			"[context=%p]\n",
			ctx);

	CFG_CTX_UNLOCK(ctx);
	return 0;

error:
	CFG_CTX_UNLOCK(ctx);

error0:

	if (child_cb_first) cfg_child_cb_free(child_cb_first);
	if (replaced) shm_free(replaced);

	return -1;
}

/* drops the not yet committed changes within the context */
int cfg_rollback(cfg_ctx_t *ctx)
{
	cfg_changed_var_t	*changed, *changed2;

	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_rollback(): context is undefined\n");
		return -1;
	}

	if (!cfg_shmized) return 0; /* nothing to do */

	LOG(L_INFO, "INFO: cfg_rollback(): deleting the config changes "
			"[context=%p]\n",
			ctx);

	/* the ctx must be locked while reading and writing
	the list of changed variables */
	CFG_CTX_LOCK(ctx);

	for (	changed = ctx->changed_first;
		changed;
		changed = changed2
	) {
		changed2 = changed->next;

		if ((CFG_VAR_TYPE(changed->var) == CFG_VAR_STRING)
		|| (CFG_VAR_TYPE(changed->var) == CFG_VAR_STR)) {
			if (changed->new_val.vp)
				shm_free(changed->new_val.vp);
		}
		shm_free(changed);
	}
	ctx->changed_first = NULL;
	ctx->changed_last = NULL;

	CFG_CTX_UNLOCK(ctx);

	return 0;
}

/* retrieves the value of a variable
 * Return value:
 *  0 - success
 * -1 - error
 *  1 - variable exists, but it is not readable
 */
int cfg_get_by_name(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			void **val, unsigned int *val_type)
{
	cfg_group_t	*group;
	cfg_mapping_t	*var;
	void		*p;
	static str	s;	/* we need the value even
				after the function returns */
	cfg_group_inst_t	*group_inst;

	/* verify the context even if we do not need it now
	to make sure that a cfg driver has called the function
	(very very weak security) */
	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_get_by_name(): context is undefined\n");
		return -1;
	}

	/* look-up the group and the variable */
	if (cfg_lookup_var(group_name, var_name, &group, &var))
		return -1;

	if (var->def->on_change_cb) {
		/* The variable cannot be retrieved, because the fixup
		function may have changed it, and it is better to return
		an error than an incorrect value */
		return 1;
	}

	if (group_id) {
		if (!cfg_local) {
			LOG(L_ERR, "ERROR: cfg_get_by_name(): Local configuration is missing\n");
			return -1;
		}
		group_inst = cfg_find_group(CFG_GROUP_META(cfg_local, group),
						group->size,
						*group_id);
		if (!group_inst) {
			LOG(L_ERR, "ERROR: cfg_get_by_name(): local group instance %.*s[%u] is not found\n",
				group_name->len, group_name->s, *group_id);
			return -1;
		}
		p = group_inst->vars + var->offset;

	} else {
		/* use the module's handle to access the variable
		It means that the variable is read from the local config
		after forking */
		p = *(group->handle) + var->offset;
	}

	switch (CFG_VAR_TYPE(var)) {
	case CFG_VAR_INT:
		*val = (void *)(long)*(int *)p;
		break;

	case CFG_VAR_STRING:
		*val = (void *)*(char **)p;
		break;

	case CFG_VAR_STR:
		memcpy(&s, p, sizeof(str));
		*val = (void *)&s;
		break;

	case CFG_VAR_POINTER:
		*val = *(void **)p;
		break;

	}
	*val_type = CFG_VAR_TYPE(var);

	return 0;
}

/* returns the description of a variable */
int cfg_help(cfg_ctx_t *ctx, str *group_name, str *var_name,
			char **ch, unsigned int *input_type)
{
	cfg_mapping_t	*var;

	/* verify the context even if we do not need it now
	to make sure that a cfg driver has called the function
	(very very weak security) */
	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_help(): context is undefined\n");
		return -1;
	}

	/* look-up the group and the variable */
	if (cfg_lookup_var(group_name, var_name, NULL, &var))
		return -1;

	*ch = var->def->descr;
	if (input_type)
		*input_type = CFG_INPUT_TYPE(var);
	return 0;
}

/* return the group name and the cfg structure definition,
 * and moves the handle to the next group
 * Return value:
 *	0: no more group
 *	1: group exists
 */
int cfg_get_group_next(void **h,
			str *gname, cfg_def_t **def)
{
	cfg_group_t	*group;

	group = (cfg_group_t *)(*h);
	if (group == NULL) return 0;

	gname->s = group->name;
	gname->len = group->name_len;
	(*def) = group->mapping->def;

	(*h) = (void *)group->next;
	return 1;
}

/* Initialize the handle for cfg_diff_next() */
int cfg_diff_init(cfg_ctx_t *ctx,
		void **h)
{
	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_diff_init(): context is undefined\n");
		return -1;
	}

	CFG_CTX_LOCK(ctx);
	(*h) = (void *)ctx->changed_first;

	return 0;
}

/* return the pending changes that have not been
 * committed yet
 */
int cfg_diff_next(void **h,
			str *gname, str *vname,
			void **old_val, void **new_val,
			unsigned int *val_type)
{
	cfg_changed_var_t	*changed;
	union cfg_var_value* pval;
	static str	old_s, new_s;	/* we need the value even
					after the function returns */

	changed = (cfg_changed_var_t *)(*h);
	if (changed == NULL) return 0;

	gname->s = changed->group->name;
	gname->len = changed->group->name_len;
	vname->s = changed->var->def->name;
	vname->len = changed->var->name_len;

	/* use the module's handle to access the variable
	It means that the variable is read from the local config
	after forking */
	pval = (union cfg_var_value*)
			(*(changed->group->handle) + changed->var->offset);

	switch (CFG_VAR_TYPE(changed->var)) {
	case CFG_VAR_INT:
		*old_val = (void *)(long)pval->vint;
		*new_val = (void *)(long)changed->new_val.vint;
		break;

	case CFG_VAR_STRING:
		*old_val = pval->vp;
		*new_val = changed->new_val.vp;
		break;

	case CFG_VAR_STR:
		old_s=pval->vstr;
		*old_val = (void *)&old_s;
		new_s=changed->new_val.vstr;
		*new_val = (void *)&new_s;
		break;

	case CFG_VAR_POINTER:
		*old_val = pval->vp;
		*new_val = changed->new_val.vp;
		break;

	}
	*val_type = CFG_VAR_TYPE(changed->var);

	(*h) = (void *)changed->next;
	return 1;
}

/* release the handle of cfg_diff_next() */
void cfg_diff_release(cfg_ctx_t *ctx)
{
	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_diff_release(): context is undefined\n");
		return;
	}

	CFG_CTX_UNLOCK(ctx);
}

/* Add a new instance to an existing group */
int cfg_add_group_inst(cfg_ctx_t *ctx, str *group_name, unsigned int group_id)
{
	cfg_group_t	*group;
	cfg_block_t	*block = NULL;
	void		**replaced = NULL;
	cfg_group_inst_t	*new_array = NULL, *new_inst;

	/* verify the context even if we do not need it now
	to make sure that a cfg driver has called the function
	(very very weak security) */
	if (!ctx) {
		LOG(L_ERR, "ERROR: cfg_add_group_inst(): context is undefined\n");
		return -1;
	}

	if (!cfg_shmized) {
		/* TODO: Add a new fake variable belonging to
		the additional group instance to the linked list. */
		return -1;
	}

	if (!(group = cfg_lookup_group(group_name->s, group_name->len))) {
		LOG(L_ERR, "ERROR: cfg_add_group_inst(): group not found\n");
		return -1;
	}

	/* make sure that nobody else replaces the global config
	while the new one is prepared */
	CFG_WRITER_LOCK();
	if (cfg_find_group(CFG_GROUP_META(*cfg_global, group),
							group->size,
							group_id)
	) {
		LOG(L_DBG, "DEBUG: cfg_add_group_inst(): the group instance already exists\n");
		CFG_WRITER_UNLOCK();
		return 0; /* not an error */
	}

	/* clone the global memory block because the additional array can be
	replaced only together with the block. */
	if (!(block = cfg_clone_global()))
		goto error;

	/* Extend the array with a new group instance */
	if (!(new_array = cfg_extend_array(CFG_GROUP_META(*cfg_global, group), group,
					group_id,
					&new_inst))
	)
		goto error;

	/* fill in the new group instance with the default data */
	memcpy(	new_inst->vars,
		CFG_GROUP_DATA(*cfg_global, group),
		group->size);

	CFG_GROUP_META(block, group)->array = new_array;
	CFG_GROUP_META(block, group)->num++;

	if (CFG_GROUP_META(*cfg_global, group)->array) {
		/* prepare the array of the replaced strings,
		and replaced group instances,
		they will be freed when the old block is freed */
		replaced = (void **)shm_malloc(sizeof(void *) * 2);
		if (!replaced) {
			LOG(L_ERR, "ERROR: cfg_add_group_inst(): not enough shm memory\n");
			goto error;
		}
		replaced[0] = CFG_GROUP_META(*cfg_global, group)->array;
		replaced[1] = NULL;
	}
	/* replace the global config with the new one */
	cfg_install_global(block, replaced, NULL, NULL);
	CFG_WRITER_UNLOCK();

	LOG(L_INFO, "INFO: cfg_add_group_inst(): "
		"group instance is added: %.*s[%u]\n",
		group_name->len, group_name->s,
		group_id);

	return 0;
error:
	CFG_WRITER_UNLOCK();
	if (block) cfg_block_free(block);
	if (new_array) shm_free(new_array);
	if (replaced) shm_free(replaced);

	LOG(L_ERR, "ERROR: cfg_add_group_inst(): "
		"Failed to add the group instance: %.*s[%u]\n",
		group_name->len, group_name->s,
		group_id);

	return -1;
}
