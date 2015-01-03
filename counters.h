/* 
 * Copyright (C) 2010 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/** Kamailio core :: counter/stats.
 * @author andrei
 * @file counters.h
 * @ingroup:  core
 *
 *  Example usage:
 *  1. register (must be before forking, e.g. from mod_init()):
 *    counter_handle_t h;
 *    counter_register(&h, "my_counters", "foo", 0, 0, 0, "test counter", 0);
 *  2. increment/add:
 *    counter_inc(h);
 *    counter_add(h, 100);
 *  3. get and existing counter handle, knowing its group and name
 *    counter_lookup(&h, "my_counters", "foo");
 *  4. get a counter value (the handle can be obtained like above)
 *    val = counter_get(h);
 */

#ifndef __counters_h
#define __counters_h

#include "pt.h"

/* counter flags */
#define CNT_F_NO_RESET 1 /* don't reset */

typedef long counter_val_t;

/* use a struct. to force errors on direct access attempts */
struct counter_handle_s {
	unsigned short id;
};


struct counter_val_s {
	counter_val_t v;
};


typedef struct counter_handle_s counter_handle_t;
typedef struct counter_val_s counter_array_t;
typedef counter_val_t (*counter_cbk_f)(counter_handle_t h, void* param);



/* counter definition structure, used in zero term. arrays for more
 *  convenient registration of several counters at once
 *  (see counter_register_array(group, counter_array)).
 */
struct counter_def_s {
	counter_handle_t* handle; /** if non 0, will be filled with the counter
							     handle */
	const char* name;         /**< counter name (inside the group) */
	int flags;                /**< counter flags */
	counter_cbk_f get_cbk;    /**< callback function for reading */
	void* get_cbk_param;      /**< callback parameter */
	const char* descr;        /**< description/documentation string */
};

typedef struct counter_def_s counter_def_t;



extern counter_array_t* _cnts_vals;
extern int _cnts_row_len; /* number of elements per row */



int counters_initialized(void);
int init_counters(void);
void destroy_counters(void);
int counters_prefork_init(int max_process_no);


int counter_register_array(const char* group, counter_def_t* defs);
int counter_register(	counter_handle_t* handle, const char* group,
						const char* name, int flags,
						counter_cbk_f cbk, void* cbk_param,
						const char* doc,
						int reg_flags);
int counter_lookup(counter_handle_t* handle,
						const char* group, const char* name);
int counter_lookup_str(counter_handle_t* handle, str* group, str* name);

void counter_reset(counter_handle_t handle);
counter_val_t counter_get_val(counter_handle_t handle);
counter_val_t counter_get_raw_val(counter_handle_t handle);
char* counter_get_name(counter_handle_t handle);
char* counter_get_group(counter_handle_t handle);
char* counter_get_doc(counter_handle_t handle);

/** gets the per process value of counter h for process p_no.
 *  Note that if used before counter_prefork_init() process_no is 0
 *  and _cnts_vals will point into a temporary one "row"  array.
 */
#define counter_pprocess_val(p_no, h) \
	_cnts_vals[(p_no) * _cnts_row_len + (h).id].v



/** increments a counter.
 * @param handle - counter handle.
 */
inline static void counter_inc(counter_handle_t handle)
{
	counter_pprocess_val(process_no, handle)++;
}



/** adds a value to a counter.
 * @param handle - counter handle.
 * @param v - value.
 */
inline static void counter_add(counter_handle_t handle, int v)
{
	counter_pprocess_val(process_no, handle)+=v;
}



void counter_iterate_grp_names(void (*cbk)(void* p, str* grp_name), void* p);
void counter_iterate_grp_var_names(	const char* group,
									void (*cbk)(void* p, str* var_name),
									void* p);
void counter_iterate_grp_vars(const char* group,
							  void (*cbk)(void* p, str* g, str* n,
								  			counter_handle_t h),
							  void *p);

#endif /*__counters_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
