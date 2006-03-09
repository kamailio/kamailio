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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 *  atomic operations init
 */
/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 */

#include "atomic_ops.h"

#ifdef ATOMIC_USE_LOCK
gen_lock_t* atomic_lock;
#endif


/* returns 0 on success, -1 on error */
int atomic_ops_init()
{
	int ret;
	
	ret=0;
#ifdef ATOMIC_USE_LOCK
	if ((atomic_lock=lock_alloc())==0){
		ret=-1;
		goto end;
	}
	if (lock_init(atomic_lock)==0){
		ret=-1;
		lock_destroy(atomic_lock);
		atomic_lock=0;
		goto end;
	}
end:
#endif
	return ret;
}

