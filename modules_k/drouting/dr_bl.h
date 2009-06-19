/*
 * $Id: routing.c,v 1.4 2007/10/03 14:10:48 bogdan Exp $
 *
 * Copyright (C) 2009 Voice Sistem SRL
 *
 * This file is part of Open SIP Server (OpenSIPS).
 *
 * DROUTING OpenSIPS-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * DROUTING OpenSIPS-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-system.ro
 *
 * History:
 * ---------
 *  2009-01-19  first version (bogdan)
 */


#ifndef _DR_DR_BL_H
#define _DR_DR_BL_H

#include "../../sr_module.h"
#include "../../blacklists.h"
#include "prefix_tree.h"

#define MAX_TYPES_PER_BL 32

struct dr_bl {
	unsigned int no_types;
	unsigned int types[MAX_TYPES_PER_BL];
	struct bl_head *bl;
	struct dr_bl *next;
};

int set_dr_bl( modparam_t type, void* val);

int init_dr_bls(void);

void destroy_dr_bls(void);

int populate_dr_bls(pgw_addr_t *pgwa);

#endif
