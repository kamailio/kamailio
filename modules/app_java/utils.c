/**
 * $Id$
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "utils.h"

char **split(char *str, char *sep)
{
    char **buf = NULL;
    char *token = NULL;
    char *saveptr = NULL;
    int i;

    buf = (char **)calloc(1, sizeof(char *));
    if (!buf)
    {
	return '\0';
    }

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

	buf = (char **)realloc(buf, (i+1) * sizeof(char *));
        buf[i] = strdup(token);
    }
    buf[i] = '\0';

    free(token);

    return buf;
}


char *rand_string(const int len)
{
    char *buf;
    const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int i;

    buf = (char *)calloc(len+1, sizeof(char));
    if (!buf)
	return NULL;

    srand(time(NULL));
    for (i=0; i<len; i++)
    {
        buf[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    buf[len] = '\0';

    return buf;
}

char *str_replace(char *original, char *pattern, char *replacement)
{
    char *oriptr, *patloc, *retptr, *returned;
    size_t replen, patlen, orilen, patcnt, retlen, skplen;

    replen = strlen(replacement);
    patlen = strlen(pattern);
    orilen = strlen(original);
    patcnt = 0;

    // find how many times the pattern occurs in the original string
    for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
    {
	patcnt++;
    }

    // allocate memory for the new string
    retlen = orilen + patcnt * (replen - patlen);
    returned = (char *)malloc((retlen+1) * sizeof(char));

    if (returned != NULL)
    {
	// copy the original string, 
	// replacing all the instances of the pattern
	retptr = returned;
	for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
	{
    	    skplen = patloc - oriptr;

    	    // copy the section until the occurence of the pattern
    	    strncpy(retptr, oriptr, skplen);
    	    retptr += skplen;

    	    // copy the replacement 
    	    strncpy(retptr, replacement, replen);
    	    retptr += replen;
	}

	// copy the rest of the string.
	strcpy(retptr, oriptr);
    }

    return returned;
}
