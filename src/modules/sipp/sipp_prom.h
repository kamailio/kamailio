/**
 * Copyright (C) 2026 Aurora Innovation AB
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _SIPP_PROM_H_
#define _SIPP_PROM_H_

#include "sipp_test.h"

/**
 * Initialize Prometheus integration
 * Checks if xhttp_prom module is loaded
 * @return 0 on success, -1 on error
 */
int sipp_prom_init(void);

/**
 * Push SIPp test metrics to Prometheus
 * @param test Test structure containing metrics
 */
void sipp_prom_push_metrics(sipp_test_t *test);

/**
 * Check if Prometheus integration is enabled
 * @return 1 if enabled, 0 if disabled
 */
int sipp_prom_is_enabled(void);

#endif /* _SIPP_PROM_H_ */
