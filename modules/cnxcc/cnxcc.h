/*
 * $Id$
 *
 * Copyright (C) 2012 Carlos Ruiz Díaz (caruizdiaz.com),
 *                    ConexionGroup (www.conexiongroup.com)
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

#ifndef _CNXCC_H
#define _CNXCC_H

#include "../../str.h"

#define DATETIME_SIZE		sizeof("0001-01-01 00:00:00")
#define DATETIME_LENGTH		DATETIME_SIZE - 1


void get_datetime(str *dest);
unsigned int get_current_timestamp();
int timestamp2isodt(str *dest, unsigned int timestamp);
double str2double(str *string);

#endif /* _CNXCC_H */
