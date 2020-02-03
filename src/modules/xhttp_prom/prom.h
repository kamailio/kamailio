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
 * Header for functionality of prometheus module.
 */

#ifndef _PROM_H_
#define _PROM_H_

#include "xhttp_prom.h"

/**
 * Get current timestamp in milliseconds.
 *
 * /param ts pointer to timestamp integer.
 * /return 0 on success.
 */
int get_timestamp(uint64_t *ts);

/**
 * Write some data in prom_body buffer.
 *
 * /return number of bytes written.
 * /return -1 on error.
 */
int prom_body_printf(prom_ctx_t *ctx, char *fmt, ...);

/**
 * Get statistics (based on stats_get_all)
 *
 * /return 0 on success
 */
int prom_stats_get(prom_ctx_t *ctx, str *stat);

#endif // _PROM_H_
