/* $Id: nathelper.c 1808 2007-03-10 17:36:19Z bogdan_iancu $
 *
 * Copyright (C) 2003 Porta Software Ltd
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
 * History:
 * ---------
 * 2007-04-13   splitted from nathelper.c (ancuta)
*/


#ifndef _NATHELPER_NATHELPER_H
#define _NATHELPER_NATHELPER_H

#include "../../str.h"

/* Handy macros */
#define STR2IOVEC(sx, ix)       do {(ix).iov_base = (sx).s; (ix).iov_len = (sx).len;} while(0)
#define SZ2IOVEC(sx, ix)        do {(ix).iov_base = (sx); (ix).iov_len = strlen(sx);} while(0)

#endif
