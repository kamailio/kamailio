/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _PDB_SERVER_BACKEND_H_
#define _PDB_SERVER_BACKEND_H_




#include "common.h"




/*
 Initializes the backend and loads the required data from the given file.
 Returns 0 on success, -1 otherwise.
*/
int init_backend(char *filename);

/*
 Finds the carrier id for the given number and returns it.
 Returns 0 if not found.
*/
carrier_t lookup_number(char *number);




#endif
