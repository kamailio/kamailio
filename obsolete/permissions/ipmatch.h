/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 *
 */

#ifndef _IPMATCH_H
#define _IPMATCH_H

/* initialize ipmatch table */
int init_ipmatch(void);

/* destroy function */
void clean_ipmatch(void);

/* wrapper functions for ipmatch */
int ipmatch_2(struct sip_msg *msg, char *str1, char *str2);
int ipmatch_1(struct sip_msg *msg, char *str1, char *str2);
int ipmatch_onsend(struct sip_msg *msg, char *str1, char *str2);

/* set IM_FILTER */
int ipmatch_filter(struct sip_msg *msg, char *str1, char *str2);

#endif /* _IPMATCH_H */
