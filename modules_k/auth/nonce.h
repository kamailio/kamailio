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
 * \brief Nonce related functions
 * \ingroup auth
 * - Module: \ref auth
 */

#ifndef NONCE_H
#define NONCE_H

#include "../../str.h"
#include <time.h>


/*! Length of nonce string in bytes */
#define NONCE_LEN (16+32)


/*!
 * \brief Calculate nonce value
 *
 * Calculate nonce value value. The nonce value consists of the
 * expires time (in seconds since 1.1 1970) and a secret phrase.
 * \param _nonce nonce value
 * \param _expires expires value
 * \param _index nonce index
 * \param _secret secret phrase
 */
void calc_nonce(char* _nonce, int _expires, int _index, str* _secret);


/*!
 * \brief Check nonce value received from user agent
 * \param _nonce nonce value
 * \param _secret secret phrase
 * \return 0 when nonce is valid, -1 on errors, positive if nonce not valid
 */
int check_nonce(str* _nonce, str* _secret);


/*!
 * \brief Get expiry time from nonce string
 * \param _nonce nonce string
 * \return expiry time
 */
time_t get_nonce_expires(str* _nonce);


/*!
 * \brief Get index from nonce string
 * \param _nonce nonce string
 * \return nonce index
 */
int get_nonce_index(str* _nonce);

/*!
 * \brief Check if a nonce is stale
 * \param _n nonce string
 * \return 1 if the nonce is stale, 0 otherwise
 */
int is_nonce_stale(str* _nonce);

#endif
