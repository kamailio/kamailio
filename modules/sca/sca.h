/*
 * Copyright (C) 2012 Andrew Mortensen
 *
 * This file is part of the sca module for Kamailio, a free SIP server.
 *
 * The sca module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The sca module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 */
#include "sca_common.h"

#include "sca_db.h"
#include "sca_hash.h"

#ifndef SCA_H
#define SCA_H

struct _sca_config {
    str		*outbound_proxy;
    str		*db_url;
    str		*subs_table;
    str		*state_table;
    int		db_update_interval;
    int		hash_table_size;
    int		call_info_max_expires;
    int		line_seize_max_expires;
    int		purge_expired_interval;
};
typedef struct _sca_config	sca_config;

struct _sca_mod {
    sca_config		*cfg;
    sca_hash_table	*subscriptions;
    sca_hash_table	*appearances;

    db_func_t		*db_api;
    struct tm_binds	*tm_api;
    sl_api_t		*sl_api;
};
typedef struct _sca_mod		sca_mod;

extern sca_mod		*sca;

#endif /* SCA_H */
