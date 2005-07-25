/* $Id$*
 *
 * mem (malloc) info 
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * History:
 * --------
 *  2005-03-02  created (andrei)
 *  2005-07-25  renamed meminfo to mem_info due to name conflict on solaris
 */

#ifndef meminfo_h
#define meminfo_h

struct mem_info{
	unsigned long total_size;
	unsigned long free;
	unsigned long used;
	unsigned long real_used; /*used + overhead*/
	unsigned long max_used;
	unsigned long min_frag;
	unsigned long total_frags; /* total fragment no */
};

#endif

