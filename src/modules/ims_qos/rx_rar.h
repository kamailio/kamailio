/*
 * Copyright (C) 2026 Lyubomir Yatanski
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

#ifndef RX_RAR_H
#define RX_RAR_H

#include "../cdp/diameter_api.h"
#include "rx_authdata.h"

int rx_rar_init(void);
void rx_rar_destroy(void);
AAAMessage *rx_process_rar(AAAMessage *request);
void rx_media_kept(rx_authsessiondata_t *session_data, unsigned int kept);

#endif
