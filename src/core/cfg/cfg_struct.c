/*
 * Copyright (C) 2007 iptelorg GmbH
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
#include "../mem/shm_mem.h"
#include "../ut.h"
#include "../locking.h"
#include "../bit_scan.h"
#include "cfg_ctx.h"
#include "cfg_script.h"
#include "cfg_select.h"
#include "cfg_struct.h"

cfg_group_t	*cfg_group = NULL;	/* linked list of registered cfg groups */
cfg_block_t	**cfg_global = NULL;	/* pointer to the active cfg block */
cfg_block_t	*cfg_local = NULL;	/* per-process pointer to the active cfg block.
					Updated only when the child process
					finishes working on the SIP message */
int		cfg_block_size = 0;	/* size of the cfg block including the meta-data (constant) */
gen_lock_t	*cfg_global_lock = 0;	/* protects *cfg_global */
gen_lock_t	*cfg_writer_lock = 0;	/* This lock makes sure that two processes do not
					try to clone *cfg_global at the same time.
					Never try to get cfg_writer_lock when
					cfg_global_lock is held */
int		cfg_shmized = 0;	/* indicates whether the cfg block has been
					already shmized */

cfg_child_cb_t	**cfg_child_cb_first = NULL;	/* first item of the per-child process
						callback list */
cfg_child_cb_t	**cfg_child_cb_last = NULL;	/* last item of the above list */
cfg_child_cb_t	*cfg_child_cb = NULL;	/* pointer to the previously executed cb */	
int		cfg_ginst_count = 0;	/* number of group instances set within the child process */


/* forward declarations */
static void del_add_var_list(cfg_group_t *group);
static int apply_add_var_list(cfg_block_t *block, cfg_group_t *group);

/* creates a new cfg group, and adds it to the linked list */
cfg_group_t *cfg_new_group(char *name, int name_len,
		int num, cfg_mapping_t *mapping,
		char *vars, int size, void **handle)
{
	cfg_group_t	*group;

	if (cfg_shmized) {
		LOG(L_ERR, "ERROR: cfg_new_group(): too late config declaration\n");
		return NULL;
	}

	if (num > CFG_MAX_VAR_NUM) {
		LOG(L_ERR, "ERROR: cfg_new_group(): too many variables (%d) within a single group,"
			" the limit is %d. Increase CFG_MAX_VAR_NUM, or split the group into multiple"
			" definitions.\n",
			num, CFG_MAX_VAR_NUM);
		return NULL;
	}

	group = (cfg_group_t *)pkg_malloc(sizeof(cfg_group_t)+name_len-1);
	if (!group) {
		LOG(L_ERR, "ERROR: cfg_new_group(): not enough memory\n");
		return NULL;
	}
	memset(group, 0, sizeof(cfg_group_t)+name_len-1);

	group->num = num;
	group->mapping = mapping;
	group->vars = vars;
	group->size = size;
	group->handle = handle;
	if (handle)
		group->orig_handle = *handle;
	group->name_len = name_len;
	memcpy(&group->name, name, name_len);

	/* add the new group to the beginning of the list */
	group->next = cfg_group;
	cfg_group = group;

	return group;
}

/* Set the values of an existing cfg group. */
void cfg_set_group(cfg_group_t *group,
		int num, cfg_mapping_t *mapping,
		char *vars, int size, void **handle)
{
	group->num = num;
	group->mapping = mapping;
	group->vars = vars;
	group->size = size;
	group->handle = handle;
	if (handle)
		group->orig_handle = *handle;
}

/* clones a string to shared memory
 * (src and dst can be the same)
 */
int cfg_clone_str(str *src, str *dst)
{
	char	*c;

	if (!src->s) {
		dst->s = NULL;
		dst->len = 0;
		return 0;
	}

	c = (char *)shm_malloc(sizeof(char)*(src->len+1));
	if (!c) {
		LOG(L_ERR, "ERROR: cfg_clone_str(): not enough shm memory\n");
		return -1;
	}
	memcpy(c, src->s, src->len);
	c[src->len] = '\0';

	dst->s = c;
	dst->len = src->len;

	return 0;
}

/* copies the strings to shared memory */
static int cfg_shmize_strings(cfg_group_t *group)
{
	cfg_mapping_t	*mapping;
	int	i;
	str	s;

	/* We do not know in advance whether the variable will be changed or not,
	and it can happen that we try to free the shm memory area when the variable
	is changed, hence, it must be already in shm mem */
	mapping = group->mapping;
	for (i=0; i<group->num; i++) {
		/* the cfg driver module may have already shmized the variable */
		if (mapping[i].flag & cfg_var_shmized) continue;

		if (CFG_VAR_TYPE(&mapping[i]) == CFG_VAR_STRING) {
			s.s = *(char **)(group->vars + mapping[i].offset);
			if (!s.s) continue;
			s.len = strlen(s.s);

		} else if (CFG_VAR_TYPE(&mapping[i]) == CFG_VAR_STR) {
			memcpy(&s, group->vars + mapping[i].offset, sizeof(str));
			if (!s.s) continue;

		} else {
			continue;
		}
		if (cfg_clone_str(&s, &s)) return -1;
		*(char **)(group->vars + mapping[i].offset) = s.s;
		mapping[i].flag |= cfg_var_shmized;
	}

	return 0;
}

