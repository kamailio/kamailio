/*
 * Path handling for intermediate proxies
 *
 * Copyright (C) 2006 Inode GmbH (Andreas Granig <andreas.granig@inode.info>)
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
 *
 */

/*! \file
 * \brief Path :: Module Core
 *
 * \ingroup path
 * - Module: path
 */


#ifndef PATH_MOD_H
#define PATH_MOD_H

#include "../outbound/api.h"

extern int path_use_received;
extern int path_received_format;
extern int path_enable_r2;
extern ob_api_t path_obb;

#endif /* PATH_MOD_H */
