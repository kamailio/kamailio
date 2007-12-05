/*
 * $Id$
 *
 * Copyright (C) 2007 iptelorg GmbH
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
 * History
 * -------
 *  2007-12-03	Initial version (Miklos)
 */

#include <string.h>

#include "../mem/mem.h"
#include "../mem/shm_mem.h"
#include "../ut.h"
#include "../locking.h"
#include "cfg_ctx.h"
#include "cfg_struct.h"

cfg_group_t	*cfg_group = NULL;	/* linked list of registered cfg groups */
cfg_block_t	**cfg_global = NULL;	/* pointer to the active cfg block */
cfg_block_t	*cfg_local = NULL;	/* per-process pointer to the active cfg block.
					Updated only when the child process
					finishes working on the SIP message */
static int	cfg_block_size = 0;	/* size of the cfg block (constant) */
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

/* creates a new cfg group, and adds it to the linked list */
int cfg_new_group(char *name, int num, cfg_mapping_t *mapping,
		char *vars, int size, void **handle)
{
	cfg_group_t	*group;
	int		len;

	if (cfg_shmized) {
		LOG(L_ERR, "ERROR: cfg_new_group(): too late config declaration\n");
		return -1;
	}

	len = strlen(name);
	group = (cfg_group_t *)pkg_malloc(sizeof(cfg_group_t)+len-1);
	if (!group) {
		LOG(L_ERR, "ERROR: cfg_new_group(): not enough memory\n");
		return -1;
	}
	memset(group, 0, sizeof(cfg_group_t)+len-1);

	group->num = num;
	group->mapping = mapping;
	group->vars = vars;
	group->size = size;
	group->handle = handle;
	group->name_len = len;
	memcpy(&group->name, name, len);

	/* add the new group to the beginning of the list */
	group->next = cfg_group;
	cfg_group = group;

	return 0;
}

/* clones a string to shared memory */
char *cfg_clone_str(str s)
{
	char	*c;

	c = (char *)shm_malloc(sizeof(char)*(s.len+1));
	if (!c) {
		LOG(L_ERR, "ERROR: cfg_clone_str(): not enough shm memory\n");
		return NULL;
	}
	memcpy(c, s.s, s.len);
	c[s.len] = '\0';

	return c;
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
			memcpy(&s.s, group->vars + mapping[i].offset, sizeof(char *));
			s.len = strlen(s.s);

		} else if (CFG_VAR_TYPE(&mapping[i]) == CFG_VAR_STR) {
			memcpy(&s, group->vars + mapping[i].offset, sizeof(str));

		} else {
			continue;
		}
		if (!(s.s = cfg_clone_str(s))) return -1;
		memcpy(group->vars + mapping[i].offset, &s.s, sizeof(char *));
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
	will contain all the variables */
	for (	size=0, group = cfg_group;
		group;
		group=group->next
	) {
		size = ROUND_POINTER(size);
		group->offset = size;
		size += group->size;
	}

	block = (cfg_block_t*)shm_malloc(sizeof(cfg_block_t)+size-1);
	if (!block) {
		LOG(L_ERR, "ERROR: cfg_clone_str(): not enough shm memory\n");
		goto error;
	}
	memset(block, 0, sizeof(cfg_block_t)+size-1);
	cfg_block_size = size;

	/* copy the memory fragments to the single block */
	for (	group = cfg_group;
		group;
		group=group->next
	) {
		/* clone the strings to shm mem */
		if (cfg_shmize_strings(group)) goto error;

		/* copy the values to the new block,
		and update the module's handle */
		memcpy(block->vars+group->offset, group->vars, group->size);
		*(group->handle) = block->vars+group->offset;
	}

	/* install the new config */
	cfg_install_global(block, NULL, NULL, NULL);
	cfg_shmized = 1;

	return 0;

error:
	if (block) shm_free(block);
	return -1;
}

