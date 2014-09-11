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

#ifndef _IM_LOCKS_H
#define _IM_LOCKS_H

#include "../../locking.h"
#include "im_hash.h"

/* reader lock for ipmatch cache */
#define reader_init_imhash_lock()	lock_init(&(IM_HASH->read_lock))
void reader_lock_imhash(void);
void reader_release_imhash(void);

/* writer lock for ipmatch cache */
#define writer_init_imhash_lock()	lock_init(&(IM_HASH)->write_lock);
#define writer_lock_imhash()	lock_get(&(IM_HASH)->write_lock);
#define writer_release_imhash()	lock_release(&(IM_HASH)->write_lock);

/* set writer demand on ipmatch cache */
void set_wd_imhash(void);
/* delete writer demand on ipmatch cache */
void del_wd_imhash(void);

#endif	/* _IM_LOCKS_H */
