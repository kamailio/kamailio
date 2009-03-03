/*
 * $Id$
 *
 * Header file for PIKE MI functions
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
 *  2006-12-05  created (bogdan)
 */


#ifndef _PIKE_MI_H_
#define _PIKE_MI_H_

#include "../../lib/kmi/mi.h"

#define MI_PIKE_LIST      "pike_list"

struct mi_root* mi_pike_list(struct mi_root* cmd_tree, void* param);

#endif


