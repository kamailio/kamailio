/*
 * Copyright (C) 2007 voice-system.ro
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
 */
/*!
 * \file
 * \brief Common string handling functions
 * \ingroup libkcore
 */

#ifndef _STRCOMMON_H_
#define _STRCOMMON_H_

#include "../../str.h"

/*
 * add backslashes to special characters
 */
int escape_common(char *dst, char *src, int src_len);
/*
 * remove backslashes to special characters
 */
int unescape_common(char *dst, char *src, int src_len);

int escape_user(str *sin, str *sout);

int unescape_user(str *sin, str *sout);

int escape_param(str *sin, str *sout);

int unescape_param(str *sin, str *sout);

#endif

