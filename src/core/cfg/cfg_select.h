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

#ifndef _CFG_SELECT_H
#define _CFG_SELECT_H

#include "../select.h"

struct cfg_read_handle {
	void	*group;
	void	*var;
};

/* free the list of not yet fixed selects */
void cfg_free_selects(void);

/* fix-up the select calls */
int cfg_fixup_selects(void);

int select_cfg_var(str *res, select_t *s, struct sip_msg *msg);

/* fix-up function for read_cfg_var()
 *
 * return value:
 * >0 - success
 *  0 - the variable has not been declared yet, but it will be automatically
 *	fixed-up later.
 * <0 - error
 */
int read_cfg_var_fixup(char *gname, char *vname, struct cfg_read_handle *read_handle);

/* read the value of a variable via a group and variable name previously fixed up
 * Returns the type of the variable
 */
unsigned int read_cfg_var(struct cfg_read_handle *read_handle, void **val);

/* wrapper functions for read_cfg_var() -- convert the value to the requested format */
int read_cfg_var_int(struct cfg_read_handle *read_handle, int *val);
int read_cfg_var_str(struct cfg_read_handle *read_handle, str *val);

/* return the selected group instance */
int cfg_selected_inst(str *res, select_t *s, struct sip_msg *msg);

#endif /* _CFG_SELECT_H */
