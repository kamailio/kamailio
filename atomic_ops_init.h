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
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief SIP-router core :: atomic_ops init functions
 *
 * \ingroup core
 * Module: \ref core
 *
 * Needed for lock intializing if no native asm locks are available
 *  for the current arch./compiler combination, see \ref atomic_ops.c
 */
/* 
 * History:
 * --------
 *  2006-03-30  created by andrei
 */

#ifndef __atomic_ops_init_h
#define __atomic_ops_init_h

/*! \brief init atomic ops */
int init_atomic_ops(void);
/*! \brief destroy atomic ops (e.g. frees the locks, if locks are used) */
void destroy_atomic_ops(void);

#endif
