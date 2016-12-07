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

/**
 * \file
 * \brief Group membership module
 * \ingroup group
 * - Module: \ref group
 */

#ifndef GROUP_H
#define GROUP_H

#include "../../parser/msg_parser.h"
#include "../../pvar.h"
#include "../../usr_avp.h"


typedef struct _group_check
{
	int id;
	pv_spec_t sp;
} group_check_t, *group_check_p;


/*!
 * \brief Extract the username and domain from the SIP message
 *
 * Set the username and domain depending on the value of the SIP
 * message and the group check structure.
 * \param msg SIP message
 * \param gcp group check structure
 * \param username stored username
 * \param domain stored domain
 * \return 0 on success, -1 on failure
 */
int get_username_domain(struct sip_msg *msg, group_check_p gcp,
	str *username, str *domain);


/*!
 * \brief Check if username in specified header field is in a table
 * \param _msg SIP message
 * \param _hf Header field
 * \param _grp checked table
 * \return 1 on success, negative on failure 
 */
int is_user_in(struct sip_msg* _msg, char* _hf, char* _grp);


/*!
 * \brief Initialize the DB connection
 * \param db_url database URL
 * \return 0 on success, -1 on failure
 */
int group_db_init(const str* db_url);


/*!
 * \brief Bind the DB connection
 * \param db_url database URL
 * \return 0 on success, -1 on failure
 */
int group_db_bind(const str* db_url);


/*!
 * \brief Close the DB connection
 */
void group_db_close(void);

#endif
