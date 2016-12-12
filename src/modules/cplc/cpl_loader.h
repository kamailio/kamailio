/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 * History:
 * -------
 * 2003-06-24: file created (bogdan)
 */

#ifndef _CPL_LOADER_H
#define _CPL_LOADER_H
#include "../../lib/kmi/mi.h"

struct mi_root *mi_cpl_load(struct mi_root *cmd, void *param);
struct mi_root *mi_cpl_remove(struct mi_root *cmd, void *param);
struct mi_root *mi_cpl_get(struct mi_root *cmd, void *param);

#endif





