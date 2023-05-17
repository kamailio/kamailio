/* 
 * PostgreSQL Database Driver for Kamailio
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _PG_MOD_H
#define _PG_MOD_H

/** @defgroup postgres PostgreSQL Database Driver
 * @ingroup DB_API 
 */
/** @{ */

/** \file 
 * Postgres module interface.
 */

extern int pg_retries;
extern int pg_timeout;
extern int pg_keepalive;

typedef struct pg_con_param_s
{
	char *name;
	char *value;
	struct pg_con_param_s *next;
} pg_con_param_t;

/** @} */

#endif /* _PG_MOD_H */
