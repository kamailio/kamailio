/*
 * $Id: mi_datagram_parser.h 1133 2007-04-02 17:31:13Z ancuta_onofrei $
 *
 * Copyright (C) 2007 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 * History:
 * ---------
 *  2007-06-25  first version (ancuta)
 */


#ifndef _MI_DATAGRAM_PARSER_H_
#define _MI_DATAGRAM_PARSER_H_

#include <stdio.h>

#define DATAGRAM_SOCK_BUF_SIZE 65457

int  mi_datagram_parser_init( unsigned int size );

struct mi_root * mi_datagram_parse_tree(datagram_stream * data);

#endif /* _MI_DATAGRAM_PARSER_H_ */
