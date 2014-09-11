/*
 * Common definitions.
 *
 * Copyright (C) 2005 RingCentral Inc.
 * Created by Dmitry Semyonov <dsemyonov@ringcentral.com>
 *
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

#ifndef _COMMON_H_
#define _COMMON_H_

#define DB_DEBUG    1

#define DB_PREALLOC_SQLT_HANDLE 1

/*
 * Values are not included directly into the statements,
 * so we can safely use relatively small buffer size.
 */
#define SQL_STMT_BUFF_SZ    2048

/* Maximum supported number of columns in a table */
#define ORA_PARAMS_MAX  128
#define ORA_BINDS_MAX   ('z' - 'a' + 1)

#ifdef DB_DEBUG
#define DBG1(fmt, args...) LOG(L_DBG, fmt, ## args)
#endif

#warning "\
This module is experimental and may crash SER or create unexpected \
results. You use the module at your own risk. Please submit bugs at \
http://tracker.iptel.org"

#endif /* _COMMON_H_ */