/* copy the variables to shm mem */
int cfg_shmize(void)
{
	cfg_group_t	*group;
	cfg_block_t	*block = NULL;
	int	size;

	if (!cfg_group) return 0;

	/* Let us allocate one memory block that
	 * will contain all the variables + meta-data
	 * in the following form:
	 * |-----------|
	 * | meta-data | <- group A: meta_offset
	 * | variables | <- group A: var_offset
	 * |-----------|
	 * | meta-data | <- group B: meta_offset
	 * | variables | <- group B: var_offset
	 * |-----------|
	 * |    ...    |
	 * |-----------|
	 *
	 * The additional array for the multiple values
	 * of the same variable is linked to the meta-data.
	 */
	for (	size=0, group = cfg_group;
		group;
		group=group->next
	) {
		size = ROUND_POINTER(size);
		group->meta_offset = size;
		size += sizeof(cfg_group_meta_t);

		size = ROUND_POINTER(size);
		group->var_offset = size;
		size += group->size;
	}

	block = (cfg_block_t*)shm_malloc(sizeof(cfg_block_t)+size-1);
	if (!block) {
		LOG(L_ERR, "ERROR: cfg_shmize(): not enough shm memory\n");
		goto error;
	}
	memset(block, 0, sizeof(cfg_block_t)+size-1);
	cfg_block_size = size;

	/* copy the memory fragments to the single block */
	for (	group = cfg_group;
		group;
		group=group->next
	) {
		if (group->dynamic == CFG_GROUP_STATIC) {
			/* clone the strings to shm mem */
			if (cfg_shmize_strings(group)) goto error;

			/* copy the values to the new block */
			memcpy(CFG_GROUP_DATA(block, group), group->vars, group->size);
		} else if (group->dynamic == CFG_GROUP_DYNAMIC) {
			/* The group was declared with NULL values,
			 * we have to fix it up.
			 * The fixup function takes care about the values,
			 * it fills up the block */
			if (cfg_script_fixup(group, CFG_GROUP_DATA(block, group))) goto error;

			/* Notify the drivers about the new config definition.
			 * Temporary set the group handle so that the drivers have a chance to
			 * overwrite the default values. The handle must be reset after this
			 * because the main process does not have a local configuration. */
			*(group->handle) = CFG_GROUP_DATA(block, group);
			cfg_notify_drivers(group->name, group->name_len,
					group->mapping->def);
			*(group->handle) = NULL;
		} else {
			LOG(L_ERR, "ERROR: cfg_shmize(): Configuration group is declared "
					"without any variable: %.*s\n",
					group->name_len, group->name);
			goto error;
		}

		/* Create the additional group instances with applying
		the temporary list. */
		if (apply_add_var_list(block, group))
			goto error;
	}

	/* try to fixup the selects that failed to be fixed-up previously */
	if (cfg_fixup_selects()) goto error;

	/* install the new config */
	cfg_install_global(block, NULL, NULL, NULL);
	cfg_shmized = 1;

	return 0;

error:
	if (block) shm_free(block);
	return -1;
}

/* deallocate the list of groups, and the shmized strings */
static void cfg_destory_groups(unsigned char *block)
{
	cfg_group_t	*group, *group2;
	cfg_mapping_t	*mapping;
	cfg_def_t	*def;
	void		*old_string;
	int		i;

	group = cfg_group;
	while(group) {
		mapping = group->mapping;
		def = mapping ? mapping->def : NULL;

		/* destory the shmized strings in the block */
		if (block && def)
			for (i=0; i<group->num; i++)
				if (((CFG_VAR_TYPE(&mapping[i]) == CFG_VAR_STRING) ||
				(CFG_VAR_TYPE(&mapping[i]) == CFG_VAR_STR)) &&
					mapping[i].flag & cfg_var_shmized) {

						old_string = *(char **)(block + group->var_offset + mapping[i].offset);
						if (old_string) shm_free(old_string);
				}

		if (group->dynamic == CFG_GROUP_DYNAMIC) {
			/* the group was dynamically allocated */
			cfg_script_destroy(group);
		} else {
			/* only the mapping was allocated, all the other
			pointers are just set to static variables */
			if (mapping) pkg_free(mapping);
		}
		/* Delete the additional variable list */
		del_add_var_list(group);

		group2 = group->next;
		pkg_free(group);
		group = group2;
	}
}

