/*
 * $Id:$
 *
 * Copyright (C) 2008 Voice System SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2008-04-20  initial version (bogdan)
 *
 */



#ifndef _DIALOG_DLG_PROFILE_H_
#define _DIALOG_DLG_PROFILE_H_

#include "../../parser/msg_parser.h"
#include "../../locking.h"
#include "../../str.h"


struct dlg_profile_hash {
	str value;
	struct dlg_cell *dlg;
	struct dlg_profile_hash *next;
	struct dlg_profile_hash *prev;
	unsigned int hash;
};


struct dlg_profile_link {
	struct dlg_profile_hash hash_linker;
	struct dlg_profile_link  *next;
	struct dlg_profile_table *profile;
};


struct dlg_profile_entry {
	struct dlg_profile_hash *first;
	unsigned int content;
};


struct dlg_profile_table {
	str name;
	unsigned int size;
	unsigned int has_value;
	gen_lock_t lock;
	struct dlg_profile_entry *entries;
	struct dlg_profile_table *next;
};


int add_profile_definitions( char* profiles, unsigned int has_value);

void destroy_dlg_profiles();

struct dlg_profile_table* search_dlg_profile(str *name);

int profile_cleanup( struct sip_msg *msg, void *param );

void destroy_linkers(struct dlg_profile_link *linker);

void set_current_dialog(struct sip_msg *msg, struct dlg_cell *dlg);

int set_dlg_profile(struct sip_msg *msg, str *value,
		struct dlg_profile_table *profile);

int is_dlg_in_profile(struct sip_msg *msg, struct dlg_profile_table *profile,
		str *value);

unsigned int get_profile_size(struct dlg_profile_table *profile, str *value);

struct mi_root * mi_get_profile(struct mi_root *cmd_tree, void *param );

struct mi_root * mi_profile_list(struct mi_root *cmd_tree, void *param );

#endif

