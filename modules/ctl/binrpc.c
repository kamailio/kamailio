/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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
/* History:
 * --------
 *  2006-02-08  created by andrei
 */



#include "binrpc.h"

/* WARNING: keep in sync with the errors listed in binrpc.h */
static const char* binrpc_str_errors[]={
	"no error",
	"invalid function arguments",
	"buffer too small (overflow)",
	"corrupted packet",
	"more data needed",
	"end of packet encountered",
	"binrpc parse context not initialized",
	"record doesn't match type",
	"bad record",
	"bug -- internal error",
	"unknown/invalid error code"
};



const char* binrpc_error(int err)
{
	if (err<0) err=-err;
	if (err>(-E_BINRPC_LAST)) err=-E_BINRPC_LAST;
	return binrpc_str_errors[err];
}
