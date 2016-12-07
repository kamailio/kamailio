/*
 * Copyright (C) 2012 Sipwise GmbH
 * Written by Richard Fuchs <rfuchs@sipwise.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

enum scales {
	SCALE_YEAR = 0,
	SCALE_MONTH,
	SCALE_WEEK,
	SCALE_MDAY,
	SCALE_WDAY,
	SCALE_YDAY,
	SCALE_HOUR,
	SCALE_MINUTE,
	SCALE_SECOND,

	SCALE_MAX
};

#define MAX_CODES 2
#define MAX_VALUE_LEN 16 /* for spelled out month names, longest is "September" */

typedef int (*scale_match_func)(const int time_var, const char *from, const char *to);

struct scale_definition {
	scale_match_func	func;
	const char		*codes[MAX_CODES];
	int			flags;
};

#define FLAG_INTEGER_ARGS		0x1

static int year_fn(const int time_var, const char *from, const char *to);
static int month_fn(const int time_var, const char *from, const char *to);
static int week_fn(const int time_var, const char *from, const char *to);
static int mday_fn(const int time_var, const char *from, const char *to);
static int wday_fn(const int time_var, const char *from, const char *to);
static int yday_fn(const int time_var, const char *from, const char *to);
static int hour_fn(const int time_var, const char *from, const char *to);
static int minute_fn(const int time_var, const char *from, const char *to);
static int second_fn(const int time_var, const char *from, const char *to);

static const struct scale_definition defs[SCALE_MAX + 1] = {
	[SCALE_YEAR  ] = { year_fn,   { "year",   "yr"  }, FLAG_INTEGER_ARGS },
	[SCALE_MONTH ] = { month_fn,  { "month",  "mo"  }, 0                 },
	[SCALE_WEEK  ] = { week_fn,   { "week",   "wk"  }, FLAG_INTEGER_ARGS },	/* week of the month */
	[SCALE_MDAY  ] = { mday_fn,   { "mday",   "md"  }, FLAG_INTEGER_ARGS },	/* day of the month */
	[SCALE_WDAY  ] = { wday_fn,   { "wday",   "wd"  }, 0                 },	/* day of the week */
	[SCALE_YDAY  ] = { yday_fn,   { "yday",   "yd"  }, FLAG_INTEGER_ARGS },	/* day of the year */
	[SCALE_HOUR  ] = { hour_fn,   { "hour",   "hr"  }, FLAG_INTEGER_ARGS },
	[SCALE_MINUTE] = { minute_fn, { "minute", "min" }, FLAG_INTEGER_ARGS },
	[SCALE_SECOND] = { second_fn, { "second", "sec" }, FLAG_INTEGER_ARGS },
	/* XXX week of the year? */

	[SCALE_MAX   ] = { NULL, { NULL, } },
};

static const char *months[12] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug",
	"sep", "oct", "nov", "dec" };
static const char *weekdays[7] = { "su", "mo", "tu", "we", "th", "fr", "sa" };

static void get_time_vars(int time_vars[SCALE_MAX], time_t t) {
	struct tm tm;

	localtime_r(&t, &tm);

	time_vars[SCALE_YEAR]   = tm.tm_year + 1900;
	time_vars[SCALE_MONTH]  = tm.tm_mon + 1;
	time_vars[SCALE_WEEK]   = (tm.tm_mday - 1 + (tm.tm_wday - tm.tm_mday + 1) % 7) / 7 + 1;
	time_vars[SCALE_MDAY]   = tm.tm_mday;
	time_vars[SCALE_WDAY]   = tm.tm_wday + 1;
	time_vars[SCALE_YDAY]   = tm.tm_yday + 1;
	time_vars[SCALE_HOUR]   = tm.tm_hour;
	time_vars[SCALE_MINUTE] = tm.tm_min;
	time_vars[SCALE_SECOND] = tm.tm_sec;
}

#define WS_SKIP() while (*p == ' ') p++;

