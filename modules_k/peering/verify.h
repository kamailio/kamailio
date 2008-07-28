/*
 * Verification functions .h file
 *
 * Copyright (C) 2008 Juha Heinanen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 *
 */

/*! \file
 * \ingroup peering
 * \brief Peering:: Verification functions
 *
 * - \ref verifiy.c
 * - Module: \ref peering
 */


#ifndef _PEERING_VERIFY_H_
#define _PEERING_VERIFY_H_

#include "../../parser/msg_parser.h"

int verify_destination(struct sip_msg* _msg, char* s1, char* s2);

int verify_source(struct sip_msg* _msg, char* s1, char* s2);

#endif /* _PEERING_VERIFY_H_ */
