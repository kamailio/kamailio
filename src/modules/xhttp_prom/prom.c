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
 * Functionality of prometheus module.
 */

#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <stdarg.h>

#include "../../core/counters.h"
#include "../../core/ut.h"

#include "prom.h"
#include "prom_metric.h"

/**
 * Delete current buffer data.
 */
void prom_body_delete(prom_ctx_t *ctx)
{
	ctx->reply.body.len = 0;
}

/**
 * Write some data in prom_body buffer.
 *
 * /return number of bytes written.
 * /return -1 on error.
 */
int prom_body_printf(prom_ctx_t *ctx, char *fmt, ...)
{
	struct xhttp_prom_reply *reply = &ctx->reply;
	
	va_list ap;
	
	va_start(ap, fmt);

	LM_DBG("Body current length: %d\n", reply->body.len);

	char *p = reply->buf.s + reply->body.len;
	int remaining_len = reply->buf.len - reply->body.len;
	LM_DBG("Remaining length: %d\n", remaining_len);

	/* int vsnprintf(char *str, size_t size, const char *format, va_list ap); */
	int len = vsnprintf(p, remaining_len, fmt, ap);
	if (len < 0) {
		LM_ERR("Error printing body buffer\n");
		goto error;
	} else if (len >= remaining_len) {
		LM_ERR("Error body buffer overflow: %d (%d)\n", len, remaining_len);
		goto error;
	} else {
		/* Buffer printed OK. */
		reply->body.len += len;
		LM_DBG("Body new length: %d\n", reply->body.len);
	}

	va_end(ap);
	return len;

error:
	va_end(ap);
	return -1;
}

/**
 * Get current timestamp in milliseconds.
 *
 * /param ts pointer to timestamp integer.
 * /return 0 on success.
 */
int get_timestamp(uint64_t *ts)
{
	assert(ts);
	
	struct timeval current_time;
	if (gettimeofday(&current_time, NULL) < 0) {
		LM_ERR("failed to get current time!\n");
		return -1;
	}

	*ts = (uint64_t)current_time.tv_sec*1000 +
		(uint64_t)current_time.tv_usec/1000;
	
	return 0;
}

/**
 * Generate a string suitable for a Prometheus metric.
 *
 * /return 0 on success.
 */
static int metric_generate(prom_ctx_t *ctx, str *group, str *name, counter_handle_t *h)
{
	long counter_val = counter_get_val(*h);

	/* Calculate timestamp. */
	uint64_t ts;
	if (get_timestamp(&ts)) {
		LM_ERR("Error getting current timestamp\n");
		return -1;
	}
	LM_DBG("Timestamp: %" PRIu64 "\n", ts);

	
	/* LM_DBG("%.*s:%.*s = %lu\n",
	   group->len, group->s, name->len, name->s, counter_val); */
	LM_DBG("kamailio_%.*s_%.*s %lu %" PRIu64 "\n",
		   group->len, group->s, name->len, name->s,
		   counter_val, (uint64_t)ts);

	if (prom_body_printf(ctx, "kamailio_%.*s_%.*s %lu %" PRIu64 "\n",
						 group->len, group->s, name->len, name->s,
						 counter_val, (uint64_t)ts) == -1) {
		LM_ERR("Fail to print\n");
		return -1;
	}

	return 0;
}

/**
 * Statistic getter callback.
 */
static void prom_get_grp_vars_cbk(void* p, str* g, str* n, counter_handle_t h)
{
	metric_generate(p, g, n, &h);
}

/**
 * Group statistic getter callback.
 */
static void prom_get_all_grps_cbk(void* p, str* g)
{
	counter_iterate_grp_vars(g->s, prom_get_grp_vars_cbk, p);
}

#define STATS_MAX_LEN 1024

/**
 * Get statistics (based on stats_get_all)
 *
 * /return 0 on success
 */
int prom_stats_get(prom_ctx_t *ctx, str *stat)
{
	if (stat == NULL) {
		LM_ERR("No stats set\n");
		return -1;
	}

	prom_body_delete(ctx);
	
	LM_DBG("User defined statistics\n");
	if (prom_metric_list_print(ctx)) {
		LM_ERR("Fail to print user defined metrics\n");
		return -1;
	}

	LM_DBG("Statistics for: %.*s\n", stat->len, stat->s);

	int len = stat->len;

	stat_var *s_stat;

	if (len == 0) {
		LM_DBG("Do not show Kamailio statistics\n");

	}
	else if (len==3 && strncmp("all", stat->s, 3)==0) {
		LM_DBG("Showing all statistics\n");
		if (prom_body_printf(
				ctx, "\n# Kamailio whole internal statistics\n") == -1) {
			LM_ERR("Fail to print\n");
			return -1;
		}	

		counter_iterate_grp_names(prom_get_all_grps_cbk, ctx);
	}
	else if (stat->s[len-1]==':') {
		LM_DBG("Showing statistics for group: %.*s\n", stat->len, stat->s);

		if (len == 1) {
			LM_ERR("Void group for statistics: %.*s\n", stat->len, stat->s);
			return -1;
			
		} else {
			if (prom_body_printf(
					ctx, "\n# Kamailio statistics for group: %.*s\n",
					stat->len, stat->s) == -1) {
				LM_ERR("Fail to print\n");
				return -1;
			}

			/* Temporary stat_tmp string. */
			char stat_tmp[STATS_MAX_LEN];
			memcpy(stat_tmp, stat->s, len);
			stat_tmp[len-1] = '\0';
			counter_iterate_grp_vars(stat_tmp, prom_get_grp_vars_cbk, ctx);
			stat_tmp[len-1] = ':';
		}
	}
	else {
		LM_DBG("Showing statistic for: %.*s\n", stat->len, stat->s);

		s_stat = get_stat(stat);
		if (s_stat) {
			str group_str, name_str;
			group_str.s = get_stat_module(s_stat);
			if (group_str.s) {
				group_str.len = strlen(group_str.s);
			} else {
				group_str.len = 0;
			}
			
			name_str.s = get_stat_name(s_stat);
			if (name_str.s) {
				name_str.len = strlen(name_str.s);
			} else {
				name_str.len = 0;
			}
			
			LM_DBG("%s:%s = %lu\n",
				   ZSW(get_stat_module(s_stat)), ZSW(get_stat_name(s_stat)),
				   get_stat_val(s_stat));

			if (group_str.len && name_str.len && s_stat) {
				if (prom_body_printf(
						ctx, "\n# Kamailio statistics for: %.*s\n",
						stat->len, stat->s) == -1) {
					LM_ERR("Fail to print\n");
					return -1;
				}

				counter_handle_t stat_handle;
				stat_handle.id = (unsigned short)(unsigned long)s_stat;
				if (metric_generate(ctx, &group_str, &name_str, &stat_handle)) {
					LM_ERR("Failed to generate metric: %.*s - %.*s\n",
						   group_str.len, group_str.s,
						   name_str.len, name_str.s);
					return -1;
				}
			} else {
				LM_ERR("Not enough length for group (%d) or name (%d)\n",
					   group_str.len, name_str.len);
				return -1;
			}
		} /* if s_stat */
		else {
			LM_ERR("stat not found: %.*s\n", stat->len, stat->s);
			return -1;
		}
	} /* if len == 0 */

	return 0;
} /* prom_stats_get */

