/*
 * Domain module headers
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version
 *
 * sip-router is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _UID_DOMAIN_MOD_H
#define _UID_DOMAIN_MOD_H

#include "../../lib/srdb2/db.h"
#include "../../str.h"
#include "../../usr_avp.h"
#include "../../parser/msg_parser.h"
#include "domain.h"


/*
 * Module parameters variables
 */
extern int db_mode;             /* Database usage mode: 0 = no cache,
								 * 1 = * cache */
extern str domain_table;	/* Domain table name */
extern str domain_col;   	/* Domain column name */
extern str did_col;             /* Domain id col */
extern str flags_col;           /* Flags column */

/*
 * Table containing domain attributes (in form of AVPs)
 */
extern str domattr_table;       /* Name of table containing domain attributes */
extern str domattr_did;         /* Column containing domain id */
extern str domattr_name;        /* Column containing name of attribute */
extern str domattr_type;        /* Column containing type of attribute */
extern str domattr_value;       /* Column containing value of attribute */
extern str domattr_flags;       /* Column containing domain attribute flags */

extern int load_domain_attrs;   /* Turn on/off domain attributes */

/*
 * Other module variables
 */

extern struct hash_entry*** active_hash; /* Pointer to current hash table */
extern domain_t** domains_1;      /* List of domains 1 */
extern domain_t** domains_2;      /* List of domains 2 */

extern struct hash_entry*** hash;  /* Pointer to the current hash table */
extern struct hash_entry** hash_1; /* Hash table 1 */
extern struct hash_entry** hash_2; /* Hash table 2 */

extern db_cmd_t* load_domains_cmd, *get_did_cmd, *load_attrs_cmd;

int reload_domain_list(void);

#endif /* _DOMAIN_MOD_H */
