/*
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

#ifndef _UTILS_H_
#define _UTILS_H_


int checkerr(dvoid *errhp, sword status, int line);
#define OCICHECK(_errhp, _status) checkerr(_errhp, _status, __LINE__)

const char *sqlt_to_str(ub2 sqlt);

int parse_sql_url(const char *sqlurl, char *sz_user, char *sz_passwd,
                  char *sz_db);

#endif /* _UTILS_H_ */
