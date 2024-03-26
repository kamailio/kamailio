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
 * \brief API integration
 * \ingroup kex
 * Module: \ref kex
 */

#include <stddef.h>

#include "api.h"
#include "pkg_stats.h"
#include "../../core/mod_fix.h"

int get_pkmem_stats_api(pkg_proc_stats_t **_pkg_procstatslist)
{
	*_pkg_procstatslist = get_pkg_proc_stats_list();
	return get_pkg_proc_stats_no();
}

/*
 * Function to load the kex api.
 */
int bind_kex(kex_api_t *kob)
{
	if(kob == NULL) {
		LM_WARN("Cannot load kex API into a NULL pointer\n");
		return -1;
	}
	kob->get_pkmem_stats = get_pkmem_stats_api;
	return 0;
}
