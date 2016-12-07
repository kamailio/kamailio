/* 
 * $Id$
 *
 * ALIAS_DB Module
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of a module for Kamailio, a free SIP server.
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
 * History:
 * --------
 * 2004-09-01: first version (ramona)
 */


#ifndef _ALOOKUP_H_
#define _ALOOKUP_H_

#include "../../core/parser/msg_parser.h"

#define ALIAS_REVERSE_FLAG	(1<<0)
#define ALIAS_DOMAIN_FLAG	(1<<1)

int alias_db_lookup(struct sip_msg* _msg, str _table);
int alias_db_lookup_ex(struct sip_msg* _msg, str _table, unsigned long flags);
int alias_db_find(struct sip_msg* _msg, str _table, char* _in, char* _out,
		char* flags);

#endif /* _ALOOKUP_H_ */
