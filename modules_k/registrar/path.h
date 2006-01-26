/*
 * $Id$
 *
 * Helper functions for Path support.
 *
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef REG_PATH_H
#define REG_PATH_H

#include "../../parser/msg_parser.h"

/*
 * Extracts all Path header bodies into one string and
 * checks if first hop is a loose router.
 */
int build_path_vector(struct sip_msg *_m, str *path);

/*
 * If Path is available, sets _dst to uri of first element .
 */
int get_path_dst_uri(str *_p, str **_dst);

#endif /* REG_PATH_H */
