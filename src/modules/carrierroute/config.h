/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file 
 * \brief Carrierroute :: Configuration
 * \ingroup carrierroute
 */


#ifndef _CARRIERROUTE_CONFIG_H
#define _CARRIERROUTE_CONFIG_H


#include "../../core/cfg/cfg.h"
#include "../../core/str.h"

struct cfg_group_carrierroute
{
	unsigned int use_domain;
	unsigned int fallback_default;
	unsigned int fetch_rows;
};

extern struct cfg_group_carrierroute default_carrierroute_cfg;
extern void *carrierroute_cfg;
extern cfg_def_t carrierroute_cfg_def[];


#endif
