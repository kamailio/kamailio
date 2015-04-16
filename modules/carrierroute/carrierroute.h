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
 */

/**
 * \file carrierroute.h
 * \brief Some globals.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CARRIERROUTE_H
#define CARRIERROUTE_H

#include "../../str.h"
#include "../../usr_avp.h"

#define DICE_MAX 1000

#define SUBSCRIBER_COLUMN_NUM 3
#define SUBSCRIBER_USERNAME_COL 0
#define SUBSCRIBER_DOMAIN_COL   1
#define SUBSCRIBER_CARRIER_COL  2

#define CARRIERROUTE_MODE_DB 1
#define CARRIERROUTE_MODE_FILE 2

extern str subscriber_table;
extern str * subscriber_columns[];
extern char * config_source;
extern char * config_file;
extern str default_tree;

extern const str CR_EMPTY_PREFIX;

extern int mode;
extern int cr_match_mode;
extern int cr_avoid_failed_dests;

extern int_str cr_uris_avp;

#endif
