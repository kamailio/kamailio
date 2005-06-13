/* domain.h v 0.1 2003/1/20
 *
 * Header file for radius based checks
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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
 */


#ifndef CHECKS_H
#define CHECKS_H


#include "../../parser/msg_parser.h"


/*
 * Check from radius if Request URI exists
 */
int radius_does_uri_exist(struct sip_msg* _msg,  char* _str1, char* _str2);


#endif /* CHECKS_H */
