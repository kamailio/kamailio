/*
 * Header file for hash table functions
 *
 * Copyright (C) 2008 Juha Heinanen
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
 * \brief SIP-router lcr :: Header file for hash table functions
 * \ingroup lcr
 * Module: \ref lcr
 */

#ifndef _LCR_HASH_H_
#define _LCR_HASH_H_

#include "lcr_mod.h"

int lcr_hash_table_insert(struct lcr_info **hash_table,
			  unsigned short prefix_len, char *prefix,
			  unsigned short from_uri_len, char *from_uri,
			  pcre *from_uri_re, unsigned int grp_id,
			  unsigned short first_gw, unsigned short priority);

struct lcr_info *lcr_hash_table_lookup(struct lcr_info **hash_table,
				       unsigned short prefix_len, char *prefix);

void lcr_hash_table_contents_free(struct lcr_info **hash_table);

#endif
