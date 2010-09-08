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
 *  2008-01-24	dynamic groups are introduced in order to make
 *		variable declaration possible in the script (Miklos)
 */

#ifndef _CFG_STRUCT_H
#define _CFG_STRUCT_H

#include "../str.h"
#include "../atomic_ops.h"
#include "../mem/shm_mem.h"
#include "../locking.h"
#include "../compiler_opt.h"
#include "../bit_test.h"
#include "cfg.h"

/*! \brief Maximum number of variables within a configuration group. */
#define CFG_MAX_VAR_NUM	256

/*! \brief indicates that the variable has been already shmized */
#define cfg_var_shmized	1U

/*! \brief Structure for storing additional values of a variable.
 * When the config is shmzied, these variables are combined in
 * an array.
 */
typedef struct _cfg_add_var {
	unsigned int	type;
	union {
		str	s;
		int	i;
	} val;
	unsigned int	group_id; /*!< Id of the group instance */
	struct _cfg_add_var	*next;
} cfg_add_var_t;

/*! \brief structure used for variable - pointer mapping */
typedef struct _cfg_mapping {
	cfg_def_t	*def;		/*!< one item of the cfg structure definition */
	int		name_len;	/*!< length of def->name */

	/* additional information about the cfg variable */
	int		pos;	/*!< position of the variable within the group starting from 0 */
	int		offset; /*!< offest within the memory block */
	unsigned int	flag;	/*!< flag indicating the state of the variable */
} cfg_mapping_t;

/*! \brief linked list of registered groups */
typedef struct _cfg_group {
	int		num;		/*!< number of variables within the group */
	cfg_mapping_t	*mapping;	/*!< describes the mapping betweeen
					the cfg variable definition and the memory block */
	char		*vars;		/*!< pointer to the memory block where the values
					are stored -- used only before the config is
					shmized. */
	cfg_add_var_t	*add_var;	/*!< Additional instances of the variables.
					This linked list is used only before the config is
					shmized. */
	int		size;		/*!< size of the memory block that has to be
					allocated to store the values */
	int		meta_offset;	/*!< offset of the group within the
					shmized memory block for the meta_data */
	int		var_offset;	/*!< offset of the group within the
					shmized memory block for the variables */
	void		**handle;	/*!< per-process handle that can be used
					by the modules to access the variables.
					It is registered when the group is created,
					and updated every time the block is replaced */

	unsigned char	dynamic;	/*!< indicates whether the variables within the group
					are dynamically	allocated or not */
	struct _cfg_group	*next;
	int		name_len;	
	char		name[1];
} cfg_group_t;

/*! \brief One instance of the cfg group variables which stores
 * the additional values. These values can overwrite the default values. */
typedef struct _cfg_group_inst {
	unsigned int	id;		/*!< identifier of the group instance */
	unsigned int	set[CFG_MAX_VAR_NUM/(sizeof(int)*8)];
					/*!< Bitmap indicating whether or not a value is explicitely set
					within this instance. If the value is not set,
					then the default value is used, and copied into this instance. */
	unsigned char	vars[1];	/*!< block for the values */
} cfg_group_inst_t;

/*! \bried Meta-data which is stored before each variable group
 * within the blob. This structure is used to handle the multivalue
 * instances of the variables, i.e. manages the array for the
 * additional values. */
typedef struct _cfg_group_meta {
	int			num;	/*!< Number of items in the array */
	cfg_group_inst_t	*array;	/*!< Array of cfg groups with num number of items */
} cfg_group_meta_t;

/*! \brief single memoy block that contains all the cfg values */
typedef struct _cfg_block {
	atomic_t	refcnt;		/*!< reference counter,
					the block is automatically deleted
					when it reaches 0 */
	void		**replaced;	/*!< set of the strings and other memory segments
					that must be freed
					together with the block. The content depends
					on the block that replaces this one */
	unsigned char	vars[1];	/*!< blob that contains the values */
} cfg_block_t;

/*! \brief Linked list of per-child process callbacks.
 * Each child process has a local pointer, and executes the callbacks
 * when the pointer is not pointing to the end of the list.
 * Items from the begginning of the list are deleted when the starter
 * pointer is moved, and no more child process uses them.
 */
typedef struct _cfg_child_cb {
	atomic_t		refcnt; /*!< number of child processes
					referring to the element */
	atomic_t		cb_count;	/*!< This counter is used to track
						 * how many times the callback needs
						 * to be executed.
						 * >0 the cb needs to be executed
						 * <=0 the cb no longer needs to be executed
						 */
	str			gname, name;	/*!< name of the variable that has changed */
	cfg_on_set_child	cb;	/*!< callback function that has to be called */

	struct _cfg_child_cb	*next;
} cfg_child_cb_t;

