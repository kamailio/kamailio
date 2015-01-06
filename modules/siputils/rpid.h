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
 *
 */

/*!
 * \file
 * \brief Remote-Party-ID related functions
 * \ingroup auth
 * - Module: \ref auth
 */

#ifndef RPID_H
#define RPID_H

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../usr_avp.h"


/*!
 * \brief Parse and set the RPID AVP specs
 * \param rpid_avp_param RPID AVP parameter
 * \return 0 on success, -1 on failure
 */
int init_rpid_avp(char *rpid_avp_param);


/*!
 * \brief Gets the RPID avp specs
 * \param rpid_avp_p AVP name
 * \param rpid_avp_type_p AVP type
 */
void get_rpid_avp( int_str *rpid_avp_p, int *rpid_avp_type_p );


/*!
 * \brief Append RPID header field to the message
 * \param _m SIP message
 * \param _s1 unused
 * \param _s2 unused
 * \return 1 on success, -1 on failure
 */
int append_rpid_hf(struct sip_msg* _m, char* _s1, char* _s2);


/*!
 * \brief Append RPID header field to the message with parameters
 * \param _m SIP message
 * \param _prefix prefix
 * \param _suffix suffix
 * \return 1 on success, -1 on failure
 */
int append_rpid_hf_p(struct sip_msg* _m, char* _prefix, char* _suffix);


/*!
 * \brief Check if URI in RPID AVP contains an E164 user part
 * \param _m SIP message
 * \param _s1 unused
 * \param _s2 unused
 * \return 1 if the URI contains an E164 user part, -1 if not
 */
int is_rpid_user_e164(struct sip_msg* _m, char* _s1, char* _s2);


#endif
