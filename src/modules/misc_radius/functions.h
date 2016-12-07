/*
 * functions.h
 *
 * Copyright (C) 2008 Juha Heinanen <jh@tutpro.com>
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

#ifndef _MISC_RADIUS_FUNCTIONS_H_
#define _MISC_RADIUS_FUNCTIONS_H_

extern int radius_load_caller_avps(struct sip_msg* _m, char* _caller,
				   char* _s2);

extern int radius_load_callee_avps(struct sip_msg* _m, char* _callee,
				   char* _s2);

int radius_is_user_in(struct sip_msg* _m, char* _user, char* _group);

int radius_does_uri_exist_0(struct sip_msg* _m, char* _s1, char* _s2);

int radius_does_uri_exist_1(struct sip_msg* _m, char* _sp, char* _s2);

int radius_does_uri_user_exist_0(struct sip_msg* _m, char* _s1, char* _s2);

int radius_does_uri_user_exist_1(struct sip_msg* _m, char* _sp, char* _s2);

#endif
