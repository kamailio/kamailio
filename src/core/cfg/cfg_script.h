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

#ifndef _CFG_SCRIPT_H
#define _CFG_SCRIPT_H

#include "../str.h"
#include "cfg_struct.h"

/* structure used for temporary storing the variables
 * which are declared in the script */
typedef struct _cfg_script_var {
	unsigned int	type;
	union {
		str	s;
		int	i;
	} val;
	int	min;
	int	max;
	struct _cfg_script_var	*next;
	int	name_len;
	char	*name;
	char	*descr;
} cfg_script_var_t;

/* allocates memory for a new config script variable
 * The value of the variable is not set!
 */
cfg_script_var_t *new_cfg_script_var(char *gname, char *vname, unsigned int type,
					char *descr);

/* Rewrite the value of an already declared script variable before forking.
 * Return value:
 * 	 0: success
 *	-1: error
 *	 1: variable not found
 */
int cfg_set_script_var(cfg_group_t *group, str *var_name,
			void *val, unsigned int val_type);

/* fix-up the dynamically declared group */
int cfg_script_fixup(cfg_group_t *group, unsigned char *block);

/* destory a dynamically allocated group definition */
void cfg_script_destroy(cfg_group_t *group);

#endif /* _CFG_SCRIPT_H */
