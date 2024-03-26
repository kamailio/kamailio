/**
 * Copyright (C) 2024 Ovidiu Sas (VoIP Embedded, Inc)
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

/*!
 * \file
 * \brief API
 * \ingroup kex
 * Module: \ref kex
 */

#include "pkg_stats.h"
#include "../../core/sr_module.h"

#ifndef _KEX_API_H_
#define _KEX_API_H_

typedef int (*kex_get_pkmem_stats_f)(pkg_proc_stats_t **_pkg_procstatslist);

/*
 * Struct with the kex api.
 */
typedef struct kex_binds
{
	kex_get_pkmem_stats_f get_pkmem_stats;
} kex_api_t;

typedef int (*bind_kex_f)(kex_api_t *);

/*
 * function exported by module - it will load the other functions
 */
int bind_kex(kex_api_t *);

/*
 * Function to be called directly from other modules to load
 * the kex API.
 */
inline static int load_kex_api(kex_api_t *kob)
{
	bind_kex_f bind_kex_exports;

	if(!(bind_kex_exports = (bind_kex_f)find_export("bind_kex", 0, 0))) {
		LM_ERR("Failed to import bind_kex\n");
		return -1;
	}
	return bind_kex_exports(kob);
}

#endif /*_KEX_API_H_*/
