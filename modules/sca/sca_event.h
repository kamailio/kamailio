/*
 * Copyright (C) 2012 Andrew Mortensen
 *
 * This file is part of the sca module for Kamailio, a free SIP server.
 *
 * The sca module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The sca module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 */
enum {
    SCA_EVENT_TYPE_UNKNOWN = -1,
    SCA_EVENT_TYPE_CALL_INFO = 1,
    SCA_EVENT_TYPE_LINE_SEIZE = 2,
};

extern str	SCA_EVENT_NAME_CALL_INFO;
extern str	SCA_EVENT_NAME_LINE_SEIZE;

#define sca_ok_status_for_event(e1) \
	(e1) == SCA_EVENT_TYPE_CALL_INFO ? 202 : 200
#define sca_ok_text_for_event(e1) \
	(e1) == SCA_EVENT_TYPE_CALL_INFO ? "Accepted" : "OK"

int		sca_event_from_str( str * );
char		*sca_event_name_from_type( int );
int		sca_event_append_header_for_type( int, char *, int );
