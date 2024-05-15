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
 * \brief PDB :: Configuration
 * \ingroup PDB
 */


#include "../../core/cfg/cfg.h"
#include "../../core/parser/msg_parser.h" /* method types */

#include "config.h"

struct cfg_group_pdb default_pdb_cfg = {
		50, /* default timeout in miliseconds */
};

void *pdb_cfg = &default_pdb_cfg;

cfg_def_t pdb_cfg_def[] = {
		{"timeout", CFG_VAR_INT | CFG_ATOMIC, 0, 0, 0, 0,
				"The time in miliseconds pdb_query() waits for and answer"},
		{0, 0, 0, 0, 0, 0}};
