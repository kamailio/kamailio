/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
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
 *
 */

/**
 * NDJSON (newline delimited JSON): one json object per line, no envelope.
 */

#include "../../../core/str.h"

#include "ndjson.h"

/* json line per entry */
static int lh_ndjson_body_entry(lh_body_t *body, const str *entry)
{
	if(lh_body_append(body, entry->s, entry->len) < 0
			|| lh_body_append(body, "\n", 1) < 0)
		return -1;
	return 0;
}

/* clang-format off */
const lh_format_t lh_format_ndjson = {
	"ndjson",
	"application/x-ndjson",
	NULL,
	NULL,
	NULL,
	lh_ndjson_body_entry,
	NULL
};
/* clang-format on */
