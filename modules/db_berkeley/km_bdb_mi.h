/*
 * $Id: $
 *
 * Header file for db_berkeley MI functions
 *
 * Copyright (C) 2007 Cisco Systems
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*! \file
 * Berkeley DB : Management interface
 *
 * \ingroup database
 */


#ifndef _KM_BDB_MI_H_
#define _KM_BDB_MI_H_

#include "../../lib/kmi/mi.h"

#define MI_BDB_RELOAD "bdb_reload"

struct mi_root* mi_bdb_reload(struct mi_root *cmd, void *param);


#endif