extern cfg_group_t	*cfg_group;
extern cfg_block_t	**cfg_global;
extern cfg_block_t	*cfg_local;
extern gen_lock_t	*cfg_global_lock;
extern gen_lock_t	*cfg_writer_lock;
extern int		cfg_shmized;
extern cfg_child_cb_t	**cfg_child_cb_first;
extern cfg_child_cb_t	**cfg_child_cb_last;
extern cfg_child_cb_t	*cfg_child_cb;

/* magic value for cfg_child_cb for processes that do not want to
   execute per-child callbacks */
#define CFG_NO_CHILD_CBS ((void*)(long)(-1))

/* macros for easier variable access */
#define CFG_VAR_TYPE(var)	CFG_VAR_MASK((var)->def->type)
#define CFG_INPUT_TYPE(var)	CFG_INPUT_MASK((var)->def->type)

/* get the meta-data of a group from the block */
#define CFG_GROUP_META(block, group) \
	((cfg_group_meta_t *)((block)->vars + (group)->meta_offset))

/* get the data block of a group from the block */
#define CFG_GROUP_DATA(block, group) \
	((unsigned char *)((block)->vars + (group)->var_offset))

/* Test whether a variable is explicitely set in the group instance,
 * or it uses the default value */
#define CFG_VAR_TEST(group_inst, var) \
	bit_test((var)->pos % (sizeof(int)*8), (group_inst)->set + (var)->pos/(sizeof(int)*8))

/* Test whether a variable is explicitely set in the group instance,
 * or it uses the default value, and set the flag. */
#define CFG_VAR_TEST_AND_SET(group_inst, var) \
	bit_test_and_set((var)->pos % (sizeof(int)*8), (group_inst)->set + (var)->pos/(sizeof(int)*8))

/* initiate the cfg framework */
int sr_cfg_init(void);

/* destroy the memory allocated for the cfg framework */
void cfg_destroy(void);

/* Register num number of child processes that will
 * keep updating their local configuration.
 * This function needs to be called from mod_init
 * before any child process is forked.
 */
void cfg_register_child(int num);

/* per-child process init function.
 * It needs to be called from the forked process.
 * cfg_register_child() must be called before this function!
 */
int cfg_child_init(void);

/* Child process init function that can be called
 * without cfg_register_child().
 * Note that the child process may miss some configuration changes.
 */
int cfg_late_child_init(void);

/* per-child init function for non-cb executing processes.
 * Mark this process as not wanting to execute any per-child config
 * callback (it will have only limited config functionality, but is useful
 * when a process needs only to watch some non-callback cfg. values,
 * e.g. the main attendant process, debug and memlog).
 * It needs to be called from the forked process.
 * cfg_register_child must _not_ be called.
 */
int cfg_child_no_cb_init(void);

/* per-child process destroy function
 * Should be called only when the child process exits,
 * but SER continues running.
 *
 * WARNING: this function call must be the very last action
 * before the child process exits, because the local config
 * is not available afterwards.
 */
void cfg_child_destroy(void);

/* creates a new cfg group, and adds it to the linked list */
cfg_group_t *cfg_new_group(char *name, int name_len,
		int num, cfg_mapping_t *mapping,
		char *vars, int size, void **handle);

/* copy the variables to shm mem */
int cfg_shmize(void);

/* free the memory of a config block */
static inline void cfg_block_free(cfg_block_t *block)
{
	int	i;

	/* free the changed variables */
	if (block->replaced) {
		for (i=0; block->replaced[i]; i++)
			shm_free(block->replaced[i]);
		shm_free(block->replaced);
	}
	shm_free(block);
}

/* lock and unlock the global cfg block -- used only at the
 * very last step when the block is replaced */
#define CFG_LOCK()	lock_get(cfg_global_lock);
#define CFG_UNLOCK()	lock_release(cfg_global_lock);

/* lock and unlock used by the cfg drivers to make sure that
 * only one driver process is considering replacing the global
 * cfg block */
#define CFG_WRITER_LOCK()	lock_get(cfg_writer_lock);
#define CFG_WRITER_UNLOCK()	lock_release(cfg_writer_lock);

/* increase and decrease the reference counter of a block */
#define CFG_REF(block) \
	atomic_inc(&(block)->refcnt)

#define CFG_UNREF(block) \
	do { \
		if (atomic_dec_and_test(&(block)->refcnt)) \
			cfg_block_free(block); \
	} while(0)

/* updates all the module handles and calls the
 * per-child process callbacks -- not intended to be used
 * directly, use cfg_update() instead!
 * params:
 *   no_cbs - if 1, do not call per child callbacks
 */
