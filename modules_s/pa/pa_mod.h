/*
 * Presence Agent, module interface
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 */

#ifndef PA_MOD_H
#define PA_MOD_H

#include "../../parser/msg_parser.h"
#include "../tm/tm_load.h"
#include "../../db/db.h"

extern int default_expires;
extern double default_priority;
extern int timer_interval;

/* TM bind */
extern struct tm_binds tmb;

/* DB module bind */
extern db_func_t pa_dbf;
extern db_con_t* pa_db;

/* PA database */
extern int use_db;
extern int use_place_table;
extern str db_url;
extern str pa_domain;
extern char *presentity_table;
extern char *presentity_contact_table;
extern char *watcherinfo_table;
extern char *place_table;
extern int use_bsearch;
extern int use_location_package;
extern int new_watcher_pending;
extern int callback_update_db;
extern int callback_lock_pdomain;
extern int new_tuple_on_publish;
extern int pa_pidf_priority;

/*
 * compare two str's
 */
int str_strcmp(const str *stra, const str *strb);
/*
 * case-insensitive compare two str's
 */
int str_strcasecmp(const str *stra, const str *strb);

#endif /* PA_MOD_H */
