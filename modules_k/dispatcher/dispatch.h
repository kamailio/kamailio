/**
 * $Id$
 *
 * dispatcher module
 * 
 * Copyright (C) 2004-2006 FhG Fokus
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
 * History
 * -------
 * 2004-07-31  first version, by daniel
 */

#ifndef _DISPATCH_H_
#define _DISPATCH_H_

#include <stdio.h>
#include "../../items.h"
#include "../../parser/msg_parser.h"


#define DS_HASH_USER_ONLY	1  /* use only the uri user part for hashing */
#define DS_FAILOVER_ON		2  /* store the other dest in avps */
#define DS_INACTIVE_DST		1  /* inactive destination */

typedef struct _ds_param
{
	int type;
	union {
		int id;
		xl_spec_t sp;
	} v;
} ds_param_t, *ds_param_p;

extern int ds_flags; 
extern int ds_use_default;

extern int_str dst_avp_name;
extern unsigned short dst_avp_type;
extern int_str grp_avp_name;
extern unsigned short grp_avp_type;
extern int_str cnt_avp_name;
extern unsigned short cnt_avp_type;

int ds_load_list(char *lfile);
int ds_destroy_list();
int ds_select_dst(struct sip_msg *msg, int set, int alg, int mode);
int ds_next_dst(struct sip_msg *msg, int mode);
int ds_set_state(int group, str *address, int state, int type);
int ds_mark_dst(struct sip_msg *msg, int mode);
int ds_print_list(FILE *fout);
int ds_print_mi_list(struct mi_node* rpl);

#endif

