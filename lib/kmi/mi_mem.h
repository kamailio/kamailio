/*
 * $Id$
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*!
 * \file 
 * \brief MI_MEM :: MI Memory Management
 * \ingroup mi
 */


#ifndef _MI_MEM_H_
#define _MI_MEM_H_

#include "../../mem/shm_mem.h"

#ifdef MI_SYSTEM_MALLOC
#include <stdlib.h>
#define mi_malloc malloc
#define mi_realloc realloc
#define mi_free free
#else
#include "../../mem/mem.h"
#define mi_malloc pkg_malloc
#define mi_realloc pkg_realloc
#define mi_free pkg_free
#endif

#endif

