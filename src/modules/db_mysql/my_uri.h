/*
 * MySQL module interface
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2006-2007 iptelorg GmbH
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

#ifndef _MY_URI_H
#define _MY_URI_H

#include "../../lib/srdb2/db_uri.h"
#include "../../lib/srdb2/db_drv.h"

struct my_uri {
	db_drv_t drv;
	char* username;
	char* password;
	char* host;
	unsigned short port;
	char* database;
};


int my_uri(db_uri_t* uri);


#endif /* _MY_URI_H */