/* initiate the cfg framework */
int sr_cfg_init(void)
{
	cfg_global_lock = lock_alloc();
	if (!cfg_global_lock) {
		LOG(L_ERR, "ERROR: sr_cfg_init(): not enough shm memory\n");
		goto error;
	}
	if (lock_init(cfg_global_lock) == 0) {
		LOG(L_ERR, "ERROR: sr_cfg_init(): failed to init lock\n");
		lock_dealloc(cfg_global_lock);
		cfg_global_lock = 0;
		goto error;
	}

	cfg_writer_lock = lock_alloc();
	if (!cfg_writer_lock) {
		LOG(L_ERR, "ERROR: sr_cfg_init(): not enough shm memory\n");
		goto error;
	}
	if (lock_init(cfg_writer_lock) == 0) {
		LOG(L_ERR, "ERROR: sr_cfg_init(): failed to init lock\n");
		lock_dealloc(cfg_writer_lock);
		cfg_writer_lock = 0;
		goto error;
	}

	cfg_global = (cfg_block_t **)shm_malloc(sizeof(cfg_block_t *));
	if (!cfg_global) {
		LOG(L_ERR, "ERROR: sr_cfg_init(): not enough shm memory\n");
		goto error;
	}
	*cfg_global = NULL;

	cfg_child_cb_first = (cfg_child_cb_t **)shm_malloc(sizeof(cfg_child_cb_t *));
	if (!cfg_child_cb_first) {
		LOG(L_ERR, "ERROR: sr_cfg_init(): not enough shm memory\n");
		goto error;
	}
	*cfg_child_cb_first = NULL;

	cfg_child_cb_last = (cfg_child_cb_t **)shm_malloc(sizeof(cfg_child_cb_t *));
	if (!cfg_child_cb_last) {
		LOG(L_ERR, "ERROR: sr_cfg_init(): not enough shm memory\n");
		goto error;
	}
	*cfg_child_cb_last = NULL;

	/* A new cfg_child_cb struct must be created with a NULL callback function.
	This stucture will be the entry point for the child processes, and
	will be freed later, when none of the processes refers to it */
	*cfg_child_cb_first = *cfg_child_cb_last =
		cfg_child_cb_new(NULL, NULL, NULL, 0);

	if (!*cfg_child_cb_first) goto error;

	return 0;

error:
	cfg_destroy();

	return -1;
}

/* destroy the memory allocated for the cfg framework */
void cfg_destroy(void)
{
	/* free the contexts */
	cfg_ctx_destroy();

	/* free the list of groups */
	cfg_destory_groups((cfg_global && (*cfg_global)) ? (*cfg_global)->vars : NULL);

	/* free the select list */
	cfg_free_selects();

	if (cfg_child_cb_first) {
		if (*cfg_child_cb_first) cfg_child_cb_free_list(*cfg_child_cb_first);
		shm_free(cfg_child_cb_first);
		cfg_child_cb_first = NULL;
	}

	if (cfg_child_cb_last) {
		shm_free(cfg_child_cb_last);
		cfg_child_cb_last = NULL;
	}

	if (cfg_global) {
		if (*cfg_global) cfg_block_free(*cfg_global);
		shm_free(cfg_global);
		cfg_global = NULL;
	}
	if (cfg_global_lock) {
		lock_destroy(cfg_global_lock);
		lock_dealloc(cfg_global_lock);
		cfg_global_lock = 0;
	}
	if (cfg_writer_lock) {
		lock_destroy(cfg_writer_lock);
		lock_dealloc(cfg_writer_lock);
		cfg_writer_lock = 0;
	}
}

/* Register num number of child processes that will
 * keep updating their local configuration.
 * This function needs to be called from mod_init
 * before any child process is forked.
 */
void cfg_register_child(int num)
{
	/* Increase the reference counter of the first list item
	 * with the number of child processes.
	 * If the counter was increased after forking then it
	 * could happen that a child process is forked and updates
	 * its local config very fast before the other processes have
	 * a chance to refer to the list item. The result is that the
	 * item is freed by the "fast" child process and the other
	 * processes do not see the beginning of the list and miss
	 * some config changes.
	 */
	atomic_add(&((*cfg_child_cb_first)->refcnt), num);
}

/* per-child process init function.
 * It needs to be called from the forked process.
 * cfg_register_child() must be called before this function!
 */
int cfg_child_init(void)
{
	/* set the callback list pointer to the beginning of the list */
	cfg_child_cb = *cfg_child_cb_first;

	return 0;
}

/* Child process init function that can be called
 * without cfg_register_child().
 * Note that the child process may miss some configuration changes.
 */
int cfg_late_child_init(void)
{
	/* set the callback list pointer to the beginning of the list */
	CFG_LOCK();
	atomic_inc(&((*cfg_child_cb_first)->refcnt));
	cfg_child_cb = *cfg_child_cb_first;
	CFG_UNLOCK();

	return 0;
}


/* per-child init function for non-cb executing processes.
 * Mark this process as not wanting to execute any per-child config
 * callback (it will have only limited config functionality, but is useful
 * when a process needs only to watch some non-callback cfg. values,
 * e.g. the main attendant process, debug and memlog).
 * It needs to be called from the forked process.
 * cfg_register_child must _not_ be called.
 */
int cfg_child_no_cb_init(void)
{
	/* set the callback list pointer to the beginning of the list */
	cfg_child_cb = CFG_NO_CHILD_CBS;
	return 0;
}

/* per-child process destroy function
 * Should be called only when the child process exits,
 * but SER continues running
 *
 * WARNING: this function call must be the very last action
 * before the child process exits, because the local config
 * is not available afterwards.
 */
