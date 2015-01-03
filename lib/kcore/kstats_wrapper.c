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
 * @file kstats_wrapper.h
 * @ingroup: libkcore
 * @author andrei
 */

#include "kstats_wrapper.h"

#ifdef STATISTICS


/** internal wrapper for kamailio type stat callbacks.
 * sr counter callbacks are different from the kamailio type stat callbacks.
 * This function is meant as a sr counter callback that will call
 * k stat callback passed as parameter.
 * @param h - not used.
 * @param param - k stat callback function pointer (stat_function).
 * @return result of calling the passed k stat_function.
 */
static counter_val_t cnt_cbk_wrapper(counter_handle_t h, void* param)
{
	stat_function k_stat_f;
	
	k_stat_f = param;
	return k_stat_f();
}



int register_stat( char *module, char *name, stat_var **pvar, int flags)
{
	int cnt_flags;
	counter_handle_t h;
	int ret;
	
	if (module == 0 || name == 0 || pvar == 0) {
		BUG("invalid parameters (%p, %p, %p)\n", module, name, pvar);
		return -1;
	}
	/* translate kamailio stat flags into sr counter flags */
	cnt_flags = (flags & STAT_NO_RESET) ? CNT_F_NO_RESET : 0;
	if (flags & STAT_IS_FUNC)
		ret = counter_register(&h, module, name, cnt_flags,
					cnt_cbk_wrapper,(stat_function)pvar,
					"kamailio statistic (no description)",
					0);
	else
		ret = counter_register(&h, module, name, cnt_flags, 0, 0,
					"kamailio statistic (no description)", 0);
	if (ret < 0) {
		if (ret == -2)
			ERR("counter %s.%s already registered\n", module, name);
		goto error;
	}
	if (!(flags & STAT_IS_FUNC))
		*pvar = (void*)(unsigned long)h.id;
	return 0;
error:
	if (!(flags & STAT_IS_FUNC))
		*pvar = 0;
	return -1;
}



int register_module_stats(char *module, stat_export_t *stats)
{
	if (module == 0 || *module == 0) {
		BUG("null or empty module name\n");
		goto error;
	}
	if (stats == 0 || stats[0].name == 0)
		/* empty stats */
		return 0;
	for (; stats->name; stats++)
		if (register_stat(module, stats->name, stats->stat_pointer,
							stats->flags) < 0 ){
			ERR("failed to add statistic %s.%s\n", module, stats->name);
			goto error;
		}
	return 0;
error:
	return -1;
}

#endif /* STATISTICS */
/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
