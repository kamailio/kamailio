/*
 * Copyright (C) 2013-19 Robert Boisvert
 *
 * This file is part of the mohqueue module for Kamailio, a free SIP server.
 *
 * The mohqueue module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The mohqueue module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef MOHQ_COMMON_H
#define MOHQ_COMMON_H

#include <assert.h>
#include <sys/stat.h>

#include "../rr/api.h"

#if VERSIONVAL < 5000000
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../dprint.h"
#include "../../dset.h"
#include "../../flags.h"
#include "../../hashes.h"
#include "../../locking.h"
#include "../../lvalue.h"
#include "../../mod_fix.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"
#include "../../sr_module.h"
#include "../../str.h"

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#include "../../parser/hf.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../parser/sdp/sdp.h"
#else
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/dprint.h"
#include "../../core/dset.h"
#include "../../core/flags.h"
#include "../../core/hashes.h"
#include "../../core/locking.h"
#include "../../core/lvalue.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/hf.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_expires.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_rr.h"
#include "../../core/parser/sdp/sdp.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/sr_module.h"
#include "../../core/str.h"
#endif

#include "../../lib/srdb1/db.h"
#include "../../modules/sl/sl.h"
#include "../../modules/tm/tm_load.h"

/* convenience macros */
#define MOHQ_STRUCT_PTR_OFFSET( struct1, cast1, offset1 ) \
	(cast1)(struct1) + (offset1)

#define MOHQ_STR_COPY( str1, str2 ) \
	memcpy((str1)->s, (str2)->s, (str2)->len ); \
	(str1)->len = (str2)->len;

#define MOHQ_STR_APPEND( str1, str2 ) \
	memcpy((str1)->s + (str1)->len, (str2)->s, (str2)->len); \
	(str1)->len += (str2)->len;

#define MOHQ_STR_APPEND_L( str1, str1_lim, s2, s2_len ) \
	if ((str1)->len + (s2_len) >= (str1_lim)) { \
	    LM_ERR( "Failed to append to str: too long!\n" ); \
	} else { \
	    MOHQ_STR_APPEND((str1), (s2), (s2_len)); \
	    (str1_lim) -= (s2_len); \
	}

#define MOHQ_STR_COPY_CSTR( str1, cstr1 ) \
	memcpy((str1)->s + (str1)->len, (cstr1), strlen((cstr1))); \
	(str1)->len += strlen((cstr1));

#define MOHQ_STR_APPEND_CSTR( str1, cstr1 ) \
	MOHQ_STR_COPY_CSTR((str1), (cstr1))

#define MOHQ_STR_APPEND_CSTR_L( str1, str1_lim, cstr1 ) \
	if ((str1)->len + strlen(cstr1) >= (str1_lim)) { \
	    LM_ERR( "Failed to append to str: too long!\n" ); \
	} else { \
	    MOHQ_STR_APPEND_CSTR((str1), (cstr1)); \
	}

/* STR_EQ assumes we're not using str pointers, which is obnoxious */
#define MOHQ_STR_EQ( str1, str2 ) \
	(((str1)->len == (str2)->len) && \
		memcmp((str1)->s, (str2)->s, (str1)->len) == 0)

#define MOHQ_STR_EMPTY( str1 ) \
	(((str1) != NULL && ((str1)->s == NULL || (str1)->len <= 0 )) \
		|| (str1) == NULL )

#define MOHQ_HEADER_EMPTY( hdr1 ) \
	((hdr1) == NULL || MOHQ_STR_EMPTY( &(hdr1)->body ))

#endif /* MOHQ_COMMON_H */
