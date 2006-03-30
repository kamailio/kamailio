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
 *  memory barriers for sparc32 ( version < v 9))
 *  see atomic_ops.h for more details 
 *
 * Config defines: NOSMP
 */
/* 
 * History:
 * --------
 *  2006-03-28  created by andrei
 */


#ifndef _atomic_sparc_h
#define _atomic_sparc_h

#define HAVE_ASM_INLINE_MEMBAR


#warning "sparc32 atomic operations support not tested"

#ifdef NOSMP
#define membar() asm volatile ("" : : : "memory") /* gcc do not cache barrier*/
#define membar_read()  membar()
#define membar_write() membar()
#else /* SMP */
#define membar_write() asm volatile ("stbar \n\t" : : : "memory") 
#define membar() membar_write()
#define membar_read() asm volatile ("" : : : "memory") 
#endif /* NOSMP */



#endif
