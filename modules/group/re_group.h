/*
 * Copyright (C) 2005-2007 Voice Sistem SRL
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

#ifndef RE_GROUP_H
#define RE_GROUP_H

#include "../../str.h"
#include "../../parser/msg_parser.h"


/*!
 * \brief Load regular expression rules from a database
 * \param table DB table
 * \return 0 on success, -1 on failure
 */
int load_re(str *table);


/*!
 * \brief Get the user group and compare to the regexp list
 * \param req SIP message
 * \param user user string
 * \param avp AVP value
 * \return number of all matches (positive), -1 on errors or when not found
 */
int get_user_group(struct sip_msg *req, char *user, char *avp);

#endif
