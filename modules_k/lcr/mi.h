/*
 * $Id$
 *
 * Header file for lcr MI functions
 *
 * Copyright (C) 2006 Voice Sistem SRL
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-10-05  created (bogdan)
 */


#ifndef _LCR_MI_H_
#define _LCR_MI_H_

#include "../../mi/mi.h"

#define MI_LCR_RELOAD "lcr_reload"
#define MI_LCR_GW_DUMP "lcr_gw_dump"
#define MI_LCR_LCR_DUMP "lcr_lcr_dump"

struct mi_root* mi_lcr_reload(struct mi_root* cmd, void* param);

struct mi_root* mi_lcr_gw_dump(struct mi_root* cmd, void* param);

struct mi_root* mi_lcr_lcr_dump(struct mi_root* cmd, void* param);

#endif
