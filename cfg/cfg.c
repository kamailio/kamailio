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

#include "../ut.h"
#include "../mem/mem.h"
#include "cfg_struct.h"
#include "cfg_ctx.h"
#include "cfg.h"

/* declares a new cfg group
 * handler is set to the memory area where the variables are stored
 * return value is -1 on error
 */
int cfg_declare(char *group_name, cfg_def_t *def, void *values, int def_size,
			void **handle)
{
	int	i, num, size;
	cfg_mapping_t	*mapping = NULL;

	/* check the number of the variables */
	for (num=0; def[num].name; num++);

	mapping = (cfg_mapping_t *)pkg_malloc(sizeof(cfg_mapping_t)*num);
	if (!mapping) {
		LOG(L_ERR, "ERROR: register_cfg_def(): not enough memory\n");
		goto error;
	}
	memset(mapping, 0, sizeof(cfg_mapping_t)*num);

	/* calculate the size of the memory block that has to
	be allocated for the cfg variables, and set the content of the 
	cfg_mapping array the same time */
	for (i=0, size=0; i<num; i++) {
		mapping[i].def = &(def[i]);
		mapping[i].name_len = strlen(def[i].name);

		/* padding depends on the type of the next variable */
		switch (CFG_VAR_MASK(def[i].type)) {

		case CFG_VAR_INT:
			size = ROUND_INT(size);
			mapping[i].offset = size;
			size += sizeof(int);
			break;

		case CFG_VAR_STRING:
		case CFG_VAR_POINTER:
			size = ROUND_POINTER(size);
			mapping[i].offset = size;
			size += sizeof(char *);
			break;

		case CFG_VAR_STR:
			size = ROUND_POINTER(size);
			mapping[i].offset = size;
			size += sizeof(str);
			break;

		default:
			LOG(L_ERR, "ERROR: register_cfg_def(): %s.%s: unsupported variable type\n",
					group_name, def[i].name);
			goto error;
		}

		/* verify the type of the input */
		if (CFG_INPUT_MASK(def[i].type)==0) {
			def[i].type |= def[i].type << 3;
		} else {
			if ((CFG_INPUT_MASK(def[i].type) != CFG_VAR_MASK(def[i].type) << 3)
			&& (def[i].on_change_cb == 0)) {
				LOG(L_ERR, "ERROR: register_cfg_def(): %s.%s: variable and input types are "
					"different, but no callback is defined for conversion\n",
					group_name, def[i].name);
				goto error;
			}
		}

		if (CFG_INPUT_MASK(def[i].type) > CFG_INPUT_STR) {
			LOG(L_ERR, "ERROR: register_cfg_def(): %s.%s: unsupported input type\n",
					group_name, def[i].name);
			goto error;
		}
	}

	/* minor validation */
	if (size != def_size) {
		LOG(L_ERR, "ERROR: register_cfg_def(): the specified size of the config "
			"structure does not equal with the calculated size, check whether "
			"the variable types are correctly defined!\n");
		goto error;
	}

	/* create a new group
	I will allocate memory in shm mem for the variables later in a single block,
	when we know the size of all the registered groups. */
	if (cfg_new_group(group_name, num, mapping, values, size, handle))
		goto error;

	/* The cfg variables are ready to use, let us set the handle
	before passing the new definitions to the drivers.
	We make the interface usable for the fixup functions
	at this step */
	*handle = values;

	/* notify the drivers about the new config definition */
	cfg_notify_drivers(group_name, def);

	LOG(L_DBG, "DEBUG: register_cfg_def(): "
		"new config group has been registered: '%s' (num=%d, size=%d)\n",
		group_name, num, size);

	/* TODO: inform the drivers about the new definition */

	return 0;

error:
	if (mapping) pkg_free(mapping);
	LOG(L_ERR, "ERROR: register_cfg_def(): Failed to register the config group: %s\n",
			group_name);

	return -1;
}
