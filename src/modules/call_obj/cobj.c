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
 * \brief call_obj :: Module functionality.
 *
 * - Module: \ref call_obj
 */

/*
 * Functionality of call_obj module.
 */

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/time.h>

#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"

#include "cobj.h"

/**
 * \brief Element of the array.
 *
 * When assigned equals false other contents are undefined.
 */
typedef struct {
	bool assigned; /**< Element of the array is assigned. */
	uint64_t timestamp; /**< Timestamp when element was assigned. */
	str callid; /**< Call-ID of associated call. */
} co_object_t;

/**
 * \brief Data type for shared called_obj data between processes.
 */
typedef struct {
	int start; /**< Number of first object. */
	int end; /**< Number of last object (included). */
	/**
	 * \brief Current position of last assigned object.
	 *
	 * 0 - no object has been assigned yet.
	 */
	int cur;
	int assigned; /**< Number of assigned objects at this moment. */
	gen_lock_t *lock; /**< Lock to protect ring array. */
	co_object_t *ring; /**< Array of call objects. */
} co_data_t;

/**
 * \brief Struct containing all call object related data.
 */
static co_data_t *co_data = NULL;

/**
 * \brief Initialize call object module.
 *
 * \return 0 on success.
 */
int cobj_init(int start, int end)
{
	
	if (start == 0) {
		LM_ERR("Wrong start value\n");
		return -1;
	}
	if (end == 0) {
		LM_ERR("Wrong end value\n");
		return -1;
	}
	if (start > end) {
		LM_ERR("End value should be greater than start one [%d, %d]\n", start, end);
		return -1;
	}

	co_data = (co_data_t*)shm_malloc(sizeof(co_data_t));
	if (!co_data) {
		LM_ERR("Cannot allocate shm memory for call object\n");
		return -1;
	}

	co_data->start = start;
	co_data->end = end;
	co_data->cur = 0; /* No object assigned yet. */
	co_data->assigned = 0; /* No assigned objects at this moment. */
	co_data->lock = NULL;
	co_data->ring = NULL;
	
	size_t total_size = (1 + end - start); /* [start, end] */
	size_t array_size = total_size * sizeof(co_object_t);
	LM_DBG("Element size: %lu\n", (unsigned long)sizeof(co_object_t));
	LM_DBG("List element size: %lu\n", (unsigned long)sizeof(cobj_elem_t));
	
	co_data->ring = (co_object_t*)shm_malloc(array_size);
	if (!co_data->ring) {
		LM_ERR("Cannot allocate shm memory for ring in call object\n");
		return -1;
	}
	LM_DBG("Allocated %lu bytes for the ring\n", (unsigned long)array_size);

	/*
	 * Initialize lock.
	 */
	co_data->lock = lock_alloc();
	if (!co_data->lock) {
		LM_ERR("Cannot allocate lock\n");
		return -1;
	}

	if(lock_init(co_data->lock)==NULL)
	{
		LM_ERR("cannot init the lock\n");
		lock_dealloc(co_data->lock);
		co_data->lock = NULL;
		return -1;
	}
	
	co_data->cur = 0; /* No object assigned yet. */

	co_data->start = start;
	co_data->end = end;
	
	/* Every object is set as free. */
	int i;
	for (i=0; i<total_size; i++) {
		co_data->ring[i].assigned = false;
	}
	/* timestamp, etc is undefined. */
	
	LM_DBG("Call object Init: cur=%d  start=%d  end=%d\n",
		   co_data->cur, co_data->start, co_data->end);
	return 0;
}

/**
 * \brief Close call object module.
 */
void cobj_destroy(void)
{
	if (!co_data) {
		/* Nothing to free. */
		return;
	}
	
	/* Free lock */
	if (co_data->lock) {
		LM_DBG("Freeing lock\n");
		lock_destroy(co_data->lock);
		lock_dealloc(co_data->lock);
		co_data->lock = NULL;
	}

	/* Free ring array. */
	if (co_data->ring) {
		LM_DBG("Freeing call object ring\n");
		shm_free(co_data->ring);
		co_data->ring = NULL;
	}

	assert(co_data);
	shm_free(co_data);
	co_data = NULL;
}

/**
 * \brief Get current timestamp in milliseconds.
 *
 * \param ts pointer to timestamp integer.
 * \return 0 on success.
 */
int get_timestamp(uint64_t *ts)
{
	assert(ts);
	
	struct timeval current_time;
	if (gettimeofday(&current_time, NULL) < 0) {
		LM_ERR("failed to get current time!\n");
		return -1;
	}

	*ts = (uint64_t)current_time.tv_sec*1000 +
		(uint64_t)current_time.tv_usec/1000;
	
	return 0;
}

