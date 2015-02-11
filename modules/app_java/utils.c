/*
 * Copyright (C) 2013 Konstantin Mosesov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../sr_module.h"

#include <jni.h>

#include "global.h"
#include "utils.h"
#include "app_java_mod.h"
#include "java_iface.h"
#include "java_support.h"
#include "java_native_methods.h"
#include "java_sig_parser.h"


char **split(char *str, char *sep)
{
	char **buf = NULL;
	char *token = NULL;
	char *saveptr = NULL;
	int i;

	buf = (char **)pkg_malloc(sizeof(char *));
	if (!buf)
	{
		LM_ERR("%s: pkg_malloc() has failed. Not enough memory!\n", APP_NAME);
		return NULL;
	}
	memset(&buf, 0, sizeof(char *));

	if (str == NULL)
		return buf;

	if (strncmp(str, sep, strlen(sep)) <= 0)
	{
		// string doesn't contains a separator
		buf[0] = strdup(str);
		return buf;
	}

	token = strdup(str);
	for (i=0; token != NULL; token = saveptr, i++)
	{
		token = strtok_r(token, (const char *)sep, &saveptr);

		if (token == NULL || !strcmp(token, ""))
			break;

		buf = (char **)pkg_realloc(buf, (i+2) * sizeof(char *));
		if (!buf)
		{
			LM_ERR("%s: pkg_realloc() has failed. Not enough memory!\n", APP_NAME);
			return NULL;
		}
		buf[i] = strdup(token);
	}
	buf[i] = NULL;

	free(token);

	return buf;
}

