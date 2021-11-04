/*
 * PERMISSIONS module
 *
 * Copyright (C) 2003 Miklós Tirpák (mtirpak@sztaki.hu)
 * Copyright (C) 2006 Juha Heinanen
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

#ifndef PERMISSIONS_H
#define PERMISSIONS_H 1

#include "../../core/sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../core/pvar.h"
#include "rule.h"

#define DEFAULT_ALLOW_FILE "permissions.allow"
#define DEFAULT_DENY_FILE  "permissions.deny"

typedef struct rule_file {
	rule* rules;    /* Parsed rule set */
	char* filename; /* The name of the file */
} rule_file_t;

/*
 * Maximum number if allow/deny file pairs that can be opened
 * at any time
 */
#define MAX_RULE_FILES 64

extern str perm_db_url;        /* Database URL */
extern int perm_db_mode;       /* Database usage mode: 0=no cache, 1=cache */
extern str perm_trusted_table; /* Name of trusted table */
extern str perm_source_col;    /* Name of source address column */
extern str perm_proto_col;     /* Name of protocol column */
extern str perm_from_col;      /* Name of from pattern column */
extern str perm_ruri_col;      /* Name of RURI pattern column */
extern str perm_tag_col;       /* Name of tag column */
extern str perm_priority_col;  /* Name of priority column */
extern str perm_address_table; /* Name of address table */
extern str perm_grp_col;       /* Name of address group column */
extern str perm_ip_addr_col;   /* Name of ip address column */
extern str perm_mask_col;      /* Name of mask column */
extern str perm_port_col;      /* Name of port column */
extern int perm_peer_tag_mode; /* Matching mode */
extern int perm_reload_delta;  /* seconds between RPC reloads */
extern int perm_trusted_table_interval; /* interval of timer to clean old trusted data */

/* backends to be loaded */
#define PERM_LOAD_ADDRESSDB	(1<<0)
#define PERM_LOAD_TRUSTEDDB	(1<<1)
#define PERM_LOAD_ALLOWFILE	(1<<2)
#define PERM_LOAD_DENYFILE	(1<<3)
extern int _perm_load_backends; /* */
extern time_t *perm_rpc_reload_time;

typedef struct int_or_pvar {
	unsigned int i;
	pv_spec_t *pvar;  /* zero if int */
} int_or_pvar_t;

#define DISABLE_CACHE 0
#define ENABLE_CACHE 1

extern char *perm_allow_suffix;
int allow_test(char *file, char *uri, char *contact);

#endif
