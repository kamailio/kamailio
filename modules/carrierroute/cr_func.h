/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * \file cr_func.h
 * \brief Routing and balancing functions.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CR_FUNC_H
#define CR_FUNC_H

#include "../../parser/msg_parser.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "prime_hash.h"


/**
 * Loads user carrier from subscriber table and stores it in an AVP.
 *
 * @param _msg the current SIP message
 * @param _user the user to determine the carrier data
 * @param _domain the domain to determine the domain data
 * @param _dstavp the name of the AVP where to store the carrier id
 *
 * @return 1 on success, -1 on failure
 */
int cr_load_user_carrier(struct sip_msg * _msg, gparam_t *_user,
		gparam_t *_domain, gparam_t *_dstavp);


/**
 * rewrites the request URI of msg after determining the
 * new destination URI
 *
 * @param _msg the current SIP message
 * @param _carrier the requested carrier
 * @param _domain the requested routing domain
 * @param _prefix_matching the user to be used for prefix matching
 * @param _rewrite_user the localpart of the URI to be rewritten
 * @param _hsrc the SIP header used for hashing
 * @param _descavp the name of the AVP where the description is stored
 *
 * @return 1 on success, -1 on failure
 */
int cr_route(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_rewrite_user, enum hash_source _hsrc,
		gparam_t *_descavp);
int cr_route5(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_rewrite_user, enum hash_source _hsrc);


/**
 *
 * rewrites the request URI of msg after determining the
 * new destination URI with the crc32 hash algorithm. The difference
 * to cr_route is that no fallback rule is chosen if there is something
 * wrong (behaves like now obselete cr_prime_route)
 *
 * @param _msg the current SIP message
 * @param _carrier the requested carrier
 * @param _domain the requested routing domain
 * @param _prefix_matching the user to be used for prefix matching
 * @param _rewrite_user the localpart of the URI to be rewritten
 * @param _hsrc the SIP header used for hashing
 * @param _dstavp the name of the destination AVP where the used host name is stored
 *
 * @return 1 on success, -1 on failure
 */
int cr_nofallback_route(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_rewrite_user, enum hash_source _hsrc,
		gparam_t *_dstavp);
int cr_nofallback_route5(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching,
		gparam_t *_rewrite_user, enum hash_source _hsrc);


/**
 * Loads next domain from failure routing table and stores it in an AVP.
 *
 * @param _msg the current SIP message
 * @param _carrier the requested carrier
 * @param _domain the requested routing domain
 * @param _prefix_matching the user to be used for prefix matching
 * @param _host the host name to be used for rule matching
 * @param _reply_code the reply code to be used for rule matching
 * @param _dstavp the name of the destination AVP
 *
 * @return 1 on success, -1 on failure
 */
int cr_load_next_domain(struct sip_msg * _msg, gparam_t *_carrier,
		gparam_t *_domain, gparam_t *_prefix_matching, gparam_t *_host,
		gparam_t *_reply_code, gparam_t *_dstavp);

#endif
