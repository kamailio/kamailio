/* 
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __LOGGER_H
#define __LOGGER_H

#ifndef SER

#include <stdio.h>

#define ERROR_LOG(a,args...)		do{printf(a,##args);}while(0)
#define DEBUG_LOG(a,args...)		do{printf(a,##args);}while(0)
#define TRACE_LOG(a,args...)		do{printf(a,##args);}while(0)
#define WARN_LOG(a,args...)			do{printf(a,##args);}while(0)
#define FLUSH_LOG()					do{fflush(stdout);}while(0)

#else
/* TODO: logging for SER */

#include "dprint.h"

#define ERROR_LOG(a,args...)		ERR(a,##args)
#define DEBUG_LOG(a,args...)		DBG(a,##args)
/* #define TRACE_LOG(a,args...)		ERR(a,##args) */
#define TRACE_LOG(fmt, args...)     LOG(L_ERR, "TRACE: "  LOC_INFO fmt, ##args)
#define WARN_LOG(a,args...)			WARN(a,##args)
#define FLUSH_LOG()					do{}while(0)

#endif

#endif