static inline void cfg_update_local(int no_cbs)
{
	cfg_group_t	*group;
	cfg_child_cb_t	*last_cb;
	cfg_child_cb_t	*prev_cb;

	if (cfg_local) CFG_UNREF(cfg_local);
	CFG_LOCK();
	CFG_REF(*cfg_global);
	cfg_local = *cfg_global;
	/* the value of the last callback must be read within the lock */
	last_cb = *cfg_child_cb_last;

	/* I unlock now, because the child process can update its own private
	config without the lock held. In the worst case, the process will get the
	lock once more to set cfg_child_cb_first, but only one of the child
	processes will do so, and only if a value, that has per-child process
	callback defined, was changed. */
	CFG_UNLOCK();

	/* update the handles */
	for (	group = cfg_group;
		group;
		group = group->next
	)
		*(group->handle) = CFG_GROUP_DATA(cfg_local, group);

	if (unlikely(cfg_child_cb==CFG_NO_CHILD_CBS || no_cbs))
		return;
	/* call the per-process callbacks */
	while (cfg_child_cb != last_cb) {
		prev_cb = cfg_child_cb;
		cfg_child_cb = cfg_child_cb->next;
		atomic_inc(&cfg_child_cb->refcnt);
		if (atomic_dec_and_test(&prev_cb->refcnt)) {
			/* No more pocess refers to this callback.
			Did this process block the deletion,
			or is there any other process that has not
			reached	prev_cb yet? */
			CFG_LOCK();
			if (*cfg_child_cb_first == prev_cb) {
				/* yes, this process was blocking the deletion */
				*cfg_child_cb_first = cfg_child_cb;
				CFG_UNLOCK();
				shm_free(prev_cb);
			} else {
				CFG_UNLOCK();
			}
		}
		if (atomic_add(&cfg_child_cb->cb_count, -1) >= 0) /* the new value is returned
								by atomic_add() */
			/* execute the callback */
			cfg_child_cb->cb(&cfg_child_cb->gname, &cfg_child_cb->name);
		/* else the callback no longer needs to be executed */
	}
}

/* sets the local cfg block to the active block
 * 
 * If your module forks a new process that implements
 * an infinite loop, put cfg_update() to the beginning of
 * the cycle to make sure, that subsequent function calls see the
 * up-to-date config set.
 */
#define cfg_update() \
	do { \
		if (unlikely(cfg_local != *cfg_global)) \
			cfg_update_local(0); \
	} while(0)

/* like cfg_update(), but does not execute callbacks
 * (it should be used sparingly only in special cases, since it
 *  breaks an important cfg framework feature)
 */
#define cfg_update_no_cbs() \
	do { \
		if (unlikely(cfg_local != *cfg_global)) \
			cfg_update_local(1); \
	} while(0)

/* searches a group by name */
cfg_group_t *cfg_lookup_group(char *name, int len);
	
/* searches a variable definition by group and variable name */
int cfg_lookup_var(str *gname, str *vname,
			cfg_group_t **group, cfg_mapping_t **var);

/* clones the global config block
 * WARNING: unsafe, cfg_writer_lock or cfg_global_lock must be held!
 */
cfg_block_t *cfg_clone_global(void);

/* Clone an array of configuration group instances. */
cfg_group_inst_t *cfg_clone_array(cfg_group_meta_t *meta, cfg_group_t *group);

/* Extend the array of configuration group instances with one more instance.
 * Only the ID of the new group is set, nothing else. */
cfg_group_inst_t *cfg_extend_array(cfg_group_meta_t *meta, cfg_group_t *group,
				unsigned int group_id,
				cfg_group_inst_t **new_group);

/* clones a string to shared memory */
int cfg_clone_str(str *src, str *dst);

/* Find the group instance within the meta-data based on the group_id */
cfg_group_inst_t *cfg_find_group(cfg_group_meta_t *meta, int group_size, unsigned int group_id);

/* append new callbacks to the end of the child callback list
 *
 * WARNING: the function is unsafe, either hold CFG_LOCK(),
 * or call the function before forking
 */
void cfg_install_child_cb(cfg_child_cb_t *cb_first, cfg_child_cb_t *cb_last);

/* installs a new global config
 *
 * replaced is an array of strings that must be freed together
 * with the previous global config.
 * cb_first and cb_last define a linked list of per-child process
 * callbacks. This list is added to the global linked list.
 */
void cfg_install_global(cfg_block_t *block, void **replaced,
			cfg_child_cb_t *cb_first, cfg_child_cb_t *cb_last);

/* creates a structure for a per-child process callback */
cfg_child_cb_t *cfg_child_cb_new(str *gname, str *name,
			cfg_on_set_child cb,
			unsigned int type);

/* free the memory allocated for a child cb list */
void cfg_child_cb_free(cfg_child_cb_t *child_cb_first);

#endif /* _CFG_STRUCT_H */
