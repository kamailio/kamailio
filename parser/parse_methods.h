/*
 * $Id$
 *
 * Copyright (c) 2004 Juha Heinanen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef PARSE_METHODS_H
#define PARSE_METHODS_H

#include "../str.h"

/*
 * Methods found in methods Contact parameter or Allow header.
 */
enum method {
	METH_UNKNOWN = 1, 
	METH_ACK = 2, 
	METH_BYE = 4, 
	METH_CANCEL = 8,
	METH_INFO = 16, 
	METH_INVITE = 32, 
	METH_NOTIFY = 64,
	METH_OPTIONS = 128, 
	METH_PRACK = 256, 
	METH_REGISTER  = 512,
	METH_SUBSCRIBE = 1024, 
	METH_UPDATE = 2048, 
	METH_MESSAGE = 4096,
	METH_REFER = 8192
};


/* 
 * Parse comma separated list of methods pointed by _body and assign their
 * enum bits to _methods.  Returns 1 on success and 0 on failure.
 */
int parse_methods(str* _body, unsigned int* _methods);


#endif /* PARSE_METHODS_H */
