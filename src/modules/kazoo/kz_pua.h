/*
 * Kazoo module interface
 *
 * Copyright (C) 2010-2014 2600Hz
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

#ifndef _DBK_PUA_
#define _DBK_PUA_


int kz_initialize_pua();
int kz_pua_publish(struct sip_msg* msg, char *json);
int kz_pua_publish_mwi(struct sip_msg* msg, char *json);
int kz_pua_publish_presence(struct sip_msg* msg, char *json);
int kz_pua_publish_dialoginfo(struct sip_msg* msg, char *json);


#endif
