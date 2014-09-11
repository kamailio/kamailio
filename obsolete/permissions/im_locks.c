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

#include "../../sched_yield.h"
#include "im_locks.h"

/* reader lock for ipmatch cache */
void reader_lock_imhash(void)
{

	/* prefer writers: suggest readers not to claim a reader ref_count if
	   a writer is interested in writing; the reader goes spinning, not too
	   bad, writes are rare (Jiri) */
	while (IM_HASH->writer_demand) sched_yield();

	/* it may be that reader tried to read at the same time when writer
	   decided to write -- make sure reader doesn't enter */
	while (1) {
		lock_get(&(IM_HASH->read_lock));
		if (IM_HASH->reader_count >= 0) {
			IM_HASH->reader_count++;
			lock_release(&(IM_HASH->read_lock));
			break;
		}
		/* reader_count < 0 ... writer is active; retry */
		lock_release(&(IM_HASH->read_lock));
		sched_yield();
	}
	
}

/* reader release for ipmatch cache */
void reader_release_imhash(void)
{
	lock_get(&(IM_HASH->read_lock));
	IM_HASH->reader_count--;
	lock_release(&(IM_HASH->read_lock));
}

/* set writer demand on ipmatch cache */
void set_wd_imhash(void)
{

	/* tell the readers to wait for a bit, while the new table becomes active */
	IM_HASH->writer_demand = 1; /* write_lock is already set */

	/* wait for the readers (Jiri) */
	while(1) {
		lock_get(&(IM_HASH->read_lock));
		if (IM_HASH->reader_count == 0) {
			IM_HASH->reader_count--;
			lock_release(&(IM_HASH->read_lock));
			break;
		}
		lock_release(&(IM_HASH->read_lock));
		/* processes still reading, retry ! */
		sched_yield();
	}
}

/* delete writer demand on ipmatch cache */
void del_wd_imhash(void) {

	lock_get(&(IM_HASH->read_lock));
	IM_HASH->reader_count++;
	lock_release(&(IM_HASH->read_lock));
	IM_HASH->writer_demand = 0;
	
}
