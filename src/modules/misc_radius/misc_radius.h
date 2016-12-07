/*
 * misc_radius.h
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

#ifndef _MISC_RADIUS_H_
#define _MISC_RADIUS_H_

/* Static attribute indexes */
enum {SA_SERVICE_TYPE = 0, SA_USER_NAME, SA_SIP_AVP, SA_SIP_GROUP,
      SA_SIP_URI_HOST, SA_STATIC_MAX};

/* Caller and callee value indexes */
enum {RV_SIP_CALLER_AVPS = 0, RV_STATIC_MAX};
enum {EV_SIP_CALLEE_AVPS = 0, EV_STATIC_MAX};
enum {GV_GROUP_CHECK = 0, GV_STATIC_MAX};
enum {UV_CALL_CHECK = 0, UV_STATIC_MAX};

extern int use_sip_uri_host;
extern int common_response;

extern void *rh;

extern struct attr caller_attrs[];
extern struct val caller_vals[];
extern struct attr callee_attrs[];
extern struct val callee_vals[];
extern struct attr group_attrs[];
extern struct val group_vals[];
extern struct attr uri_attrs[];
extern struct val uri_vals[];

extern struct extra_attr *caller_extra;
extern struct extra_attr *callee_extra;
extern struct extra_attr *group_extra;
extern struct extra_attr *uri_extra;

#endif
