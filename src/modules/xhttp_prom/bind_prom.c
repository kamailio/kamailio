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

#include "bind_prom.h"
#include "prom_metric.h"

int bind_prom(prom_api_t *api)
{
	if(!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	api->counter_create = prom_counter_create;
	api->counter_reset = prom_counter_reset;
	api->counter_inc = prom_counter_inc;
	api->gauge_create = prom_gauge_create;
	api->gauge_reset = prom_gauge_reset;
	api->gauge_inc = prom_gauge_inc;
	api->gauge_set = prom_gauge_set;
	api->histogram_create = prom_histogram_create;
	api->histogram_observe = prom_histogram_observe;
	return 0;
}
