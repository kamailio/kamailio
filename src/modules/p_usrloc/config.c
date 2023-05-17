/*
 * $Id$
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!
 * \file 
 * \brief P_USRLOC :: Configuration
 * \ingroup usrloc
 */


//#include "../../core/parser/msg_parser.h" /* method types */

#include "config.h"

struct cfg_group_p_usrloc default_p_usrloc_cfg = {
		DEFAULT_EXPIRE,			/* expire_time */
		DEFAULT_ERR_THRESHOLD,	/* db_err_threshold */
		DEFAULT_FAILOVER_LEVEL, /* failover_level */
		0,						/* db_ops_ruid */
		1,						/* db_update_as_insert */
		CONTACT_ONLY,			/* matching_mode */
		0,						/* utc_timestamps */
};

void *p_usrloc_cfg = &default_p_usrloc_cfg;

cfg_def_t p_usrloc_cfg_def[] = {
		{"expire_time", CFG_VAR_INT | CFG_ATOMIC, 0, 0, 0, 0,
				"Contains number of second to expire if no expire hf or "
				"contact expire present"},
		{"db_err_threshold", CFG_VAR_INT | CFG_ATOMIC, 0, 100, 0, 0,
				" Specifies the error value on which a database shall be "
				"turned off. "},
		{"failover_level", CFG_VAR_INT | CFG_ATOMIC, 0, 0, 0, 0,
				"Specifies the manner a failover is done (1 = turn off, 2 = "
				"find a spare) "},
		{"db_ops_ruid", CFG_VAR_INT | CFG_ATOMIC, 0, 2, 0, 0,
				"Set this if you want to update / delete from DB using ruid "
				"value "},
		{"db_update_as_insert", CFG_VAR_INT | CFG_ATOMIC, 0, 1, 0, 0,
				"Set this parameter if you want to do INSERT DB operations "
				"instead of UPDATE DB operations. "},
		{"matching_mode", CFG_VAR_INT | CFG_ATOMIC, 0, 0, 0, 0,
				"Specified which contact maching algorithm to be used (0 - "
				"Contact only / 1 - Contact and Call-ID / 2 - Contact and "
				"Path)"},
		{"utc_timestamps", CFG_VAR_INT | CFG_ATOMIC, 0, 0, 0, 0,
				"Expires and last_modified timestamps in UTC time format"},
		{0, 0, 0, 0, 0, 0}};
