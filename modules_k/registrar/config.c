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
 * History
 * -------
 *  2008-02-05	adapting tm module for the configuration framework (Miklos)
 */

/*!
 * \file 
 * \brief Registrar :: Configuration
 * \ingroup Registrar
 */


#include "../../cfg/cfg.h"
#include "../../parser/msg_parser.h" /* method types */

#include "config.h"

struct cfg_group_registrar	default_registrar_cfg = {
		3600, 	/* default_expires */
		60,	/* min_expires */
		0,	/* max_expires */
		0,	/* max_contacts */
		0,	/* retry_after */
		0	/* case_sensitive */
	};

void	*registrar_cfg = &default_registrar_cfg;

cfg_def_t	registrar_cfg_def[] = {
	{"default_expires",	CFG_VAR_INT | CFG_ATOMIC,	0, 0, 0, 0,
		"Contains number of second to expire if no expire hf or contact expire present" },
	{"min_expires",		CFG_VAR_INT | CFG_ATOMIC, 	0, 0, 0, 0,
		"The minimum expires value of a Contact. Value 0 disables the checking. "},
	{"max_expires",		CFG_VAR_INT | CFG_ATOMIC, 	0, 0, 0, 0,
		"The maximum expires value of a Contact. Value 0 disables the checking. "},
	{"max_contacts",	CFG_VAR_INT | CFG_ATOMIC, 	0, 0, 0, 0,
		"The maximum number of Contacts for an AOR. Value 0 disables the checking. "},
	{"retry_after",		CFG_VAR_INT | CFG_ATOMIC, 	0, 0, 0, 0,
		"If you want to add the Retry-After header field in 5xx replies, set this parameter to a value grater than zero"},
	{"case_sensitive",	CFG_VAR_INT | CFG_ATOMIC,	0, 0, 0, 0,
		"If set to 1 then AOR comparison will be case sensitive. Recommended and default is 0, case insensitive"},
	{0, 0, 0, 0, 0, 0}
};
