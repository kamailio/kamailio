/*
 * header file of utils.c
 *
 * Copyright (C) 2008 Juha Heinanen
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

/*!
 * \file
 * \brief Kamailio utils :: Core
 * \ingroup utils
 * Module: \ref utils
 */



#ifndef UTILS_H
#define UTILS_H

#include "../../str.h"
#include "../../lib/srdb1/db.h"

extern int http_query_timeout;
extern db1_con_t *pres_dbh;
extern db_func_t pres_dbf;

typedef struct {
	char		*buf;
	size_t		curr_size;
	size_t		pos;
} http_res_stream_t;

#endif /* UTILS_H */
