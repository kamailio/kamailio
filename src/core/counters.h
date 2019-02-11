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


/* k stat flags */
#define STAT_NO_RESET	1  /* used in dialog(k), nat_traversal(k),
							  registrar(k), statistics(k), usrloc(k) */
/* #define STAT_NO_SYN	2  -- not used */
#define STAT_SHM_NAME	4 /* used only from usrloc(k) */
#define STAT_IS_FUNC	8



#ifdef STATISTICS

/* statistics support check */
#define stats_support() 1

/* types */

typedef counter_val_t    stat_val;
/* stat_var is always used as a pointer in k, we missuse
   stat_var* for holding out counter id */
typedef void stat_var;
/* get val callback
 * TODO: change it to counter_cbk_f compatible callback?
 */
typedef counter_val_t (*stat_function)(void);

/* statistic module interface */
struct stat_export_s {
	char* name;
	int flags;
	stat_var** stat_pointer; /* pointer to the memory location
								(where a counter handle will be stored)
								Note: it's a double pointer because of
								the original k version which needed it
								allocated in shm. This version
								will store the counter id at *stat_pointer.
							  */
};

typedef struct stat_export_s stat_export_t;

int register_stat( char *module, char *name, stat_var **pvar, int flags);
int register_module_stats(char *module, stat_export_t *stats);

inline static stat_var* get_stat(str *name)
{
	counter_handle_t h;
	str grp;

	grp.s = 0;
	grp.len = 0;
	if (counter_lookup_str(&h, &grp, name) < 0)
		return 0;
	return (void*)(unsigned long)h.id;
}



inline static unsigned long get_stat_val(stat_var *v)
{
	counter_handle_t h;
	h.id = (unsigned short)(unsigned long)v;
	return (unsigned long)counter_get_val(h);
}



inline static char* get_stat_name(stat_var *v)
{
	counter_handle_t h;
	h.id = (unsigned short)(unsigned long)v;
	return counter_get_name(h);
}



inline static char* get_stat_module(stat_var *v)
{
	counter_handle_t h;
	h.id = (unsigned short)(unsigned long)v;
	return counter_get_group(h);
}



inline static void update_stat(stat_var* v, int n)
{
	counter_handle_t h;
	h.id = (unsigned short)(unsigned long)v;
	counter_add(h, n);
}



inline static void reset_stat(stat_var* v)
{
	counter_handle_t h;
	h.id = (unsigned short)(unsigned long)v;
	counter_reset(h);
}


#define if_update_stat(c, var, n) \
	do{ \
		if ((c)) update_stat((var), (n)); \
	}while(0)

#define if_reset_stat(c, var) \
	do{ \
		if ((c)) reset_stat((var)); \
	}while(0)

#else /* STATISTICS */

/* statistics support check */
#define stats_support() 0
#define register_module_stats(mod, stats) 0
#define register_stat(mod, name, var, flags) 0
#define get_stat(name)  0
#define get_stat_val(var) 0
#define update_stat(v, n)
#define reset_stat(v)
#define if_update_stat(c, v, n)
#define if_reset_stat(c, v)

#endif /* STATISTICS */

#endif /*__counters_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
