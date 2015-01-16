/*
 * timer frequency and ticks conversions
 *
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/**
 * @file
 * @brief Kamailio core :: timer frequency and ticks conversions
 * @ingroup core
 * Module: @ref core
 */

#ifndef _timer_ticks_h
#define _timer_ticks_h

/** @brief how many ticks per second (must >1 and < 100 (on linux x86))
 * recomended values >=8, <=32 (a 2^k value is better/faster)*/
#define TIMER_TICKS_HZ	16U

/** @brief how many ticks per m milliseconds? (rounded up) */
#define MS_TO_TICKS(m)  (((m)*TIMER_TICKS_HZ+999U)/1000U)


/** @brief how many ticks per s seconds? */
#define S_TO_TICKS(s)	((s)*TIMER_TICKS_HZ)


/** @brief how many s pe per t ticks, integer value */
#define TICKS_TO_S(t)	((t)/TIMER_TICKS_HZ)

/** @brief how many ms per t ticks, integer value */
#define TICKS_TO_MS(t) (((t)*1000U)/TIMER_TICKS_HZ)


/** @brief ticks comparison operations: t1 OP t2, where OP can be <, >, <=, >= */
#define TICKS_CMP_OP(t1, t2, OP) \
	(((s_ticks_t)((ticks_t)(t1)-(ticks_t)(t2))) OP (s_ticks_t)0)
/** @brief t1 < t2 */
#define TICKS_LT(t1, t2)  TICKS_CMP_OP(t1, t2, <)
/** @brief t1 <= t2 */
#define TICKS_LE(t1, t2)  TICKS_CMP_OP(t1, t2, <=)
/** @brief t1 > t2 */
#define TICKS_GT(t1, t2)  TICKS_CMP_OP(t1, t2, >)
/** @brief t1 >= t2 */
#define TICKS_GE(t1, t2)  TICKS_CMP_OP(t1, t2, >=)


typedef unsigned int ticks_t;/* type used to keep the ticks (must be 32 bits)*/
typedef signed   int s_ticks_t; /* signed ticks type */

#endif
