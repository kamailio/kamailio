/*
 * Regular expression functions
 *
 * Copyright (C) 2003 Juha Heinanen
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

/*!
 * \file 
 * \brief Regular Expression functions
 * Copyright (C) 2003 Juha Heinanen
 * \ingroup libkcore
 */

#include <sys/types.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include "regexp.h"
#include "../../dprint.h"

/*! \brief Replace in replacement tokens \\d with substrings of string pointed by
 * pmatch.
 */
int replace(regmatch_t* pmatch, char* string, char* replacement, str* result)
{
	int len, i, j, digit, size;

	len = strlen(replacement);
	j = 0;

	for (i = 0; i < len; i++) {
		if (replacement[i] == '\\') {
			if (i < len - 1) {
				if (isdigit((unsigned char)replacement[i+1])) {
					digit = replacement[i+1] - '0';
					if (pmatch[digit].rm_so != -1) {
						size = pmatch[digit].rm_eo - pmatch[digit].rm_so;
						if (j + size < result->len) {
							memcpy(&(result->s[j]), string+pmatch[digit].rm_so, size);
							j = j + size;
						} else {
							return -1;
						}
					} else {
						return -2;
					}
					i = i + 1;
					continue;
				} else {
					i = i + 1;
				}
			} else {
				return -3;
			}
		}
		if (j + 1 < result->len) {
			result->s[j] = replacement[i];
			j = j + 1;
		} else {
			return -4;
		}
	}
	result->len = j;
	return 1;
}


/*! \brief Match pattern against string and store result in pmatch */
int reg_match(char *pattern, char *string, regmatch_t *pmatch)
{
	regex_t preg;

	if (regcomp(&preg, pattern, REG_EXTENDED | REG_NEWLINE)) {
		return -1;
	}
	if (preg.re_nsub > MAX_MATCH) {
		regfree(&preg);
		return -2;
	}
	if (regexec(&preg, string, MAX_MATCH, pmatch, 0)) {
		regfree(&preg);
		return -3;
	}
	regfree(&preg);
	return 0;
}


/*! \brief Match pattern against string and, if match succeeds, and replace string
 * with replacement substituting tokens \\d with matched substrings.
 */
int reg_replace(char *pattern, char *replacement, char *string, str *result)
{
	regmatch_t pmatch[MAX_MATCH];

	LM_DBG("pattern: '%s', replacement: '%s', string: '%s'\n",
	    pattern, replacement, string);

	if (reg_match(pattern, string, &(pmatch[0]))) {
		return -1;
	}

	return replace(&pmatch[0], string, replacement, result);

}
