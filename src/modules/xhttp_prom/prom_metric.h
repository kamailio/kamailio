/*
 * Copyright (C) 2012 VoIP Embedded, Inc.
 *
 * Copyright (C) 2019 Vicente Hernando (Sonoc)
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

/**
 * Header user defined metrics for Prometheus.
 */

#ifndef _PROM_METRIC_H_
#define _PROM_METRIC_H_

#include "xhttp_prom.h"

/**
 * Initialize user defined metrics.
 */
int prom_metric_init(int timeout_minutes);

/**
 * Close user defined metrics.
 */
void prom_metric_close();

/**
 * Create a counter and add it to list.
 */
int prom_counter_create(char *spec);

/**
 * Create a gauge and add it to list.
 */
int prom_gauge_create(char *spec);

/**
 * Print user defined metrics.
 */
int prom_metric_list_print(prom_ctx_t *ctx);

/**
 * Reset a counter.
 */
int prom_counter_reset(str *s_name, str *l1, str *l2, str *l3);

/**
 * Reset value in a gauge.
 */
int prom_gauge_reset(str *s_name, str *l1, str *l2, str *l3);

/**
 * Add some positive amount to a counter.
 */
int prom_counter_inc(str *s_name, int number, str *l1, str *l2, str *l3);

/**
 * Set a value in a gauge.
 */
int prom_gauge_set(str *s_name, double number, str *l1, str *l2, str *l3);

#endif // _PROM_METRIC_H_