/* initiate the cfg framework */
int cfg_init(void)
{
	cfg_global_lock = lock_alloc();
	if (!cfg_global_lock) {
		LOG(L_ERR, "ERROR: cfg_init(): not enough shm memory\n");
		goto error;
	}
	if (lock_init(cfg_global_lock) == 0) {
		LOG(L_ERR, "ERROR: cfg_init(): failed to init lock\n");
		lock_dealloc(cfg_global_lock);
		cfg_global_lock = 0;
		goto error;
	}

	cfg_writer_lock = lock_alloc();
	if (!cfg_writer_lock) {
		LOG(L_ERR, "ERROR: cfg_init(): not enough shm memory\n");
		goto error;
	}
	if (lock_init(cfg_writer_lock) == 0) {
		LOG(L_ERR, "ERROR: cfg_init(): failed to init lock\n");
		lock_dealloc(cfg_writer_lock);
		cfg_writer_lock = 0;
		goto error;
	}

	cfg_global = (cfg_block_t **)shm_malloc(sizeof(cfg_block_t *));
	if (!cfg_global) {
		LOG(L_ERR, "ERROR: cfg_init(): not enough shm memory\n");
		goto error;
	}
	*cfg_global = NULL;

	cfg_child_cb_first = (cfg_child_cb_t **)shm_malloc(sizeof(cfg_child_cb_t *));
	if (!cfg_child_cb_first) {
		LOG(L_ERR, "ERROR: cfg_init(): not enough shm memory\n");
		goto error;
	}
	*cfg_child_cb_first = NULL;

	cfg_child_cb_last = (cfg_child_cb_t **)shm_malloc(sizeof(cfg_child_cb_t *));
	if (!cfg_child_cb_last) {
		LOG(L_ERR, "ERROR: cfg_init(): not enough shm memory\n");
		goto error;
	}
	*cfg_child_cb_last = NULL;

	/* A new cfg_child_cb struct must be created with a NULL callback function.
	This stucture will be the entry point for the child processes, and
	will be freed later, when none of the processes refers to it */
	*cfg_child_cb_first = *cfg_child_cb_last =
		cfg_child_cb_new(NULL, NULL);

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

	if (cfg_child_cb_first) {
		if (*cfg_child_cb_first) cfg_child_cb_free(*cfg_child_cb_first);
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

/* per-child process init function */
int cfg_child_init(void)
{
	/* set the callback list pointer to the beginning of the list */
	cfg_child_cb = *cfg_child_cb_first;
	atomic_inc(&cfg_child_cb->refcnt);

	return 0;
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

			for (	i = 0;
				i < g->size;
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

	LOG(L_ERR, "ERROR: cfg_lookup_var(): variable not found: %.*s.%.*s\n",
			gname->len, gname->s,
			vname->len, vname->s);
	return -1;
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

/* installs a new global config
 *
 * replaced is an array of strings that must be freed together
 * with the previous global config.
 * cb_first and cb_last define a linked list of per-child process
 * callbacks. This list is added to the global linked list.
 */
void cfg_install_global(cfg_block_t *block, char **replaced,
			cfg_child_cb_t *cb_first, cfg_child_cb_t *cb_last)
{
	CFG_LOCK();

	if (*cfg_global) {
		if (replaced) (*cfg_global)->replaced = replaced;
		CFG_UNREF(*cfg_global);
	}
	CFG_REF(block);
	*cfg_global = block;

	if (cb_first) {
		/* add the new callbacks to the end of the linked-list */
		(*cfg_child_cb_last)->next = cb_first;
		*cfg_child_cb_last = cb_last;
	}

	CFG_UNLOCK();

}

/* creates a structure for a per-child process callback */
cfg_child_cb_t *cfg_child_cb_new(str *name, cfg_on_set_child cb)
{
	cfg_child_cb_t	*cb_struct;

	cb_struct = (cfg_child_cb_t *)shm_malloc(sizeof(cfg_child_cb_t));
	if (!cb_struct) {
		LOG(L_ERR, "ERROR: cfg_child_cb_new(): not enough shm memory\n");
		return NULL;
	}
	memset(cb_struct, 0, sizeof(cfg_child_cb_t));
	if (name) {
		cb_struct->name.s = name->s;
		cb_struct->name.len = name->len;
	}
	cb_struct->cb = cb;
	atomic_set(&cb_struct->refcnt, 0);

	return cb_struct;
}

/* free the memory allocated for a child cb list */
void cfg_child_cb_free(cfg_child_cb_t *child_cb_first)
{
	cfg_child_cb_t	*cb, *cb_next;

	for(	cb = child_cb_first;
		cb;
		cb = cb_next
	) {
		cb_next = cb->next;
		shm_free(cb);
	}
}
