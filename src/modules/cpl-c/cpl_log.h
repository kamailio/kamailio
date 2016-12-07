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
 * 2003-09-22: created (bogdan)
 *
 */


#ifndef _CPL_LOG_H_
#define _CPL_LOG_H_

#include <stdarg.h>
#include "../../str.h"


#define MAX_LOG_NR    64

#define MSG_ERR     "Error: "
#define MSG_ERR_LEN (sizeof(MSG_ERR)-1)
#define MSG_WARN    "Warning: "
#define MSG_WARN_LEN (sizeof(MSG_WARN)-1)
#define MSG_NOTE     "Notice: "
#define MSG_NOTE_LEN (sizeof(MSG_NOTE)-1)

#define LF       "\n"
#define LF_LEN   (1)


void reset_logs(void);

void append_log( int nr, ...);

void compile_logs( str *log);

#endif