/**
 * \brief Fill an object with data.
 *
 * \return 0 on success.
 */
static int cobj_fill(co_object_t *obj, uint64_t timestamp, str *callid)
{
	assert(obj->assigned == false);
	
	int res = -1;
	
	obj->callid.s = (char*)shm_malloc(callid->len + 1); /* +1 Zero at the end */
	if (!obj->callid.s) {
		LM_ERR("Cannot allocate memory for callid\n");
		goto clean;
	}
	memcpy(obj->callid.s, callid->s, callid->len);
	obj->callid.s[callid->len] = '\0';
	obj->callid.len = callid->len;

	/* Assign timestamp */
	obj->timestamp = timestamp;
	
	/* Everything went fine. */
	obj->assigned = true;
	res = 0;
clean:	
	return res;
}

/**
 * \brief Get a free object.
 *
 * \param timestamp assign this timestamp to the object we get.
 * \param callid pointer to callid str.
 * \return -1 if an error happens.
 * \return number of a free object on success.
 */
int cobj_get(uint64_t timestamp, str *callid)
{
	assert(callid);
	assert(callid->len > 0);
	
	int res = -1; /* Error by default */

	lock_get(co_data->lock);

	LM_DBG("IN co_data->cur: %d\n", co_data->cur);

	if (co_data->cur == 0) {
		/* First object to assign. */
		co_object_t *obj = &co_data->ring[0];
		if (cobj_fill(obj, timestamp, callid)) {
			LM_ERR("Cannot create object 0\n");
			goto clean;
		}
	
		co_data->cur = co_data->start;
		res = co_data->cur;
		co_data->assigned++;
		LM_DBG("Object found: %d\n", res);
		LM_DBG("Current timestamp: %" PRIu64 "\n", obj->timestamp);
		LM_DBG("Current Call-ID: %.*s\n", obj->callid.len, obj->callid.s);
		goto clean;
	}
	assert(co_data->cur >= co_data->start && co_data->cur <= co_data->end);

	/* Find next free position in array. */
	int pos_cur, pos, pos_max;
	pos_cur = co_data->cur - co_data->start; /* Last used position in array. */
	pos = pos_cur + 1; /* Position to check in array. */
	pos_max = co_data->end - co_data->start; /* Maximum acceptable position in array. */
	
	while (pos != pos_cur) {
		if (pos > pos_max) {
			pos = 0;
			continue;
		}
		assert(pos <= pos_max && pos >= 0);

		co_object_t *obj = &co_data->ring[pos];
		if (obj->assigned == false) {
			/* We found a free object. */
		  if (cobj_fill(obj, timestamp, callid)) {
				LM_ERR("Cannot create object %d\n", pos);
				goto clean;
			}
			
			co_data->cur = pos + co_data->start;
			res = co_data->cur;
			co_data->assigned++;
			LM_DBG("Object found: %d\n", res);
			LM_DBG("Current timestamp: %" PRIu64 "\n", obj->timestamp);
			LM_DBG("Current Call-ID: %.*s\n", obj->callid.len, obj->callid.s);
			goto clean;
		}

		pos++;
	}
	
	/* No free object found. */
	res = -1;
	LM_ERR("No free objects available\n");
	
clean:

	LM_DBG("OUT co_data->cur: %d\n", co_data->cur);
	lock_release(co_data->lock);
	return res;
}

/**
 * \brief Free an Object
 *
 * \param num number of object to free
 * \return 0 on success
 */
int cobj_free(int num)
{
	int res = -1; // It fails by default.

	lock_get(co_data->lock);

	if (num < co_data->start || num > co_data->end) {
		LM_ERR("Object out of range %d  [%d, %d]\n", num, co_data->start, co_data->end);
		goto clean;
	}

	int pos = num - co_data->start;
	co_object_t *obj = &co_data->ring[pos];
	if (obj->assigned == true) {
		LM_DBG("Freeing object %d - timestamp: %" PRIu64 " - Call-ID: %.*s\n",
			   num, obj->timestamp, obj->callid.len, obj->callid.s);

		if (obj->callid.s) {
			shm_free(obj->callid.s);
			obj->callid.s = NULL;
		}

		obj->assigned = false;
		co_data->assigned--;
	} else {
		LM_WARN("Freeing an already free object: %d\n", num);
	}
	res = 0;
	LM_DBG("Object %d freed\n", num);
	
clean:		
	lock_release(co_data->lock);
	return res;
}

