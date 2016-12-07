/*
 * Copyright (C) 2009, 2013 Henning Westerholt
 * Copyright (C) 2013 Charles Chance, sipcentric.com
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*!
 * \file
 * \brief memcached module
 */

#ifndef _MCD_VAR_H_
#define _MCD_VAR_H_

#include "../../pvar.h"


/*!
 * \brief Get a cached value from memcached
 * \param msg SIP message
 * \param param parameter
 * \param res result
 * \return null on success, negative on failure
 */
int pv_get_mcd_value(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);


/*!
 * \brief Set a value in the cache of memcached
 * \param msg SIP message
 * \param param parameter
 * \param op not used
 * \param val value
 * \return 0 on success, -1 on failure
 */
int pv_set_mcd_value(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val);


/*!
 * \brief Increment a key atomically in the cache
 * \param msg SIP message
 * \param param parameter
 * \param op not used
 * \param val value
 * \return 0 on success, -1 on failure
 */
int pv_inc_mcd_value(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val);


/*!
 * \brief Decrement a key atomically in the cache
 * \param msg SIP message
 * \param param parameter
 * \param op not used
 * \param val value
 * \return 0 on success, -1 on failure
 */
int pv_dec_mcd_value(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val);


/*!
 * \brief Set the expire value in the cache of memcached
 * \note The memcache library don't provide functions to change the expiration
 * time for a certain key after creation, so we need to do a get and set here.
 * \param msg SIP message
 * \param param parameter
 * \param op not used
 * \param val value
 * \return 0 on success, -1 on failure
 */
int pv_set_mcd_expire(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val);


/*!
 * \brief Parse the pseudo-variable specification parameter
 * \param sp pseudo-variable specification
 * \param in parameter string
 * \return 0 on success, -1 on failure
 */
int pv_parse_mcd_name(pv_spec_p sp, str *in);

#endif
