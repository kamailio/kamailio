/*
 * ss7 module - ISUP parsed
 *
 * Copyright (C) 2016 Holger Hans Peter Freyther (help@moiji-mobile.com)
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
#pragma once

#include <stdint.h>
#include <sys/types.h>

#include "../../lib/srutils/srjson.h"

/**
 * State structure
 */
struct isup_state {
	srjson_doc_t*	json;
};

int isup_parse(const uint8_t *data, size_t len, struct isup_state *ptrs);
