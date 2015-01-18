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

#ifndef _CFG_CTX_H
#define _CFG_CTX_H

#include "../str.h"
#include "../locking.h"
#include "cfg.h"
#include "cfg_struct.h"


/* variable values */
union cfg_var_value{
	void* vp;
	long vlong;
	int vint;
	str vstr;
	unsigned char	vraw[1]; /* variable length */
};


/** linked list of variables with their new values. */
typedef struct _cfg_changed_var {
	cfg_group_t	*group;
	cfg_mapping_t	*var;
	struct _cfg_changed_var	*next;

	unsigned int	group_id; /* valid only if group_id_set==1 */
	unsigned char	group_id_set;
	unsigned char	del_value;	/* delete the value instead of setting it */

	/* blob that contains the new value */
	union cfg_var_value new_val; /* variable size */
} cfg_changed_var_t;

/*! \brief callback that is called when a new group is declared */
typedef void (*cfg_on_declare)(str *, cfg_def_t *);

/*! \brief linked list of registered contexts */
typedef struct _cfg_ctx {
	/* variables that are already changed
	but have not been committed yet */
	cfg_changed_var_t	*changed_first;
	/* lock protecting the linked-list of
	changed variables */
	gen_lock_t		lock;

	/* callback that is called when a new
	group is registered */
	cfg_on_declare		on_declare_cb;

	struct _cfg_ctx	*next;
} cfg_ctx_t;

#define CFG_CTX_LOCK(ctx)	lock_get(&(ctx)->lock)
#define CFG_CTX_UNLOCK(ctx)	lock_release(&(ctx)->lock)

/*! \brief creates a new config context that is an interface to the
 * cfg variables with write permission */
int cfg_register_ctx(cfg_ctx_t **handle, cfg_on_declare on_declare_cb);

/*! \brief free the memory allocated for the contexts */
void cfg_ctx_destroy(void);

/*! \brief set the value of a variable without the need of explicit commit */
int cfg_set_now(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			void *val, unsigned int val_type);
int cfg_set_now_int(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			int val);
int cfg_set_now_string(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			char *val);
int cfg_set_now_str(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			str *val);

/*! \brief Delete a variable from the group instance.
 * wrapper function for cfg_set_now */
int cfg_del_now(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name);

/* sets the value of a variable but does not commit the change */
int cfg_set_delayed(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			void *val, unsigned int val_type);
int cfg_set_delayed_int(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			int val);
int cfg_set_delayed_string(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			char *val);
int cfg_set_delayed_str(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			str *val);

/*! \brief Delete a variable from the group instance.
 * wrapper function for cfg_set_delayed */
int cfg_del_delayed(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name);

/*! \brief commits the previously prepared changes within the context */
int cfg_commit(cfg_ctx_t *ctx);

/*! \brief drops the not yet committed changes within the context */
int cfg_rollback(cfg_ctx_t *ctx);

/*! \brief returns the value of a variable */
int cfg_get_by_name(cfg_ctx_t *ctx, str *group_name, unsigned int *group_id, str *var_name,
			void **val, unsigned int *val_type);

/*! \brief returns the description of a variable */
int cfg_help(cfg_ctx_t *ctx, str *group_name, str *var_name,
			char **ch, unsigned int *input_type);

/*! \brief notify the drivers about the new config definition */
void cfg_notify_drivers(char *group_name, int group_name_len, cfg_def_t *def);

/*! \brief convert the value to the requested type.
 * Do not forget the call convert_val_cleaup afterwards. */
int convert_val(unsigned int val_type, void *val,
			unsigned int var_type, void **new_val);

/*! \brief cleanup function for convert_val() */
void convert_val_cleanup(void);

/*! \brief initialize the handle for cfg_get_group_next() */
#define cfg_get_group_init(handle) \
	(*(handle)) = (void *)cfg_group

/*! \brief returns the group name and the cfg structure definition,
 * and moves the handle to the next group
 *
 * Return value:
 *	0: no more group
 *	1: group exists
 *
 * can be used as follows:
 *
 * void	*handle;
 * cfg_get_group_init(&handle)
 * while (cfg_get_group_next(&handle, &name, &def)) {
 * 	...
 * }
 */
int cfg_get_group_next(void **h,
			str *gname, cfg_def_t **def);

/*! \brief Initialize the handle for cfg_diff_next()
 * WARNING: keeps the context lock held, do not forget
 * to release it with cfg_diff_release()
 */
int cfg_diff_init(cfg_ctx_t *ctx,
		void **h);

/*! \brief return the pending changes that have not been
 * committed yet
 * return value:
 *	1: valid value is found
 *	0: no more changed value found
 *	-1: error occured
 *
 *
 * can be used as follows:
 *
 * void *handle;
 * if (cfg_diff_init(ctx, &handle)) return -1
 * while ((err = cfg_diff_next(&handle
 *			&group_name, &group_id, &var_name,
 *			&old_val, &new_val
 *			&val_type)) > 0
 * ) {
 *		...
 * }
 * cfg_diff_release(ctx);
 * if (err) {
 *	error occured, the changes cannot be retrieved
 *	...
 * }
 */
int cfg_diff_next(void **h,
			str *gname, unsigned int **gid, str *vname,
			void **old_val, void **new_val,
			unsigned int *val_type);

/*! \brief destroy the handle of cfg_diff_next() */
void cfg_diff_release(cfg_ctx_t *ctx);

/* Add a new instance to an existing group */
int cfg_add_group_inst(cfg_ctx_t *ctx, str *group_name, unsigned int group_id);

/* Delete an instance of a group */
int cfg_del_group_inst(cfg_ctx_t *ctx, str *group_name, unsigned int group_id);

/* Check the existance of a group instance.
 * return value:
 *	1: exists
 *	0: does not exist
 */
int cfg_group_inst_exists(cfg_ctx_t *ctx, str *group_name, unsigned int group_id);

/* Apply the changes to a group instance as long as the additional variable
 * belongs to the specified group_id. *add_var_p is moved to the next additional
 * variable, and all the consumed variables are freed.
 */
int cfg_apply_list(cfg_group_inst_t *ginst, cfg_group_t *group,
			unsigned int group_id, cfg_add_var_t **add_var_p);

#endif /* _CFG_CTX_H */
