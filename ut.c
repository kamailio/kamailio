/*
 *$Id$
 *
 * various general purpose functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define _GNU_SOURCE 1 /* strndup in get_abs_pathname */
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <libgen.h>
#include <time.h>
#include "ut.h"
#include "mem/mem.h"
#include "globals.h"

/* converts a username into uid:gid,
 * returns -1 on error & 0 on success */
int user2uid(int* uid, int* gid, char* user)
{
	char* tmp;
	struct passwd *pw_entry;
	
	if (user){
		*uid=strtol(user, &tmp, 10);
		if ((tmp==0) ||(*tmp)){
			/* maybe it's a string */
			pw_entry=getpwnam(user);
			if (pw_entry==0){
				goto error;
			}
			*uid=pw_entry->pw_uid;
			if (gid) *gid=pw_entry->pw_gid;
		}
		return 0;
	}
error:
	return -1;
}



/* converts a group name into a gid
 * returns -1 on error, 0 on success */
int group2gid(int* gid, char* group)
{
	char* tmp;
	struct group  *gr_entry;
	
	if (group){
		*gid=strtol(group, &tmp, 10);
		if ((tmp==0) ||(*tmp)){
			/* maybe it's a string */
			gr_entry=getgrnam(group);
			if (gr_entry==0){
				goto error;
			}
			*gid=gr_entry->gr_gid;
		}
		return 0;
	}
error:
	return -1;
}


/*
 * Replacement of timegm (does not exists on all platforms
 * Taken from 
 * http://lists.samba.org/archive/samba-technical/2002-November/025737.html
 */
time_t _timegm(struct tm* t)
{
	time_t tl, tb;
	struct tm* tg;

	t->tm_isdst = 0;
	tl = mktime(t);
	if (tl == -1) {
		t->tm_hour--;
		tl = mktime (t);
		if (tl == -1) {
			return -1; /* can't deal with output from strptime */
		}
		tl += 3600;
	}
	
	tg = gmtime(&tl);
	tg->tm_isdst = 0;
	tb = mktime(tg);
	if (tb == -1) {
		tg->tm_hour--;
		tb = mktime (tg);
		if (tb == -1) {
			return -1; /* can't deal with output from gmtime */
		}
		tb += 3600;
	}
	return (tl - (tb - tl));
}


/*
 * Return str as zero terminated string allocated
 * using pkg_malloc
 */
char* as_asciiz(str* s)
{
    char* r;

    r = (char*)pkg_malloc(s->len + 1);
    if (!r) {
		ERR("Out of memory\n");
		return 0;
    }
    memcpy(r, s->s, s->len);
    r[s->len] = '\0';
    return r;
}


char* get_abs_pathname(str* base, str* file)
{
	str ser_cfg;
	char* buf, *dir, *res;
	int len;

	if (base == NULL) {
		ser_cfg.s = cfg_file;
		ser_cfg.len = strlen(cfg_file);
		base = &ser_cfg;
	}

	if (!base->s || base->len <= 0 || base->s[0] != '/') {
		BUG("get_abs_pathname: Base file must be absolute pathname: "
			"'%.*s'\n", STR_FMT(base));
		return NULL;
	}

	if (!file || !file->s || file->len <= 0) {
		BUG("get_abs_pathname: Invalid 'file' parameter\n");
		return NULL;
	}

	if (file->s[0] == '/') {
		/* This is an absolute pathname, make a zero terminated
		 * copy and use it as it is */
		if ((res = strndup(file->s, file->len)) == NULL) {
			ERR("get_abs_pathname: No memory left (strndup failed)\n");
		}
	} else {
		/* This is not an absolute pathname, make it relative
		 * to the location of the base file
		 */
		/* Make a copy, function dirname may modify the string */
		if ((buf = strndup(base->s, base->len)) == NULL) {
			ERR("get_abs_pathname: No memory left (strdup failed)\n");
			return NULL;
		}
		dir = dirname(buf);

		len = strlen(dir);
		if ((res = malloc(len + 1 + file->len + 1)) == NULL) {
			ERR("get_abs_pathname: No memory left (malloc failed)\n");
			free(buf);
			return NULL;
		}
		memcpy(res, dir, len);
		res[len] = '/';
		memcpy(res + len + 1, file->s, file->len);
		res[len + 1 + file->len] = '\0';
		free(buf);
	}
	return res;
}