int in_period(time_t t, const char *p) {
	int time_vars[SCALE_MAX];
	int scale_results[SCALE_MAX];
	int scale, j, len, res;
	const char *c, *a1, *a2;
	char from[MAX_VALUE_LEN], to[MAX_VALUE_LEN], *str;

	/* If no period is given or string is empty, the time always matches */
	if (!p)
		return 1;
	WS_SKIP();
	if (!*p)
		return 1;

	/* If "none" or "never" is given, time never matches */
	if (!strcasecmp(p, "none") || !strcasecmp(p, "never"))
		return 0;

	get_time_vars(time_vars, t);

	/* Loop through all sub-periods, separated by commas.
	   string :=  PERIOD [, PERIOD ... ] */
	while (1) {
		memset(scale_results, -1, sizeof(scale_results));

		/* Each sub-period consists of one or more scales, separated by spaces.
		   PERIOD := SCALE { VALUES } [ SCALE { VALUES } ... ] */
		while (1) {
			/* XXX could do some hashing here */
			for (scale = 0; scale < SCALE_MAX; scale++) {
				for (j = 0; j < MAX_CODES; j++) {
					c = defs[scale].codes[j];
					len = strlen(c);
					if (strncasecmp(p, c, len))
						continue;
					if (p[len] == ' ' || p[len] == '{')
						goto found_scale;
				}
			}

			/* No valid scale definition found, syntax error */
			return -1;

found_scale:
			/* Skip over scale name, whitespace and brace */
			p += len;
			WS_SKIP();
			if (*p != '{')
				return -1; /* Syntax error */
			p++;
			WS_SKIP();

			/* Keep track of what we've found */
			if (scale_results[scale] == -1)
				scale_results[scale] = 0;
			else if (scale_results[scale] == 1) {
				/* We already have a valid match for this scale. Skip
				   over all this nonsense then. */
				while (*p && *p != '}')
					p++;
				if (!*p)
					return -1; /* Syntax error */
				goto close_brace;
			}

			/* We're inside the braces now. Values are separated
			   by spaces, possibly given as ranges.
			   VALUES := ( VALUE | RANGE ) [ ( VALUE | RANGE ) ... ]
			   RANGE := VALUE - VALUE */

			while (1) {
				str = from;
				len = sizeof(from) - 1;
				*from = *to = '\0';
				while (1) {
					switch (*p) {
						case '\0':
							return -1; /* Syntax error */

						case ' ':
							WS_SKIP();
							/* Here, it's either a hyphen or end of value/range */
							if (*p == '-')
								goto hyphen;
							break;

						case '-':
hyphen:
							if (!*from)
								return -1; /* Range given as "-foo", syntax error */
							if (*to)
								return -1; /* Range given as "foo-bar-baz", syntax error */
							/* Terminate "from" string and init for "to" */
							*str = '\0';
							str = to;
							len = sizeof(to) - 1;
							p++;
							WS_SKIP();
							continue;

						case '}':
							break;

						default:
							/* everything else gets copied and lowercased */
							if (len <= 0)
								return -1; /* String too long, syntax error */
							*str++ = *p++ | 0x20;	/* works for letters and digits */
							len--;
							continue;
					}
					break;
				}

				*str = '\0';

				/* Finished parsing out value or range. An empty result
				   is valid, e.g. at the end of the list for the scale */
				if (!*from) {
					if (*p == '}')
						break;
					continue;
				}
				a1 = from;
				a2 = *to ? to : NULL;
				if ((defs[scale].flags & FLAG_INTEGER_ARGS)) {
					a1 = (void *) atol(a1);
					a2 = (void *) (a2 ? atol(a2) : -1);
				}
				res = defs[scale].func(time_vars[scale], a1, a2);
				printf("result: %i\n", res);

				if (res == -1)
					return -1; /* Syntax error */
				else if (res == 1)
					scale_results[scale] = 1;
			}

close_brace:
			p++;

			/* Finished with one scale, braces closed. See if there's any more */
			WS_SKIP();
			if (!*p || *p == ',') {
				/* Nope! Evaluate our result */
				for (scale = 0; scale < SCALE_MAX; scale++) {
					if (scale_results[scale] == 0)
						goto no_match;
				}

				/* All scales that were given matched! We're done! */
				return 1;

no_match:
				/* No luck, try next one if there are any more */
				if (*p == ',') {
					p++;
					WS_SKIP();
					break;
				}

				return 0;
			}

			continue;
		}
	}
}

