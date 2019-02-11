/*
 * $Id$
 *
 * SQlite module interface
 *
 * Copyright (C) 2017 Julien Chavanton, Flowroute
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
 */

#ifndef DBSQLITE_H
#define DBSQLITE_H

typedef struct db_param_list {
	struct db_param_list* next;
	struct db_param_list* prev;
	str database;
	int readonly;
	str journal_mode;
} db_param_list_t;

db_param_list_t *db_param_list_search(str db_filename);

#endif
