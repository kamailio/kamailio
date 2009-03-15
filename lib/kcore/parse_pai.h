/*
 *
 * Copyright (C) 2006 Juha Heinanen
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
 * \brief P-Asserted-Identity header parser
 * \ingroup parser
 */

#ifndef PARSE_PAI_H
#define PARSE_PAI_H

#include "../../msg_parser.h"
#include "../../parser/parse_to.h"


/*! casting macro for accessing P-Asserted-Identity body */
#define get_pai(p_msg)  ((struct to_body*)(p_msg)->pai->parsed)


/*!
 * P-Asserted-Identity header field parser
 */
int parse_pai_header( struct sip_msg *msg);

#endif /* PARSE_PAI_H */
