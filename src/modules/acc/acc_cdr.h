/*
 * Accounting module
 *
 * Copyright (C) 2011 - Sven Knoblich 1&1 Internet AG
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

/*! \file
 * \ingroup acc
 * \brief Acc:: File to handle CDR generation with the help of the dialog module
 *
 * - Module: \ref acc
 */

/*! \defgroup acc ACC :: The Kamailio accounting Module
 *
 * The ACC module is used to account transactions information to
 *  different backends like syslog, SQL, RADIUS and DIAMETER (beta
 *  version).
 *
 */

#define MAX_CDR_CORE 3
#define MAX_CDR_EXTRA 64


int set_cdr_extra( char* cdr_extra_value);
int set_cdr_facility( char* cdr_facility);
int init_cdr_generation( void);
void destroy_cdr_generation( void);



