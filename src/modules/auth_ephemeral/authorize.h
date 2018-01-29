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
#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include "../../core/str.h"
#include "../../core/parser/msg_parser.h"

int autheph_verify_timestamp(str *_username);

int autheph_check(struct sip_msg *_m, char *_realm, char *_p2);
int autheph_www(struct sip_msg *_m, char *_realm, char *_p2);
int autheph_www2(struct sip_msg *_m, char *_realm, char *_method);
int autheph_proxy(struct sip_msg *_m, char *_realm, char *_p2);
int autheph_authenticate(struct sip_msg *_m, char *_username, char *_password);

int ki_autheph_check(sip_msg_t *_m, str *srealm);
int ki_autheph_www(sip_msg_t *_m, str *srealm);
int ki_autheph_www_method(sip_msg_t *_m, str *srealm, str *smethod);
int ki_autheph_proxy(sip_msg_t *_m, str *srealm);
int ki_autheph_authenticate(sip_msg_t *_m, str *susername, str *spassword);

#endif /* AUTHORIZE_H */
