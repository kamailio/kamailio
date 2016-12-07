/*
 * $Id: benchmark_api.h 944 2007-04-11 12:43:49Z bastian $
 *
 * Benchmarking module for Kamailio
 *
 * Copyright (C) 2007 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
 * Copyright (C) 2007 Voice Sistem SRL
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

/*! \file
 * \brief Benchmark :: API functions
 *
 * \ingroup benchmark
 * - Module: benchmark
 */


#ifndef BENCHMARK_API_H
#define BENCHMARK_API_H

#include "../../sr_module.h"

typedef int (*bm_register_timer_f)(char *tname, int mode, unsigned int *id);
typedef int (*bm_start_timer_f)(unsigned int id);
typedef int (*bm_log_timer_f)(unsigned int id);

struct bm_binds {
	bm_register_timer_f bm_register;
	bm_start_timer_f bm_start;
	bm_log_timer_f bm_log;
};

typedef int(*load_bm_f)(struct bm_binds *bmb);

int load_bm(struct bm_binds *bmb);

static inline int load_bm_api( struct bm_binds *bmb )
{
	load_bm_f load_bm;

	/* import the benchmark auto-loading function */
	if ( !(load_bm=(load_bm_f)find_export("load_bm", 0, 0)))
	{
		LM_ERR("can't import load_bm\n");
		return -1;
	}
	/* let the auto-loading function load all benchmarking stuff */
	if (load_bm( bmb )==-1)
	{
		LM_ERR("load_bm failed\n");
		return -1;
	}
	
	return 0;
}

#endif /* BENCHMARK_API_H */
