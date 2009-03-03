/*
 * $Id$
 *
 * Copyright (C)2008  Voice System S.R.L
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
 *
 * History:
 * --------
 *  2008-05-29  initial version (anca)
*/

/*!
 * \file
 * \brief Nonce index related functions
 * \ingroup auth
 * - Module: \ref auth
 */

#ifndef _NONCE_INDEX_H_
#define _NONCE_INDEX_H_

/*!
 * \brief Get valid index for nonce
 * \return index on success, -1 on failure
 */
int reserve_nonce_index(void);


/*!
 * \brief Check if the nonce has been used before
 * \param index index
 * \return 1 if nonce is valid, 0 if not valid or on errors
 */
int is_nonce_index_valid(int index);

#endif