void cfg_child_destroy(void)
{
	cfg_child_cb_t	*prev_cb;

	/* unref the local config */
	if (cfg_local) {
		CFG_UNREF(cfg_local);
		cfg_local = NULL;
	}

	if (!cfg_child_cb || cfg_child_cb==CFG_NO_CHILD_CBS) return;

	/* The lock must be held to make sure that the global config
	is not replaced meantime, and the other child processes do not
	leave the old value of *cfg_child_cb_last. Otherwise it could happen,
	that all the other processes move their own cfg_child_cb pointer before
	this process reaches *cfg_child_cb_last, though, it is very unlikely. */
	CFG_LOCK();

	/* go through the list and check whether there is any item that
	has to be freed (similar to cfg_update_local(), but without executing
	the callback functions) */
	while (cfg_child_cb != *cfg_child_cb_last) {
		prev_cb = cfg_child_cb;
		cfg_child_cb = cfg_child_cb->next;
		atomic_inc(&cfg_child_cb->refcnt);
		if (atomic_dec_and_test(&prev_cb->refcnt)) {
			/* No more pocess refers to this callback.
			Did this process block the deletion,
			or is there any other process that has not
			reached	prev_cb yet? */
			if (*cfg_child_cb_first == prev_cb) {
				/* yes, this process was blocking the deletion */
				*cfg_child_cb_first = cfg_child_cb;
				cfg_child_cb_free_item(prev_cb);
			}
		} else {
			/* no need to continue, because there is at least
			one process that stays exactly at the same point
			in the list, so it will free the items later */
			break;
		}
	}
	atomic_dec(&cfg_child_cb->refcnt);

	CFG_UNLOCK();
	cfg_child_cb = NULL;
}

/* searches a group by name */
cfg_group_t *cfg_lookup_group(char *name, int len)
{
	cfg_group_t	*g;

	for (	g = cfg_group;
		g;
		g = g->next
	)
		if ((g->name_len == len)
		&& (memcmp(g->name, name, len)==0))
			return g;

	return NULL;
}

/* searches a variable definition by group and variable name */
int cfg_lookup_var(str *gname, str *vname,
			cfg_group_t **group, cfg_mapping_t **var)
{
	cfg_group_t	*g;
	int		i;

	for (	g = cfg_group;
		g;
		g = g->next
	)
		if ((g->name_len == gname->len)
		&& (memcmp(g->name, gname->s, gname->len)==0)) {

			if (!g->mapping) return -1; /* dynamic group is not ready */

			for (	i = 0;
				i < g->num;
				i++
			) {
				if ((g->mapping[i].name_len == vname->len)
				&& (memcmp(g->mapping[i].def->name, vname->s, vname->len)==0)) {
					if (group) *group = g;
					if (var) *var = &(g->mapping[i]);
					return 0;
				}
			}
			break;
		}

	LOG(L_DBG, "DEBUG: cfg_lookup_var(): variable not found: %.*s.%.*s\n",
			gname->len, gname->s,
			vname->len, vname->s);
	return -1;
}

/* searches a variable definition within a group by its name */
cfg_mapping_t *cfg_lookup_var2(cfg_group_t *group, char *name, int len)
{
	int	i;

	if (!group->mapping) return NULL; /* dynamic group is not ready */

	for (	i = 0;
		i < group->num;
		i++
	) {
		if ((group->mapping[i].name_len == len)
		&& (memcmp(group->mapping[i].def->name, name, len)==0)) {
			return &(group->mapping[i]);
		}
	}

	LOG(L_DBG, "DEBUG: cfg_lookup_var2(): variable not found: %.*s.%.*s\n",
			group->name_len, group->name,
			len, name);
	return NULL;
}

/* clones the global config block
 * WARNING: unsafe, cfg_writer_lock or cfg_global_lock must be held!
 */
cfg_block_t *cfg_clone_global(void)
{
	cfg_block_t	*block;

	block = (cfg_block_t*)shm_malloc(sizeof(cfg_block_t)+cfg_block_size-1);
	if (!block) {
		LOG(L_ERR, "ERROR: cfg_clone_global(): not enough shm memory\n");
		return NULL;
	}
	memcpy(block, *cfg_global, sizeof(cfg_block_t)+cfg_block_size-1);

	/* reset the reference counter */
	atomic_set(&block->refcnt, 0);

	return block;
}

/* Clone an array of configuration group instances. */
cfg_group_inst_t *cfg_clone_array(cfg_group_meta_t *meta, cfg_group_t *group)
{
	cfg_group_inst_t	*new_array;
	int			size;

	if (!meta->array || !meta->num)
		return NULL;

	size = (sizeof(cfg_group_inst_t) + group->size - 1) * meta->num;
	new_array = (cfg_group_inst_t *)shm_malloc(size);
	if (!new_array) {
		LOG(L_ERR, "ERROR: cfg_clone_array(): not enough shm memory\n");
		return NULL;
	}
	memcpy(new_array, meta->array, size);

	return new_array;
}

/* Extend the array of configuration group instances with one more instance.
 * Only the ID of the new group is set, nothing else. */
cfg_group_inst_t *cfg_extend_array(cfg_group_meta_t *meta, cfg_group_t *group,
				unsigned int group_id,
				cfg_group_inst_t **new_group)
{
	int			i;
	cfg_group_inst_t	*new_array, *old_array;
	int			inst_size;

	inst_size = sizeof(cfg_group_inst_t) + group->size - 1;
	new_array = (cfg_group_inst_t *)shm_malloc(inst_size * (meta->num + 1));
	if (!new_array) {
		LOG(L_ERR, "ERROR: cfg_extend_array(): not enough shm memory\n");
		return NULL;
	}
	/* Find the position of the new group in the array. The array is ordered
	by the group IDs. */
	old_array = meta->array;
	for (	i = 0;
		(i < meta->num)
			&& (((cfg_group_inst_t *)((char *)old_array + inst_size * i))->id < group_id);
		i++
	);
	if (i > 0)
		memcpy(	new_array,
			old_array,
			inst_size * i);

	memset((char*)new_array + inst_size * i, 0, inst_size);
	*new_group = (cfg_group_inst_t *)((char*)new_array + inst_size * i);
	(*new_group)->id = group_id;

	if (i < meta->num)
		memcpy(	(char*)new_array + inst_size * (i + 1),
			(char*)old_array + inst_size * i,
			inst_size * (meta->num - i));

	return new_array;
}

