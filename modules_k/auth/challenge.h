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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file
 * \brief Challenge related functions
 * \ingroup auth
 * - Module: \ref auth
 */

#ifndef CHALLENGE_H
#define CHALLENGE_H

#include "../../parser/msg_parser.h"


/*!
 * \brief Challenge a user to send credentials using WWW-Authorize header field
 * \param _msg SIP message
 * \param _realm authentification realm
 * \param _qop qop value
 * \return 0 if challenge could be sended, -1 on failure
 */
int www_challenge(struct sip_msg* _msg, char* _realm, char* _str2);


/*!
 * \brief Challenge a user to send credentials using Proxy-Authorize header field
 * \param _msg SIP message
 * \param _realm authentification realm
 * \param _qop qop value
 * \return 0 if challenge could be sended, -1 on failure
 */
int proxy_challenge(struct sip_msg* _msg, char* _realm, char* _str2);


/*!
 * \brief Remove used credentials from a SIP message header
 * \param _m SIP message
 * \param _s1 unused
 * \param _s2 unused
 * \return 1 when credentials could be removed, -1 if not found or on failure
 */
int consume_credentials(struct sip_msg* _m, char* _s1, char* _s2);


#endif
