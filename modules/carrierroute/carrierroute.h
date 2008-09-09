/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/**
 * \file carrierroute.h
 * \brief Contains the functions exported by the module.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef SP_ROUTE_H
#define SP_ROUTE_H

#include "../../str.h"
#include "../../usr_avp.h"
#include "../../pvar.h"

#define SIP_URI "sip:"
#define SIP_URI_LEN 4
#define SIPS_URI "sips:"
#define SIPS_URI_LEN 5
#define AT_SIGN "@"
#define AT_SIGN_LEN 1

#define DICE_MAX 1000

#define SUBSCRIBER_COLUMN_NUM 3
#define SUBSCRIBER_USERNAME_COL 0
#define SUBSCRIBER_DOMAIN_COL   1
#define SUBSCRIBER_CARRIER_COL  2

#define SP_ROUTE_MODE_DB 1
#define SP_ROUTE_MODE_FILE 2

extern str subscriber_table;
extern str * subscriber_columns[];
extern char * config_source;
extern char * config_file;
extern str default_tree;

extern const str SP_EMPTY_PREFIX;

extern int mode;
extern int use_domain;
extern int fallback_default;

enum hash_algorithm {
	alg_crc32 = 1, /*!< hashing algorithm is CRC32 */
	alg_prime, /*!< hashing algorithm is (right 18 digits of hash_source % prime_number) % max_targets + 1 */
	alg_error
};

/**
 * Generic parameter that holds a string, an int or an pseudo-variable
 * @todo replace this with gparam_t
 */
struct multiparam_t {
	enum {
		MP_INT,
		MP_STR,
		MP_AVP,
		MP_PVE,
	} type;
	union {
		int n;
		str s;
		struct {
			unsigned short flags;
			int_str name;
		} a;
		pv_elem_t *p;
	} u;
};

#endif