/* Remove an instance from a group array.
 * inst must point to an instance within meta->array.
 * *_new_array is set to the newly allocated array. */
int cfg_collapse_array(cfg_group_meta_t *meta, cfg_group_t *group,
				cfg_group_inst_t *inst,
				cfg_group_inst_t **_new_array)
{
	cfg_group_inst_t	*new_array, *old_array;
	int			inst_size, offset;

	if (!meta->num)
		return -1;

	if (meta->num == 1) {
		*_new_array = NULL;
		return 0;
	}

	inst_size = sizeof(cfg_group_inst_t) + group->size - 1;
	new_array = (cfg_group_inst_t *)shm_malloc(inst_size * (meta->num - 1));
	if (!new_array) {
		LOG(L_ERR, "ERROR: cfg_collapse_array(): not enough shm memory\n");
		return -1;
	}

	old_array = meta->array;
	offset = (char *)inst - (char *)old_array;
	if (offset)
		memcpy(	new_array,
			old_array,
			offset);

	if (meta->num * inst_size > offset + inst_size)
		memcpy( (char *)new_array + offset,
			(char *)old_array + offset + inst_size,
			(meta->num - 1) * inst_size - offset);

	*_new_array = new_array;
	return 0;
}

/* Find the group instance within the meta-data based on the group_id */
cfg_group_inst_t *cfg_find_group(cfg_group_meta_t *meta, int group_size, unsigned int group_id)
{
	int	i;
	cfg_group_inst_t *ginst;

	if (!meta)
		return NULL;

	/* For now, search lineray.
	TODO: improve */
	for (i = 0; i < meta->num; i++) {
		ginst = (cfg_group_inst_t *)((char *)meta->array
			+ (sizeof(cfg_group_inst_t) + group_size - 1) * i);
		if (ginst->id == group_id)
			return ginst;
		else if (ginst->id > group_id)
			break; /* needless to continue, the array is ordered */
	}
	return NULL;
}

/* append new callbacks to the end of the child callback list
 *
 * WARNING: the function is unsafe, either hold CFG_LOCK(),
 * or call the function before forking
 */
void cfg_install_child_cb(cfg_child_cb_t *cb_first, cfg_child_cb_t *cb_last)
{
	/* add the new callbacks to the end of the linked-list */
	(*cfg_child_cb_last)->next = cb_first;
	*cfg_child_cb_last = cb_last;
}

/* installs a new global config
 *
 * replaced is an array of strings that must be freed together
 * with the previous global config.
 * cb_first and cb_last define a linked list of per-child process
 * callbacks. This list is added to the global linked list.
 */
void cfg_install_global(cfg_block_t *block, void **replaced,
			cfg_child_cb_t *cb_first, cfg_child_cb_t *cb_last)
{
	cfg_block_t* old_cfg;
	
	CFG_REF(block);

	if (replaced) {
		/* The replaced array is specified, it has to be linked to the child cb structure.
		 * The last child process processing this structure will free the old strings and the array. */
		if (cb_first) {
			cb_first->replaced = replaced;
		} else {
			/* At least one child cb structure is needed. */
			cb_first = cfg_child_cb_new(NULL, NULL, NULL, 0 /* gname, name, cb, type */);
			if (cb_first) {
				cb_last = cb_first;
				cb_first->replaced = replaced;
			} else {
				LOG(L_ERR, "ERROR: cfg_install_global(): not enough shm memory\n");
				/* Nothing more can be done here, the replaced strings are still needed,
				 * they cannot be freed at this moment.
				 */
			}
		}
	}

	CFG_LOCK();

	old_cfg = *cfg_global;
	*cfg_global = block;

	if (cb_first)
		cfg_install_child_cb(cb_first, cb_last);

	CFG_UNLOCK();
	
	if (old_cfg)
		CFG_UNREF(old_cfg);
}

/* creates a structure for a per-child process callback */
cfg_child_cb_t *cfg_child_cb_new(str *gname, str *name,
			cfg_on_set_child cb,
			unsigned int type)
{
	cfg_child_cb_t	*cb_struct;

	cb_struct = (cfg_child_cb_t *)shm_malloc(sizeof(cfg_child_cb_t));
	if (!cb_struct) {
		LOG(L_ERR, "ERROR: cfg_child_cb_new(): not enough shm memory\n");
		return NULL;
	}
	memset(cb_struct, 0, sizeof(cfg_child_cb_t));
	if (gname) {
		cb_struct->gname.s = gname->s;
		cb_struct->gname.len = gname->len;
	}
	if (name) {
		cb_struct->name.s = name->s;
		cb_struct->name.len = name->len;
	}
	cb_struct->cb = cb;
	atomic_set(&cb_struct->refcnt, 0);

	if (type & CFG_CB_ONLY_ONCE) {
		/* The callback needs to be executed only once.
		 * Set the cb_count value to 1, so the first child
		 * process that executes the callback will decrement
		 * it to 0, and no other children will execute the
		 * callback again.
		 */
		atomic_set(&cb_struct->cb_count, 1);
	} else {
		/* Set the cb_count to a high value, i.e. max signed integer,
		 * so all the child processes will execute the callback,
		 * the counter will never reach 0.
		 */
		atomic_set(&cb_struct->cb_count, (1U<<(sizeof(int)*8-1))-1);
	}

	return cb_struct;
}

