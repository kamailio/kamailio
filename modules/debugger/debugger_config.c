/*
 * $Id$
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

/*!
 * \file
 * \brief Debugger :: Configuration
 * \ingroup debugger
 */


#include "../../cfg/cfg.h"

#include "debugger_config.h"

struct cfg_group_dbg	default_dbg_cfg = {
	0, /* level_mode */
	0  /* hash_size */
};

void *dbg_cfg = &default_dbg_cfg;

cfg_def_t dbg_cfg_def[] = {
	{"mod_level_mode", CFG_VAR_INT|CFG_ATOMIC, 0, 1,
		dbg_level_mode_fixup, 0,
		"Enable or disable per module log level (0 - disabled, 1 - enabled)"},
	{"mod_hash_size", CFG_VAR_INT|CFG_READONLY, 0, 0,
		0, 0,
		"power of two as size of internal hash table to store levels per module"},
	{0, 0, 0, 0, 0, 0}
};
