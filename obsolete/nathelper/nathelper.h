/*
 * $Id$
 *
 * Copyright (C) 2005-2008 Sippy Software, Inc., http://www.sippysoft.com
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _NATHELPER_H
#define _NATHELPER_H

/* Parameters from nathelper.c */
extern struct socket_info* force_socket;

/* Functions from natping.c */
int natpinger_init(void);
int natpinger_child_init(int);
int natpinger_cleanup(void);

int natping_contact(str, struct dest_info *);

int intercept_ping_reply(struct sip_msg* msg);

/* Variables from natping.c referenced from nathelper.c */
extern int natping_interval;
extern int ping_nated_only;
extern char *natping_method;
extern int natping_stateful;
extern int natping_crlf;

#endif
