/**
 * Copyright (C) 2026 Justin LaVelle
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../core/counters.h"
#include "../../core/dprint.h"
#include "../../core/timer.h"

#include "dlgs_records.h"
#include "dlgs_stats.h"

#ifdef STATISTICS

unsigned long dlgs_stats_active_init(void);
unsigned long dlgs_stats_active_progress(void);
unsigned long dlgs_stats_active_answered(void);
unsigned long dlgs_stats_active_confirmed(void);
unsigned long dlgs_stats_active_terminated(void);
unsigned long dlgs_stats_active_notanswered(void);
unsigned long dlgs_stats_final_init(void);
unsigned long dlgs_stats_final_progress(void);
unsigned long dlgs_stats_final_answered(void);
unsigned long dlgs_stats_final_confirmed(void);
unsigned long dlgs_stats_final_terminated(void);
unsigned long dlgs_stats_final_notanswered(void);

/* clang-format off */
static stat_export_t mod_stats[] = {
	{"active_init",        STAT_IS_FUNC, (stat_var **)dlgs_stats_active_init},
	{"active_progress",    STAT_IS_FUNC, (stat_var **)dlgs_stats_active_progress},
	{"active_answered",    STAT_IS_FUNC, (stat_var **)dlgs_stats_active_answered},
	{"active_confirmed",   STAT_IS_FUNC, (stat_var **)dlgs_stats_active_confirmed},
	{"active_terminated",  STAT_IS_FUNC, (stat_var **)dlgs_stats_active_terminated},
	{"active_notanswered", STAT_IS_FUNC, (stat_var **)dlgs_stats_active_notanswered},
	{"final_init",         STAT_IS_FUNC, (stat_var **)dlgs_stats_final_init},
	{"final_progress",     STAT_IS_FUNC, (stat_var **)dlgs_stats_final_progress},
	{"final_answered",     STAT_IS_FUNC, (stat_var **)dlgs_stats_final_answered},
	{"final_confirmed",    STAT_IS_FUNC, (stat_var **)dlgs_stats_final_confirmed},
	{"final_terminated",   STAT_IS_FUNC, (stat_var **)dlgs_stats_final_terminated},
	{"final_notanswered",  STAT_IS_FUNC, (stat_var **)dlgs_stats_final_notanswered},
	{0, 0, 0}
};
/* clang-format on */

static dlgs_stats_t _dlgs_stats_active;
static dlgs_stats_t _dlgs_stats_final;
static ticks_t _dlgs_stats_tm = 0;

static void dlgs_stats_proc_update(void)
{
	ticks_t t;

	t = get_ticks();
	if(t > _dlgs_stats_tm) {
		dlgs_get_stats(&_dlgs_stats_active, &_dlgs_stats_final);
		_dlgs_stats_tm = t;
	}
}

unsigned long dlgs_stats_active_init(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_active.c_init;
}

unsigned long dlgs_stats_active_progress(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_active.c_progress;
}

unsigned long dlgs_stats_active_answered(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_active.c_answered;
}

unsigned long dlgs_stats_active_confirmed(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_active.c_confirmed;
}

unsigned long dlgs_stats_active_terminated(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_active.c_terminated;
}

unsigned long dlgs_stats_active_notanswered(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_active.c_notanswered;
}

unsigned long dlgs_stats_final_init(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_final.c_init;
}

unsigned long dlgs_stats_final_progress(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_final.c_progress;
}

unsigned long dlgs_stats_final_answered(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_final.c_answered;
}

unsigned long dlgs_stats_final_confirmed(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_final.c_confirmed;
}

unsigned long dlgs_stats_final_terminated(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_final.c_terminated;
}

unsigned long dlgs_stats_final_notanswered(void)
{
	dlgs_stats_proc_update();
	return _dlgs_stats_final.c_notanswered;
}

#endif /* STATISTICS */

int dlgs_stats_init(void)
{
#ifdef STATISTICS
	if(register_module_stats("dlgs", mod_stats) != 0) {
		LM_ERR("failed to register statistics\n");
		return -1;
	}
#endif
	return 0;
}
