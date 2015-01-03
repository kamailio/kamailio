/*
 * Copyright (C) 2006 Voice Sistem SRL
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

/*! \file
 *  \brief USRLOC - Usrloc MI functions
 *  \ingroup usrloc
 */


#ifndef _USRLOC_MI_H_
#define _USRLOC_MI_H_

#include "../../lib/kmi/mi.h"

#define MI_USRLOC_RM           "ul_rm"
#define MI_USRLOC_RM_CONTACT   "ul_rm_contact"
#define MI_USRLOC_DUMP         "ul_dump"
#define MI_USRLOC_FLUSH        "ul_flush"
#define MI_USRLOC_ADD          "ul_add"
#define MI_USRLOC_SHOW_CONTACT "ul_show_contact"


/*!
 * \brief Delete a address of record including its contacts
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \note expects 2 nodes: the table name and the AOR
 * \return mi_root with the result
 */
struct mi_root* mi_usrloc_rm_aor(struct mi_root *cmd, void *param);


/*!
 * \brief Delete a contact from an AOR record
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \note expects 3 nodes: the table name, the AOR and contact
 * \return mi_root with the result or 0 on failure
 */
struct mi_root* mi_usrloc_rm_contact(struct mi_root *cmd, void *param);


/*!
 * \brief Dump the content of the usrloc
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \return mi_root with the result or 0 on failure
 */
struct mi_root* mi_usrloc_dump(struct mi_root *cmd, void *param);


/*!
 * \brief Flush the usrloc memory cache to DB
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \return mi_root with the result or 0 on failure
 */
struct mi_root* mi_usrloc_flush(struct mi_root *cmd, void *param);


/*!
 * \brief Add a new contact for an address of record
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \note Expects 7 nodes: table name, AOR, contact, expires, Q,
 * useless - backward compatible, flags, cflags, methods
 * \return mi_root with the result
 */
struct mi_root* mi_usrloc_add(struct mi_root *cmd, void *param);


/*!
 * \brief Dumps the contacts of an AOR
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \note expects 2 nodes: the table name and the AOR
 * \return mi_root with the result or 0 on failure
 */
struct mi_root* mi_usrloc_show_contact(struct mi_root *cmd, void *param);


#endif
