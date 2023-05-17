/*
 * CALL_OBJ module
 *
 * Copyright (C) 2017-2019 - Sonoc
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
 *
 */

/**
 * \file
 * \ingroup call_obj
 * \brief call_obj :: Module functionality
 *
 * - See \ref cobj.c
 * - Module: \ref call_obj
 */

/*
 * Header for functionality of Call Object module.
 */

#ifndef _CALL_OBJ_H_
#define _CALL_OBJ_H_

#include <stdint.h>

#include "../../core/str.h"

/**
 * \brief Initialize call object module.
 *
 * \return 0 on success.
 */
int cobj_init(int c_start, int c_end);

/**
 * \brief Close call object module.
 */
void cobj_destroy(void);

/**
 * \brief Get a free object.
 *
 * \param timestamp assign this timestamp to the object we get.
 * \param callid pointer to callid str.
 * \return -1 if an error happens.
 * \return number of a free object on success.
 */
int cobj_get(uint64_t timestamp, str *callid);

/**
 * \brief Free an Object
 *
 * \param num number of object to free
 * \return 0 on success
 */
int cobj_free(int num);

/**
 * \brief Structure to store module statistics.
 */
typedef struct
{
	int start;	  /**< First element in the array. */
	int end;	  /**< Last element in the array (included). */
	int assigned; /**< Number of currently assigned elements. */
} cobj_stats_t;

/**
 * \brief Fill data in cobj_stats_t structure passed as pointer.
 *
 * \param stats pointer to cobj_stats_t structure.
 * \return 0 on success
 */
int cobj_stats_get(cobj_stats_t *stats);

/**
 * \brief Free all objects at once.
 */
void cobj_free_all(void);

/**
 * \brief Element of a returned object list.
 */
typedef struct _cobj_elem
{
	int number;				 /**< Number assigned to the call. */
	uint64_t timestamp;		 /**< Timestamp for the call. */
	str callid;				 /**< Call-ID of the call. */
	struct _cobj_elem *next; /**< Next element in the list. */
} cobj_elem_t;

/**
 * \brief Free an object list.
 *
 * \param elem pointer to first element in the list.
 */
void cobj_free_list(cobj_elem_t *elem);

/**
 * \brief Get current timestamp in milliseconds.
 *
 * \param ts pointer to timestamp integer.
 * \return 0 on success.
 */
int get_timestamp(uint64_t *ts);

/**
 * \brief Get all objects which timestamp is less than or equals some value.
 *
 * User shall free returned list when not used any more.
 *
 * \param ts timestamp to compare.
 * \param elem returned list. NULL on error of if zero elements.
 * \param limit maximum number of objects to return. 0 means unlimited.
 *
 * \return number of returned objects on success.
 * \return -1 on error
 */
int cobj_get_timestamp(uint64_t ts, cobj_elem_t **elem, int limit);

#endif /* _CALL_OBJ_H_ */
