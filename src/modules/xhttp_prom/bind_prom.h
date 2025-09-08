/*
 * Copyright (C) 2025 Anton Yabchinskiy
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#ifndef _BIND_PROM_H
#define _BIND_PROM_H

#include "../../core/sr_module.h"

typedef int (*prom_api_counter_create_f)(char *spec);
typedef int (*prom_api_counter_reset_f)(str *s_name, str *l1, str *l2, str *l3);
typedef int (*prom_api_counter_inc_f)(
		str *s_name, int number, str *l1, str *l2, str *l3);

typedef int (*prom_api_gauge_create_f)(char *spec);
typedef int (*prom_api_gauge_reset_f)(str *s_name, str *l1, str *l2, str *l3);
typedef int (*prom_api_gauge_inc_f)(
		str *s_name, double number, str *l1, str *l2, str *l3);
typedef int (*prom_api_gauge_set_f)(
		str *s_name, double number, str *l1, str *l2, str *l3);

typedef int (*prom_api_histogram_create_f)(char *spec);
typedef int (*prom_api_histogram_observe_f)(
		str *s_name, double number, str *l1, str *l2, str *l3);

typedef struct prom_api
{
	prom_api_counter_create_f counter_create;
	prom_api_counter_reset_f counter_reset;
	prom_api_counter_inc_f counter_inc;

	prom_api_gauge_create_f gauge_create;
	prom_api_gauge_reset_f gauge_reset;
	prom_api_gauge_inc_f gauge_inc;
	prom_api_gauge_set_f gauge_set;

	prom_api_histogram_create_f histogram_create;
	prom_api_histogram_observe_f histogram_observe;
} prom_api_t;

int bind_prom(prom_api_t *api);

typedef int (*bind_prom_f)(prom_api_t *api);

static inline int prom_load_api(prom_api_t *api)
{
	bind_prom_f bind_prom_exports;
	if(!(bind_prom_exports =
					   (bind_prom_f)find_export("bind_prom", NO_SCRIPT, 0))) {
		LM_ERR("cannot find bind_prom\n");
		return -1;
	}
	return bind_prom_exports(api);
}

#endif /* _BIND_PROM_H */
