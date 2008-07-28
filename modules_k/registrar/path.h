/*
 * $Id$
 *
 * Helper functions for Path support.
 *
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
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
 * \brief SIP registrar module - helper functions for Path support
 * \ingroup registrar   
 */  

#ifndef REG_PATH_H
#define REG_PATH_H

#include "../../parser/msg_parser.h"

/*! \brief
 * Extracts all Path header bodies into one string and
 * checks if first hop is a loose router. It also extracts
 * the received-param of the first hop if path_use_received is 1.
 */
int build_path_vector(struct sip_msg *_m, str *path, str *received);

#endif /* REG_PATH_H */
