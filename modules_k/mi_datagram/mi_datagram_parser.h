/*
 * $Id: mi_datagram_parser.h 1133 2007-04-02 17:31:13Z ancuta_onofrei $
 *
 * Copyright (C) 2007 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
