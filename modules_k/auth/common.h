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
 * \brief Digest Authentication Module
 * \ingroup auth
 * - Module: \ref auth
 */

#ifndef COMMON_H
#define COMMON_H

#include "../../parser/msg_parser.h"

#define MESSAGE_400 "Bad Request"
#define MESSAGE_500 "Server Internal Error"


/*!
 * \brief Return parsed To or From, host part of the parsed uri is realm
 * \param _m SIP message
 * \param _hftype header field type
 * \param _u SIP URI
 * \return 0 on success, negative on failure
 */
int get_realm(struct sip_msg* _m, hdr_types_t _hftype, struct sip_uri** _u);


/*!
 * \brief Create a response with given code and reason phrase
 *
 * Create a response with given code and reason phrase
 * Optionally add new headers specified in _hdr
 * \param _m SIP message
 * \param _code response code
 * \param _reason reason string
 * \param _hdr header to add
 * \param _hdr_len header length
 * \return 1 if reply could be sended, -1 on failure
 */
int send_resp(struct sip_msg* _m, int _code, str* _reason,
	char* _hdr, int _hdr_len);

#endif