/* free the memory allocated for a child cb list */
void cfg_child_cb_free_list(cfg_child_cb_t *child_cb_first)
{
	cfg_child_cb_t	*cb, *cb_next;

	for(	cb = child_cb_first;
		cb;
		cb = cb_next
	) {
		cb_next = cb->next;
		cfg_child_cb_free_item(cb);
	}
}

/* Allocate memory for a new additional variable
 * and link it to a configuration group.
 * type==0 results in creating a new group instance with the default values.
 * The group is created with CFG_GROUP_UNKNOWN type if it does not exist.
 * Note: this function is usable only before the configuration is shmized.
 */
int new_add_var(str *group_name, unsigned int group_id, str *var_name,
				void *val, unsigned int type)
{
	cfg_group_t	*group;
	cfg_add_var_t	*add_var = NULL, **add_var_p;
	int		len;

	if (type && !var_name) {
		LOG(L_ERR, "ERROR: new_add_var(): Missing variable specification\n");
		goto error;
	}
	if (type)
		LOG(L_DBG, "DEBUG: new_add_var(): declaring a new variable instance %.*s[%u].%.*s\n",
			group_name->len, group_name->s,
			group_id,
			var_name->len, var_name->s);
	else
		LOG(L_DBG, "DEBUG: new_add_var(): declaring a new group instance %.*s[%u]\n",
			group_name->len, group_name->s,
			group_id);

	if (cfg_shmized) {
		LOG(L_ERR, "ERROR: new_add_var(): too late, the configuration has already been shmized\n");
		goto error;
	}

	group = cfg_lookup_group(group_name->s, group_name->len);
	if (!group) {
		/* create a new group with NULL values, it will be filled in later */
		group = cfg_new_group(group_name->s, group_name->len,
					0 /* num */, NULL /* mapping */,
					NULL /* vars */, 0 /* size */, NULL /* handle */);

		if (!group)
			goto error;
		/* It is not yet known whether the group will be static or dynamic */
		group->dynamic = CFG_GROUP_UNKNOWN;
	}

	add_var = (cfg_add_var_t *)pkg_malloc(sizeof(cfg_add_var_t) +
						(type ? (var_name->len - 1) : 0));
	if (!add_var) {
		LOG(L_ERR, "ERROR: new_add_var(): Not enough memory\n");
		goto error;
	}
	memset(add_var, 0, sizeof(cfg_add_var_t) +
				(type ? (var_name->len - 1) : 0));

	add_var->group_id = group_id;
	if (type) {
		add_var->name_len = var_name->len;
		memcpy(add_var->name, var_name->s, var_name->len);

		switch (type) {
		case CFG_VAR_INT:
			add_var->val.i = (int)(long)val;
			break;

		case CFG_VAR_STR:
			len = ((str *)val)->len;
			if (len) {
				add_var->val.s.s = (char *)pkg_malloc(sizeof(char) * len);
				if (!add_var->val.s.s) {
					LOG(L_ERR, "ERROR: new_add_var(): Not enough memory\n");
					goto error;
				}
				memcpy(add_var->val.s.s, ((str *)val)->s, len);
			} else {
				add_var->val.s.s = NULL;
			}
			add_var->val.s.len = len;
			break;

		case CFG_VAR_STRING:
			if (val) {
				len = strlen((char *)val);
				add_var->val.ch = (char *)pkg_malloc(sizeof(char) * (len + 1));
				if (!add_var->val.ch) {
					LOG(L_ERR, "ERROR: new_add_var(): Not enough memory\n");
					goto error;
				}
				memcpy(add_var->val.ch, (char *)val, len);
				add_var->val.ch[len] = '\0';
			} else {
				add_var->val.ch = NULL;
			}
			break;

		default:
			LOG(L_ERR, "ERROR: new_add_var(): unsupported value type: %u\n",
				type);
			goto error;
		}
		add_var->type = type;
	}

	/* order the list by group_id, it will be easier to count the group instances */
	for(	add_var_p = &group->add_var;
		*add_var_p && ((*add_var_p)->group_id <= group_id);
		add_var_p = &((*add_var_p)->next));

	add_var->next = *add_var_p;
	*add_var_p = add_var;

	return 0;

error:
	if (!type)
		LOG(L_ERR, "ERROR: new_add_var(): failed to add the additional group instance: %.*s[%u]\n",
			group_name->len, group_name->s, group_id);
	else
		LOG(L_ERR, "ERROR: new_add_var(): failed to add the additional variable instance: %.*s[%u].%.*s\n",
			group_name->len, group_name->s, group_id,
			(var_name)?var_name->len:0, (var_name&&var_name->s)?var_name->s:"");

	if (add_var)
		pkg_free(add_var);
	return -1;
}

