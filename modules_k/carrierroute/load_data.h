/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
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

/**
 * @file load_data.h
 * @brief API to bind a data loading function.
 */

#ifndef SP_ROUTE_LOAD_DATA_H
#define SP_ROUTE_LOAD_DATA_H

#include "route_tree.h"

typedef int (*route_data_load_func_t)(struct rewrite_data * rd);

/**
 * Binds the loader function pointer api to the matching loader
 * functionm depending on source
 *
 * @param source the configuration data source, at the moment 
 * it can be db or file
 * @param api pointer to the api where the loader function is
 * bound to
 *
 * @return 0 means everything is ok, -1 means an error
 */
int bind_data_loader(const char * source, route_data_load_func_t * api);

int data_main_finalize(void);

int data_child_init(void);

void data_destroy(void);

#endif
