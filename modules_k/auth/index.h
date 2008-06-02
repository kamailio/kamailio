/*
 * $Id:$
 *
 * Nonce index  related functions
 *
 * Copyright (C)2008  Voice System S.R.L
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2008-05-29  initial version (anca)
*/

#ifndef _NONCE_INDEX_H_
#define _NONCE_INDEX_H_

/*
 * Get valid index for nonce
 */
int reserve_nonce_index(void);

/*
 * Check index validity
 */
int is_nonce_index_valid(int index);

#endif
