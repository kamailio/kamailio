/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file cr_config.h
 * \brief Functions for load and save routing data from a config file.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CR_CONFIG_H
#define CR_CONFIG_H

#include "cr_data.h"


/**
 * Loads the routing data from the config file given in global
 * variable config_data and stores it in routing tree rd.
 *
 * @param rd Pointer to the route data tree where the routing data
 * shall be loaded into
 *
 * @return 0 means ok, -1 means an error occured
 *
 */
int load_config(struct route_data_t * rd);


/**
 * Stores the routing data rd in config_file
 *
 * @param rd Pointer to the routing tree which shall be saved to file
 *
 * @return 0 means ok, -1 means an error occured
 *
 */
int save_config(struct route_data_t * rd);

#endif
