/*
 * $Id$
 *
 * Digest Authentication - Radius support
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
 * History:
 * -------
 * 2003-03-09: Based on authorize.h from radius_auth (janakj)
 */

#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include "../../parser/msg_parser.h"


/*
 * Authorize using Proxy-Authorize header field (no URI user parameter given)
 */
int radius_proxy_authorize_1(struct sip_msg* _msg, char* _realm, char* _s2);


/*
 * Authorize using Proxy-Authorize header field (URI user parameter given)
 */
int radius_proxy_authorize_2(struct sip_msg* _msg, char* _realm, char* _uri_user);


/*
 * Authorize using WWW-Authorization header field (no URI user parameter given)
 */
int radius_www_authorize_1(struct sip_msg* _msg, char* _realm, char* _s2);

/*
 * Authorize using WWW-Authorization header field (URI user parameter given)
 */
int radius_www_authorize_2(struct sip_msg* _msg, char* _realm, char* _uri_user);


#endif /* AUTHORIZE_H */
