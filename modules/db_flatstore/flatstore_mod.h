/*
 * $Id$
 *
 * Copyright (C) 2004 FhG Fokus
 * Copyright (C) 2008 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _FLATSTORE_MOD_H
#define _FLATSTORE_MOD_H

/** @defgroup flatstore Fast plain-text write-only DB driver.
 * @ingroup DB_API
 */
/** @{ */

/** \file 
 * Flatstore module interface.
 */

#include "../../str.h"
#include <time.h>

extern str     flat_pid;
extern int     flat_flush;
extern str     flat_record_delimiter;
extern str     flat_delimiter;
extern str     flat_escape;
extern str     flat_suffix;
extern time_t* flat_rotate;
extern time_t  flat_local_timestamp;

/** @} */

#endif /* _FLATSTORE_MOD_H */
