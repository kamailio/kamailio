/*
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2016 Travis Cross <tc@traviscross.com>
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
 * \brief Crypto :: Fast enough high entropy Call-ID generator
 * \ingroup tm
 */


#ifndef __CRYPTO_UUID_H__
#define __CRYPTO_UUID_H__

#include "../../core/str.h"


/**
 * \brief Initialize the Call-ID generator
 * \return 0 on success, -1 on error
 */
int crypto_init_callid(void);


/**
 * \brief Child initialization
 * \param rank not used
 * \return 0 on success, -1 on error
 */
int crypto_child_init_callid(int rank);


/**
 * \brief TM API export
 */
typedef void (*generate_callid_f)(str*);


/**
 * \brief Get a unique Call-ID
 * \param callid returned Call-ID
 */
void crypto_generate_callid(str* callid);

/**
 *
 */
int crypto_register_callid_func(void);


/**
 * \brief generate SHA1 hash over a given input string
 * \param str to apply hash over
 * \param SHA1 hash
 */
int crypto_generate_SHA1(str* in, str* hash);

#endif /* CALLID_H */
