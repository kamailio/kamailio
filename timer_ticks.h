/*
 * $Id$
 *
 *
 * timer frequency and ticks conversions
 *
 * Copyright (C) 2005 iptelorg GmbH
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
/* History:
 * --------
 *  2005-07-27  complete re-design/re-implemnetation (andrei)
 */

#ifndef _timer_ticks_h
#define _timer_ticks_h

/* how many ticks per second (must >1 and < 100 (on linux x86))
 * recomended values >=8, <=32 (a 2^k value is better/faster)*/
#define TIMER_TICKS_HZ	16U

/* how many ticks per m milliseconds? (rounded up) */
#define MS_TO_TICKS(m)  (((m)*TIMER_TICKS_HZ+999U)/1000U)


/* how many ticks per s seconds? */
#define S_TO_TICKS(s)	((s)*TIMER_TICKS_HZ)


/* how many s pe per t ticks, integer value */
#define TICKS_TO_S(t)	((t)/TIMER_TICKS_HZ)

/* how many ms per t ticks, integer value */
#define TICKS_TO_MS(t) (((t)*1000U)/TIMER_TICKS_HZ)


typedef unsigned int ticks_t;/* type used to keep the ticks (must be 32 bits)*/
typedef signed   int s_ticks_t; /* signed ticks type */

#endif