/* delete the additional variable list */
static void del_add_var_list(cfg_group_t *group)
{
	cfg_add_var_t	*add_var, *add_var2;

	add_var = group->add_var;
	while (add_var) {
		add_var2 = add_var->next;
		if ((add_var->type == CFG_VAR_STR) && add_var->val.s.s)
			pkg_free(add_var->val.s.s);
		else if ((add_var->type == CFG_VAR_STRING) && add_var->val.ch)
			pkg_free(add_var->val.ch);
		pkg_free(add_var);
		add_var = add_var2;
	}
	group->add_var = NULL;
}

/* create the array of additional group instances from the linked list */
static int apply_add_var_list(cfg_block_t *block, cfg_group_t *group)
{
	int		i, num, size;
	unsigned int	group_id;
	cfg_add_var_t	*add_var;
	cfg_group_inst_t	*new_array, *ginst;
	cfg_group_meta_t *gm;

	/* count the number of group instances */
	for (	add_var = group->add_var, num = 0, group_id = 0;
		add_var;
		add_var = add_var->next
	) {
		if (!num || (group_id != add_var->group_id)) {
			num++;
			group_id = add_var->group_id;
		}
	}

	if (!num)	/* nothing to do */
		return 0;

	LOG(L_DBG, "DEBUG: apply_add_var_list(): creating the group instance array "
		"for '%.*s' with %d slots\n",
		group->name_len, group->name, num);
	size = (sizeof(cfg_group_inst_t) + group->size - 1) * num;
	new_array = (cfg_group_inst_t *)shm_malloc(size);
	if (!new_array) {
		LOG(L_ERR, "ERROR: apply_add_var_list(): not enough shm memory\n");
		return -1;
	}
	memset(new_array, 0, size);

	for (i = 0; i < num; i++) {
		/* Go though each group instance, set the default values,
		and apply the changes */

		if (!group->add_var) {
			LOG(L_ERR, "BUG: apply_add_var_list(): no more additional variable left\n");
			goto error;
		}
		ginst = (cfg_group_inst_t *)((char*)new_array + (sizeof(cfg_group_inst_t) + group->size - 1) * i);
		ginst->id = group->add_var->group_id;
		/* fill in the new group instance with the default data */
		memcpy(	ginst->vars,
			CFG_GROUP_DATA(block, group),
			group->size);
		/* cfg_apply_list() moves the group->add_var pointer to
		the beginning of the new group instance. */
		if (cfg_apply_list(ginst, group, ginst->id, &group->add_var))
			goto error;
	}

#ifdef EXTRA_DEBUG
	if (group->add_var) {
		LOG(L_ERR, "BUG: apply_add_var_list(): not all the additional variables have been consumed\n");
		goto error;
	}
#endif

	gm = CFG_GROUP_META(block, group);
	gm->num = num;
	gm->array = new_array;
	return 0;

error:
	LOG(L_ERR, "ERROR: apply_add_var_list(): Failed to apply the additional variable list\n");
	shm_free(new_array);
	return -1;
}

/* Move the group handle to the specified group instance pointed by dst_ginst.
 * src_ginst shall point to the active group instance.
 * Both parameters can be NULL meaning that the src/dst config is the default, 
 * not an additional group instance.
 * The function executes all the per-child process callbacks which are different
 * in the two instaces.
 */
void cfg_move_handle(cfg_group_t *group, cfg_group_inst_t *src_ginst, cfg_group_inst_t *dst_ginst)
{
	cfg_mapping_t		*var;
	unsigned int		bitmap;
	int			i, pos;
	str			gname, vname;

	if (src_ginst == dst_ginst)
		return;	/* nothing to do */

	/* move the handle to the variables of the dst group instance,
	or to the local config if no dst group instance is specified */
	*(group->handle) = dst_ginst ?
				dst_ginst->vars
				: CFG_GROUP_DATA(cfg_local, group);

	if (cfg_child_cb != CFG_NO_CHILD_CBS) {
		/* call the per child process callback of those variables
		that have different value in the two group instances */
		/* TODO: performance optimization: this entire loop can be
		skipped if the group does not have any variable with
		per-child process callback. Use some flag in the group
		structure for this purpose. */
		gname.s = group->name;
		gname.len = group->name_len;
		for (i = 0; i < CFG_MAX_VAR_NUM/(sizeof(int)*8); i++) {
			bitmap = ((src_ginst) ? src_ginst->set[i] : 0U)
				| ((dst_ginst) ? dst_ginst->set[i] : 0U);
			while (bitmap) {
				pos = bit_scan_forward32(bitmap);
				var = &group->mapping[pos + i*sizeof(int)*8];
				if (var->def->on_set_child_cb) {
					vname.s = var->def->name;
					vname.len = var->name_len;
					var->def->on_set_child_cb(&gname, &vname);
				}
				bitmap -= (1U << pos);
			}
		}
	}
	/* keep track of how many group instences are set in the child process */
	if (!src_ginst && dst_ginst)
		cfg_ginst_count++;
	else if (!dst_ginst)
		cfg_ginst_count--;
#ifdef EXTRA_DEBUG
	if (cfg_ginst_count < 0)
		LOG(L_ERR, "ERROR: cfg_select(): BUG, cfg_ginst_count is negative: %d. group=%.*s\n",
			cfg_ginst_count, group->name_len, group->name);
#endif
	return;
}

