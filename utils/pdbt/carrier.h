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

#ifndef _CARRIER_H_
#define _CARRIER_H_




#include "common.h"




/*
 Initializes data structures.
 Must be called before any of the other functions!
 Returns 0 on success, -1 otherwise.
*/
void init_carrier_names();

/*
 Loads carrier names from a file.
 Format: "D[0-9][0-9][0-9] <name>".
*/
int load_carrier_names(char *filename);

/*
  Returns a name for the given carrier id.
  Always returns a string, even if id is invalid or the id is unknown.
*/
char *carrierid2name(carrier_t carrier);




#endif