static int generic_fn(const int time_var, const long f, long t, const long min, const long max) {
	if (t == -1)
		t = f;

	if (f < min || f > max)
		return -1;
	if (t < min || t > max)
		return -1;

	if (f > t) {
		if (f <= time_var || t >= time_var)
			return 1;
	}
	else if (f <= time_var && time_var <= t)
		return 1;
	return 0;
}

static int generic_named_fn(const int time_var, const char *from, const char *to,
		const char **array, int arr_len, int str_len) {

	int i, f = 0, t = 0;

	f = atoi(from);
	if (!f)
		for (i = 0; i < arr_len; i++)
			if (!strncmp(array[i], from, str_len)) {
				f = i + 1;
				break;
			}
	if (!f)
		return -1;

	if (!to)
		t = f;
	else {
		t = atoi(to);
		if (!t)
			for (i = 0; i < arr_len; i++)
				if (!strncmp(array[i], to, str_len)) {
					t = i + 1;
					break;
				}
		if (!t)
			return -1;
	}

	return generic_fn(time_var, f, t, 1, arr_len);
}

static int year_fn(const int time_var, const char *from, const char *to) {
	long f, t, c;
	printf("%s %i %li %li\n", __FUNCTION__, time_var, (long) from, (long) to);

	f = (long) from;
	t = (long) to;

	if (t == -1)
		t = f;

	c = time_var / 100;	

	if (t < 0)
		return -1;
	else if (t <= 99)
		t += c;
	else if (t < 1970)
		return -1;

	if (f < 0)
		return -1;
	else if (f <= 99)
		f += c;
	else if (f < 1970)
		return -1;

	if (time_var >= f && time_var <= t)
		return 1;
	return 0;
}

static int month_fn(const int time_var, const char *from, const char *to) {
	printf("%s %i '%s' '%s'\n", __FUNCTION__, time_var, from, to);
	return generic_named_fn(time_var, from, to, months, 12, 3);
}

static int week_fn(const int time_var, const char *from, const char *to) {
	printf("%s %i %li %li\n", __FUNCTION__, time_var, (long) from, (long) to);
	return generic_fn(time_var, (long) from, (long) to, 1, 6);
}

static int mday_fn(const int time_var, const char *from, const char *to) {
	printf("%s %i %li %li\n", __FUNCTION__, time_var, (long) from, (long) to);
	return generic_fn(time_var, (long) from, (long) to, 1, 31);
}

static int wday_fn(const int time_var, const char *from, const char *to) {
	printf("%s %i '%s' '%s'\n", __FUNCTION__, time_var, from, to);
	return generic_named_fn(time_var, from, to, weekdays, 7, 2);
}

static int yday_fn(const int time_var, const char *from, const char *to) {
	printf("%s %i %li %li\n", __FUNCTION__, time_var, (long) from, (long) to);
	return generic_fn(time_var, (long) from, (long) to, 1, 366);
}

static int hour_fn(const int time_var, const char *from, const char *to) {
	printf("%s %i %li %li\n", __FUNCTION__, time_var, (long) from, (long) to);
	return generic_fn(time_var, (long) from, (long) to, 0, 23);
}

static int minute_fn(const int time_var, const char *from, const char *to) {
	printf("%s %i %li %li\n", __FUNCTION__, time_var, (long) from, (long) to);
	return generic_fn(time_var, (long) from, (long) to, 0, 59);
}

static int second_fn(const int time_var, const char *from, const char *to) {
	printf("%s %i %li %li\n", __FUNCTION__, time_var, (long) from, (long) to);
	return generic_fn(time_var, (long) from, (long) to, 0, 60); /* allow for leap seconds */
}
