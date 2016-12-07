/*
 * $Id$
 *
 * Copyright (C) 2013 Crocodile RCS Ltd
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
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */
#ifndef CHECKS_H
#define CHECKS_H

#include "../../parser/msg_parser.h"

int autheph_check_from0(struct sip_msg *_m);
int autheph_check_from1(struct sip_msg *_m, char *_username);
int autheph_check_to0(struct sip_msg *_m);
int autheph_check_to1(struct sip_msg *_m, char *_username);
int autheph_check_timestamp(struct sip_msg *_m, char *_username);

typedef enum {
	CHECK_NO_USER	= -2,
	CHECK_ERROR	= -1,
	CHECK_OK	= 1
} autheph_check_result_t;

#endif /* CHECKS_H */
