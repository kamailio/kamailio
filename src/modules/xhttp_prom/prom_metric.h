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
 * @file
 * @brief xHTTP_PROM :: Header for user defined metrics.
 * @ingroup xhttp_prom
 * - Module: @ref xhttp_prom
 */

#ifndef _PROM_METRIC_H_
#define _PROM_METRIC_H_

#include "xhttp_prom.h"

/**
 * @brief Initialize user defined metrics.
 */
int prom_metric_init();

/**
 * @brief Close user defined metrics.
 */
void prom_metric_close();

/**
 * @brief Create a counter and add it to list.
 */
int prom_counter_create(char *spec);

/**
 * @brief Create a gauge and add it to list.
 */
int prom_gauge_create(char *spec);

/**
 * @brief Create a histogram and add it to list.
 *
 * @return 0 on success.
 */
int prom_histogram_create(char *spec);

/**
 * @brief Print user defined metrics.
 */
int prom_metric_list_print(prom_ctx_t *ctx);

/**
 * @brief Reset a counter.
 */
int prom_counter_reset(str *s_name, str *l1, str *l2, str *l3);

/**
 * @brief Reset value in a gauge.
 */
int prom_gauge_reset(str *s_name, str *l1, str *l2, str *l3);

/**
 * @brief Updates a counter.
 */
int prom_counter_update(str *s_name, operation operation, int number, str *l1, str *l2, str *l3);

/**
 * @brief Set a value in a gauge.
 */
int prom_gauge_set(str *s_name, double number, str *l1, str *l2, str *l3);

/**
 * @brief Observe a value in a histogram.
 *
 * @param number value to observe.
 */
int prom_histogram_observe(str *s_name, double number, str *l1, str *l2, str *l3);

/**
 * @brief Parse a string and convert to double.
 *
 * @param s_number pointer to number string.
 * @param pnumber double passed as reference.
 *
 * @return 0 on success.
 * On error value pointed by pnumber is undefined.
 */
int double_parse_str(str *s_number, double *pnumber);

#endif // _PROM_METRIC_H_
