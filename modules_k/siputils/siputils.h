/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _SIPUTILS_H_
#define _SIPUTILS_H_

/*! Siputils module API */
typedef struct siputils_api {
	int_str rpid_avp;      /*!< Name of AVP containing Remote-Party-ID */
	int     rpid_avp_type; /*!< type of the RPID AVP */
} siputils_api_t;

typedef int (*bind_siputils_t)(siputils_api_t* api);

/*!
 * \brief Bind function for the SIPUtils API
 * \param api binded API
 * \return 0 on success, -1 on failure
 */
int bind_siputils(siputils_api_t* api);

#endif
