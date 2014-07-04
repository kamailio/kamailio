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

#ifndef _COMMON_H_
#define _COMMON_H_




#include <stdint.h> 




/*
 0 no carrier id defined.
 1..999 are regular carrier ids.
 1000 is used as fake carrier id when merging carriers we are not interested in.
 -1000..-1 used in dtm to indicate a carrier id and that no more nodes will follow (leaf node compression).
 -1001 used in dtm to mark a pointer to a child node as NULL.
*/
#define MIN_PDB_CARRIERID 1
#define MAX_PDB_CARRIERID 999
#define OTHER_CARRIERID 1000
#define MAX_CARRIERID 1000
#define NULL_CARRIERID -1001

#define IS_VALID_PDB_CARRIERID(id) ((id>=MIN_PDB_CARRIERID) && (id<=MAX_PDB_CARRIERID))
#define IS_VALID_CARRIERID(id) ((id>=MIN_PDB_CARRIERID) && (id<=MAX_CARRIERID))




typedef int16_t carrier_t;




#endif
