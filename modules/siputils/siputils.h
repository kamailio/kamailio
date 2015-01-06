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


#ifndef _SIPUTILS_H_
#define _SIPUTILS_H_

typedef int (*siputils_has_totag_t)(struct sip_msg*, char*, char*);
typedef int (*siputils_is_uri_user_e164_t)(str*);

/*! Siputils module API */
typedef struct siputils_api {
	int_str rpid_avp;      /*!< Name of AVP containing Remote-Party-ID */
	int     rpid_avp_type; /*!< type of the RPID AVP */
	siputils_has_totag_t has_totag;
	siputils_is_uri_user_e164_t is_uri_user_e164;
} siputils_api_t;

typedef int (*bind_siputils_t)(siputils_api_t* api);

/*!
 * \brief Bind function for the SIPUtils API
 * \param api binded API
 * \return 0 on success, -1 on failure
 */
int bind_siputils(siputils_api_t* api);

inline static int siputils_load_api(siputils_api_t *pxb)
{
        bind_siputils_t bind_siputils_exports;
        if (!(bind_siputils_exports = (bind_siputils_t)find_export("bind_siputils", 1, 0)))
        {
                LM_ERR("Failed to import bind_siputils\n");
                return -1;
        }
        return bind_siputils_exports(pxb);
}

#endif
