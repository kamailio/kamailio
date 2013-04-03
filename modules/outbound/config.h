/*
 * $Id$
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
 *
 */

/*!
 * \file 
 * \brief Outbound :: Configuration Framework support
 * \ingroup outbound
 */


#ifndef _OUTBOUND_CONFIG_H
#define _OUTBOUND_CONFIG_H

#include "../../cfg/cfg.h"

struct cfg_group_outbound {
	int outbound_active;
};

extern struct cfg_group_outbound default_outbound_cfg;
extern void *outbound_cfg;
extern cfg_def_t outbound_cfg_def[];

#endif /* _OUTBOUND_CONFIG_H */
