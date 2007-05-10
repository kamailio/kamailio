/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007 1und1 Internet AG
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
 */


#ifndef DB_UT_H
#define DB_UT_H


/* for strptime, use 600 for 'Single UNIX Specification, Version 3' */
#define _XOPEN_SOURCE 600          /* glibc2 on linux, bsd */
#define _XOPEN_SOURCE_EXTENDED 1   /* solaris */

#include <time.h>

#undef _XOPEN_SOURCE
#undef _XOPEN_SOURCE_EXTENDED

#include "db_key.h"

int db_str2int(const char* _s, int* _v);

int db_str2double(const char* _s, double* _v);

int db_int2str(int _v, char* _s, int* _l);

int db_double2str(double _v, char* _s, int* _l);

int db_time2str(time_t _v, char* _s, int* _l);

int db_str2time(const char* _s, time_t* _v);

int db_print_columns(char* _b, int _l, db_key_t* _c, int _n);

#endif
