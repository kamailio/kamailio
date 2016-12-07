/*
 * domainpolicy.h v 0.1 2002/12/27
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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
 * \brief Header file for domainpolicy table relates functions
 */


#ifndef DOMAINPOLICY_H
#define DOMAINPOLICY_H
		

#include "../../parser/msg_parser.h"

/*! maximal length of a domain string */
#define MAX_DOMAIN_SIZE 512

/*! how many indirect DDDS naptr are we going to follow? */
#define MAX_DDDS_RECURSIONS 5


/*!
 * return codes of dp_can_connect
 * negative values means false, 0 stops execution
 * and discards the SIP request
 */
#define DP_DDDS_RET_DNSERROR -2
#define DP_DDDS_RET_NEGATIVE -1  
#define DP_DDDS_RET_POSITIVE 1
#define DP_DDDS_RET_NOTFOUND 2


/*!
 * \brief Check if host in Request URI has DP-DDDS NAPTRs and if we can connect to them
 * \param _msg SIP message
 * \param _s1 unused
 * \param _s2 unused
 * \return negative on failure, positive on success
 */
int dp_can_connect(struct sip_msg* _msg, char* _s1, char* _s2);


/*!
 * \brief Apply DP-DDDS policy to current SIP message
 *
 * Apply DP-DDDS policy to current SIP message. This means
 * build a new destination URI from the policy AVP and export it
 * as AVP. Then in kamailio.cfg this new target AVP can be pushed
 * into the destination URI $duri
 * \param _msg SIP message
 * \param _s1 unused
 * \param _s2 unused
 * \return negative on failure, positive on succes
 */
int dp_apply_policy(struct sip_msg* _msg, char* _s1, char* _s2);


/*!
 * \brief Bind the database interface
 * \param db_url database url
 * \return -1 on failure, 0 on success
 */
int domainpolicy_db_bind(const str* db_url);


/*!
 * \brief Initialize the database connection
 * \param db_url database url
 * \return -1 on failure, 0 on success
 */
int domainpolicy_db_init(const str* db_url);

/*!
 * \brief Close the database connection
 */
void domainpolicy_db_close(void);


/*!
 * \brief Check the database table version
 * \param db_url database URL
 * \param name table name
 * \return -1 on failure, positive database version on success
 */
int domainpolicy_db_ver(const str* db_url, const str* name);


#endif
