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

#ifndef _CFG_H
#define _CFG_H

#include "../str.h"

/* variable type */
#define CFG_VAR_UNSET		0U
#define CFG_VAR_INT		1U
#define CFG_VAR_STRING		2U
#define CFG_VAR_STR		3U
#define CFG_VAR_POINTER		4U

/*! \brief number of bits required for the variable type */
#define CFG_INPUT_SHIFT		3

/*! \brief input types */
#define CFG_INPUT_INT		(CFG_VAR_INT << CFG_INPUT_SHIFT)
#define CFG_INPUT_STRING	(CFG_VAR_STRING << CFG_INPUT_SHIFT)
#define CFG_INPUT_STR		(CFG_VAR_STR << CFG_INPUT_SHIFT)

#define CFG_VAR_MASK(x)		((x)&((1U<<CFG_INPUT_SHIFT)-1))
#define CFG_INPUT_MASK(x)	((x)&((1U<<(2*CFG_INPUT_SHIFT))-(1U<<CFG_INPUT_SHIFT)))

#define CFG_ATOMIC		(1U<<(2*CFG_INPUT_SHIFT))	/*!< atomic change is allowed */
#define CFG_READONLY		(1U<<(2*CFG_INPUT_SHIFT+1))	/*!< variable is read-only */
#define CFG_CB_ONLY_ONCE	(1U<<(2*CFG_INPUT_SHIFT+2))	/*!< per-child process callback needs to be called only once */

typedef int (*cfg_on_change)(void *, str *, str *, void **);
typedef void (*cfg_on_set_child)(str *, str *);

/*! \brief structrure to be used by the module interface */
typedef struct _cfg_def {
	char	*name;
	unsigned int	type;
	int	min;
	int	max;
	cfg_on_change		on_change_cb;
	cfg_on_set_child	on_set_child_cb;
	char	*descr;
} cfg_def_t;

/*! \brief declares a new cfg group
 * handler is set to the memory area where the variables are stored
 * return value is -1 on error
 */
int cfg_declare(char *group_name, cfg_def_t *def, void *values, int def_size,
			void **handler);

#define cfg_sizeof(gname) \
	sizeof(struct cfg_group_##gname)

#define cfg_get(gname, handle, var) \
	((struct cfg_group_##gname *)handle)->var

/*! \brief declares a single variable with integer type */
int cfg_declare_int(char *group_name, char *var_name,
		int val, int min, int max, char *descr);

/*! \brief declares a single variable with str type */
int cfg_declare_str(char *group_name, char *var_name, char *val, char *descr);

/*! \brief Add a variable to a group instance with integer type.
 * The group instance is created if it does not exist.
 * wrapper function for new_add_var()
 */
int cfg_ginst_var_int(char *group_name, unsigned int group_id, char *var_name,
			int val);

/*! \brief Add a variable to a group instance with string type.
 * The group instance is created if it does not exist.
 * wrapper function for new_add_var()
 */
int cfg_ginst_var_string(char *group_name, unsigned int group_id, char *var_name,
			char *val);

/*! \brief Create a new group instance.
 * wrapper function for new_add_var()
 */
int cfg_new_ginst(char *group_name, unsigned int group_id);

/*! \brief returns the handle of a cfg group */
void **cfg_get_handle(char *gname);

#endif /* _CFG_H */
