/*
 * Copyright (C) 2013 Konstantin Mosesov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __JAVA_IFACE_H__
#define	__JAVA_IFACE_H__

#include "../../parser/msg_parser.h"

int j_nst_exec_0(struct sip_msg *, char *, char *);
int j_nst_exec_1(struct sip_msg *, char *, char *, char *);
int j_s_nst_exec_0(struct sip_msg *, char *, char *);
int j_s_nst_exec_1(struct sip_msg *, char *, char *, char *);

int j_st_exec_0(struct sip_msg *, char *, char *);
int j_st_exec_1(struct sip_msg *, char *, char *, char *);
int j_s_st_exec_0(struct sip_msg *, char *, char *);
int j_s_st_exec_1(struct sip_msg *, char *, char *, char *);

int java_exec(struct sip_msg *, int, int, char *, char *, char *);



#endif
