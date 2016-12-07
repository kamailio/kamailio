/*
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
 */

/*!
 * \file
 * \brief Route & Record-Route module
 * \ingroup rr
 */

#ifndef RECORD_H
#define RECORD_H

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../pvar.h"

/*!
 * \brief Insert a new Record-Route header field with lr parameter
 *
 * Insert a new Record-Route header field with lr parameter, and also 2nd one if it is
 * enabled and the realm changed so the 2nd record-route header will be necessary.
 * \param _m SIP message
 * \param params RR parameter
 * \return 0 on success, negative on failure
 */
int record_route(struct sip_msg* _m, str* _param);


/*!
 * \brief Insert manually created Record-Route header
 *
 * Insert manually created Record-Route header, no checks, no restrictions,
 * always adds lr parameter, only fromtag is added automatically when requested.
 * Allocates new private memory for this.
 * \param _m SIP message
 * \param _data manually created RR header
 * \return 1 on success, negative on failure
 */
int record_route_preset(struct sip_msg* _m, str* _data);


/*!
 * \brief Insert manually created Record-Route header
 *
 * Insert manually created Record-Route header, no checks, no restrictions,
 * always adds lr parameter, fromtag is added automatically when requested,
 * Allows addition of rr parameters using add_rr_param.
 * Adds a 2nd header with the same details if the protocol changes and double
 * rr enabled
 * Allocates new private memory for this.
 * \param _m SIP message
 * \param _data manually created RR header
 * \return 1 on success, negative on failure
 */
int record_route_advertised_address(struct sip_msg* _m, str* _data);


/*!
 * \brief Appends a new Record-Route parameter
 * \param msg SIP message
 * \param rr_param RR parameter
 * \return 0 on success, -1 on failure
 */
int add_rr_param(struct sip_msg* msg, str* rr_param);

void init_custom_user(pv_spec_t *custom_user_avp);


#endif /* RECORD_H */
