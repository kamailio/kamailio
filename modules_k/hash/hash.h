/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef HASH__SP_HASH_H
#define HASH__SP_HASH_H 1

#include "../../parser/msg_parser.h"

enum hash_source {
	shs_call_id = 1,
	shs_from_uri,
	shs_from_user,
	shs_to_uri,
	shs_to_user,
	shs_error
};

typedef int (*hash_func_t)(struct sip_msg * msg,
	enum hash_source source, int denominator);

#endif
