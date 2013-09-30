/*
 * $Id$
 *
 * Copyright (C) 2008 iptelorg GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _T_SUSPEND_H
#define _T_SUSPEND_H

#define LOCK_ASYNC_CONTINUE(_t) lock(&(_t)->async_mutex )
#define UNLOCK_ASYNC_CONTINUE(_t) unlock(&(_t)->async_mutex )

int t_suspend(struct sip_msg *msg,
		unsigned int *hash_index, unsigned int *label);
typedef int (*t_suspend_f)(struct sip_msg *msg,
		unsigned int *hash_index, unsigned int *label);

int t_continue(unsigned int hash_index, unsigned int label,
		struct action *route);
typedef int (*t_continue_f)(unsigned int hash_index, unsigned int label,
		struct action *route);

int t_cancel_suspend(unsigned int hash_index, unsigned int label);
typedef int (*t_cancel_suspend_f)(unsigned int hash_index, unsigned int label);


#endif /* _T_SUSPEND_H */
