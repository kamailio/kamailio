/*
 * Header file for hash table functions
 *
 * Copyright (C) 2008-2012 Juha Heinanen
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

int rule_hash_table_insert(struct rule_info **hash_table,
			   unsigned int lcr_id, unsigned int rule_id,
			   unsigned short prefix_len, char *prefix,
			   unsigned short from_uri_len, char *from_uri,
			   pcre *from_uri_re, unsigned short request_uri_len,
			   char *request_uri, pcre *request_uri_re,
			   unsigned short stopper);

int rule_hash_table_insert_target(struct rule_info **hash_table,
				  struct gw_info *gws,
				  unsigned int rule_id, unsigned int gw_id,
				  unsigned int priority, unsigned int weight);

struct rule_info *rule_hash_table_lookup(struct rule_info **hash_table,
					 unsigned short prefix_len,
					 char *prefix);

void rule_hash_table_contents_free(struct rule_info **hash_table);

void rule_id_hash_table_contents_free();

#endif
