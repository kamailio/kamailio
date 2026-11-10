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
 * Elasticsearch Bulk API: every entry is preceded by an action line
 * {"index":{"_index":"<index>"}}.
 * When "index" is not set, the index has to be part of the target url
 * (http://host:9200/<index>/_bulk) and the action line is {"index":{}}.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../../core/dprint.h"
#include "../../../core/str.h"

#include "elasticsearch.h"

#define LH_INDEX_BUF_SIZE 256

static str lh_es_index = STR_NULL;
static char lh_es_action[LH_INDEX_BUF_SIZE + 32];
static int lh_es_action_len = 0;

/* clang-format off */
static const lh_format_opt_t lh_es_opts[] = {
	{"index", LH_OPT_STR, &lh_es_index},
	{NULL, 0, NULL}
};
/* clang-format on */

static int lh_es_init(void)
{
	if(lh_es_index.len >= LH_INDEX_BUF_SIZE) {
		LM_ERR("index name too long [%.*s]\n", lh_es_index.len, lh_es_index.s);
		return -1;
	}
	return 0;
}

/* compute the bulk action line once per batch */
static int lh_es_body_begin(lh_body_t *body)
{
	char index[LH_INDEX_BUF_SIZE];
	int n;

	(void)body;

	if(lh_es_index.len) {
		if(memchr(lh_es_index.s, '%', lh_es_index.len)) {
			time_t now = time(NULL);
			struct tm tm_;

			if(!gmtime_r(&now, &tm_)
					|| strftime(index, sizeof(index), lh_es_index.s, &tm_)
							   == 0) {
				LM_ERR("failed to format index name [%s]\n", lh_es_index.s);
				return -1;
			}
		} else {
			memcpy(index, lh_es_index.s, lh_es_index.len + 1);
		}
		n = snprintf(lh_es_action, sizeof(lh_es_action),
				"{\"index\":{\"_index\":\"%s\"}}\n", index);
	} else {
		n = snprintf(lh_es_action, sizeof(lh_es_action), "{\"index\":{}}\n");
	}
	if(n < 0 || n >= (int)sizeof(lh_es_action)) {
		LM_ERR("index name too long [%s]\n", lh_es_index.s);
		return -1;
	}
	lh_es_action_len = n;
	return 0;
}

/* bulk item: action line, json entry, newline */
static int lh_es_body_entry(lh_body_t *body, const str *entry)
{
	if(lh_body_append(body, lh_es_action, lh_es_action_len) < 0
			|| lh_body_append(body, entry->s, entry->len) < 0
			|| lh_body_append(body, "\n", 1) < 0)
		return -1;
	return 0;
}

/* clang-format off */
const lh_format_t lh_format_elasticsearch = {
	"elasticsearch",
	"application/x-ndjson",
	lh_es_opts,
	lh_es_init,
	lh_es_body_begin,
	lh_es_body_entry,
	NULL
};
/* clang-format on */
