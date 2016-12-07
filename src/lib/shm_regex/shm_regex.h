/*
 * Copyright (C) 2009 iptelorg GmbH
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

#ifndef _SHM_REGEX_H
#define _SHM_REGEX_H

#include <sys/types.h>
#include <regex.h>
#include "locking.h"

typedef struct shm_regex {
	regex_t regexp;
	gen_lock_t lock;
} shm_regex_t;

int shm_regcomp(shm_regex_t *preg, const char *regex, int cflags);
void shm_regfree(shm_regex_t *preg);
int shm_regexec(shm_regex_t *preg, const char *string, size_t nmatch,
                   regmatch_t pmatch[], int eflags);
size_t shm_regerror(int errcode, const shm_regex_t *preg, char *errbuf,
                      size_t errbuf_size);

#endif /* _SHM_REGEX_H */
