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
/** k compatible statistics implemented in terms of sr counters.
 * New functions:
 *  stats_support() - partially replaces get_stats_collector().
 *    Returns 1 if statistics support is compiled, 0 otherwise.
 *  get_stat_name() - returns the name of a stat_var.
 *  get_stat_module() - returns the module of a stat_var.
 * Removed functions:
 *  get_stats_collector()
 *  destroy_stats_collector()
 * Removed variables/structures:
 *   stats_collector
 *   module_stats
 *
 * @file kstats_wrapper.h
 * @ingroup: libkcore
 */

/*! @defgroup libkcore Kamailio compatibility core library
 *
 */

#ifndef __kstats_wrapper_h
#define __kstats_wrapper_h

#include "../../counters.h"
#include "../../kstats_types.h"

/* k stat flags */
#define STAT_NO_RESET	1  /* used in dialog(k), nat_traversal(k),
							  registrar(k), statistics(k), usrloc(k) */
/* #define STAT_NO_SYN	2  -- not used */
#define STAT_SHM_NAME	4 /* used only from usrloc(k) */
#define STAT_IS_FUNC	8



#ifdef STATISTICS

/* statistics support check */
#define stats_support() 1

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

#endif /*__kstats_wrapper_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