/**
 * \brief Fill data in cobj_stats_t structure passed as pointer.
 *
 * \param stats pointer to cobj_stats_t structure.
 * \return 0 on success
 */
int cobj_stats_get(cobj_stats_t *stats)
{
	int result = -1; /* It fails by default. */

	lock_get(co_data->lock);
	
	if (!stats) {
		LM_ERR("No cobj_stats_t structure provided\n");
		goto clean;
	}

	stats->start = co_data->start;
	stats->end = co_data->end;
	stats->assigned = co_data->assigned;
	
	/* TODO */

	/* Everything went fine. */
	result = 0;
	
clean:
	lock_release(co_data->lock);
	return result;
}

/**
 * \brief Free all objects at once.
 */
void cobj_free_all(void)
{
	lock_get(co_data->lock);	

	int i;
	int start = co_data->start;
	int end = co_data->end;
	int total = end - start + 1;

	/* Free all objects in the array. */
	for (i=0; i < total; i++) {

		co_object_t *obj = &co_data->ring[i];
		if (obj->assigned == true) {
			if (obj->callid.s) {
				shm_free(obj->callid.s);
				obj->callid.s = NULL;
			}
			obj->assigned = false;
		}

	} /* for i */

	co_data->cur = 0; /* No object assigned yet. */
	co_data->assigned = 0; /* No assigned objects at this moment. */

	LM_DBG("Objects in range [%d, %d] freed\n", start, end);

	lock_release(co_data->lock);
}

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
int cobj_get_timestamp(uint64_t ts, cobj_elem_t **elem, int limit)
{
	assert(elem);
	assert(limit >= 0);

	LM_DBG("Received timestamp: %" PRIu64 "\n", ts);
	
	int res = -1; /* Fail by default; */
	*elem = NULL; /* Empty list by default. */

	int total = co_data->end - co_data->start + 1;
	int num_objects = 0; /* Not found any object by now. */

	/* First and last element of the list. */
	cobj_elem_t *first = NULL;
	
	int i;
	for (i=0; i<total; i++) {
		co_object_t *obj = &co_data->ring[i];
		if (obj->assigned == true && obj->timestamp <= ts) {
			/* Object found */

			cobj_elem_t *elem_new = (cobj_elem_t*)pkg_malloc(sizeof(cobj_elem_t));
			if (!elem_new) {
				LM_ERR("Memory error\n");
				goto clean;
			}

			/* Fill new element with data */
			elem_new->number = co_data->start + i;
			elem_new->timestamp = obj->timestamp;
			elem_new->next = NULL;
			elem_new->callid.s = (char*)pkg_malloc(obj->callid.len + 1); /* +1 Zero at the end */
			if (!elem_new->callid.s) {
				LM_ERR("Cannot allocate memory for callid\n");
				pkg_free(elem_new);
				elem_new = NULL;
				goto clean;
			}
			memcpy(elem_new->callid.s, obj->callid.s, obj->callid.len);
			elem_new->callid.s[obj->callid.len] = '\0';
			elem_new->callid.len = obj->callid.len;

			/* Insert the element in the ascending ordered list. */
			cobj_elem_t *previous = NULL;
			cobj_elem_t *tmp = first;
			while (tmp) {
				if (elem_new->timestamp <= tmp->timestamp) {
					/* We found the position of the new element. */
					break;
				}
				previous = tmp;
				tmp = tmp->next;
			}

			if (previous) {
				/* Non-void list. */
				elem_new->next = previous->next;
				previous->next = elem_new;
				
			} else {
				/* Insert at the beginning. */
				elem_new->next = first;
				first = elem_new;
			}
			num_objects++;

			/* Delete an element if we surpassed the limit. */
			if (limit && num_objects > limit) {
				tmp = first;
				first = first->next;
				tmp->next = NULL;
				cobj_free_list(tmp);
			}

		} /* if obj->assigned */
		
	} /* for i=0 */
		 
	/* Everything went fine */
	res = num_objects;
	*elem = first;
	first = NULL;
	
clean:
	if (first) {
		/* An error occurred */
		cobj_free_list(first);
	}
	
	return res;
}

/**
 * \brief Free an object list.
 *
 * \param elem pointer to first element in the list.
 */
void cobj_free_list(cobj_elem_t *elem)
{
	while (elem) {
		cobj_elem_t *next = elem->next;
		if (elem->callid.s) {
			pkg_free(elem->callid.s);
		}
		pkg_free(elem);
		elem = next;
	}
}