/* Move the group handle to the specified group instance. */
int cfg_select(cfg_group_t *group, unsigned int id)
{
	cfg_group_inst_t	*ginst;

	if (!cfg_local) {
		LOG(L_ERR, "ERROR: The child process has no local configuration\n");
		return -1;
	}

	if (!(ginst = cfg_find_group(CFG_GROUP_META(cfg_local, group),
				group->size,
				id))
	) {
		LOG(L_ERR, "ERROR: cfg_select(): group instance '%.*s[%u]' does not exist\n",
				group->name_len, group->name, id);
		return -1;
	}

	cfg_move_handle(group,
			CFG_HANDLE_TO_GINST(*(group->handle)), /* the active group instance */
			ginst);

	LOG(L_DBG, "DEBUG: cfg_select(): group instance '%.*s[%u]' has been selected\n",
			group->name_len, group->name, id);
	return 0;
}

/* Reset the group handle to the default, local configuration */
int cfg_reset(cfg_group_t *group)
{
	if (!cfg_local) {
		LOG(L_ERR, "ERROR: The child process has no local configuration\n");
		return -1;
	}

	cfg_move_handle(group,
			CFG_HANDLE_TO_GINST(*(group->handle)), /* the active group instance */
			NULL);

	LOG(L_DBG, "DEBUG: cfg_reset(): default group '%.*s' has been selected\n",
			group->name_len, group->name);
	return 0;
}

/* Move the group handle to the first group instance.
 * This function together with cfg_select_next() can be used
 * to iterate though the list of instances.
 *
 * Return value:
 *	-1: no group instance found
 *	 0: first group instance is successfully selected.
 */
int cfg_select_first(cfg_group_t *group)
{
	cfg_group_meta_t	*meta;
	cfg_group_inst_t	*ginst;

	if (!cfg_local) {
		LOG(L_ERR, "ERROR: The child process has no local configuration\n");
		return -1;
	}

	meta = CFG_GROUP_META(cfg_local, group);
	if (!meta || (meta->num == 0))
		return -1;

	ginst = (cfg_group_inst_t *)meta->array;
	cfg_move_handle(group,
			CFG_HANDLE_TO_GINST(*(group->handle)), /* the active group instance */
			ginst);

	LOG(L_DBG, "DEBUG: cfg_select_first(): group instance '%.*s[%u]' has been selected\n",
			group->name_len, group->name, ginst->id);
	return 0;
}

/* Move the group handle to the next group instance.
 * This function together with cfg_select_first() can be used
 * to iterate though the list of instances.
 *
 * Return value:
 *	-1: no more group instance found. Note, that the active group
 *		instance is not changed in this case.
 *	 0: the next group instance is successfully selected.
 */
int cfg_select_next(cfg_group_t *group)
{
	cfg_group_meta_t	*meta;
	cfg_group_inst_t	*old_ginst, *new_ginst;
	int	size;

	if (!cfg_local) {
		LOG(L_ERR, "ERROR: The child process has no local configuration\n");
		return -1;
	}

	if (!(meta = CFG_GROUP_META(cfg_local, group)))
		return -1;

	if (!(old_ginst = CFG_HANDLE_TO_GINST(*(group->handle)) /* the active group instance */)) {
		LOG(L_ERR, "ERROR: cfg_select_next(): No group instance is set currently. Forgot to call cfg_select_first()?\n");
		return -1;
	}

	size = sizeof(cfg_group_inst_t) + group->size - 1;
	if (((char *)old_ginst - (char *)meta->array)/size + 1 >= meta->num)
		return -1; /* this is the last group instance */

	new_ginst = (cfg_group_inst_t *)((char *)old_ginst + size);
	cfg_move_handle(group,
			old_ginst, /* the active group instance */
			new_ginst);

	LOG(L_DBG, "DEBUG: cfg_select_next(): group instance '%.*s[%u]' has been selected\n",
			group->name_len, group->name, new_ginst->id);
	return 0;
}

/* Temporary set the local configuration in the main process before forking.
 * This makes the group instances usable in the main process after
 * the configuration is shmized, but before the children are forked.
 */
void cfg_main_set_local(void)
{
	/* Disable the execution of child-process callbacks,
	 * they can cause trouble because the children inherit all the
	 * values later */
	cfg_child_cb = CFG_NO_CHILD_CBS;
	cfg_update_no_cbs();
}

/* Reset the local configuration of the main process back to its original state
 * to make sure that the forked processes are not affected.
 */
void cfg_main_reset_local(void)
{
	cfg_group_t	*group;

	/* Unref the local config, and set it back to NULL.
	 * Each child will set its own local configuration. */
	if (cfg_local) {
		CFG_UNREF(cfg_local);
		cfg_local = NULL;

		/* restore the original value of the module handles */
		for (	group = cfg_group;
			group;
			group = group->next
		)
			*(group->handle) = group->orig_handle;
		/* The handle might have pointed to a group instance,
		 * reset the instance counter. */
		cfg_ginst_count = 0;
	}
	cfg_child_cb = NULL;
}


