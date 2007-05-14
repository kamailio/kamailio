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


#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <time.h>
#include <sys/utsname.h> /* uname() */


#include "ut.h"
#include "mem/mem.h"


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



/* return system version (major.minor.minor2) as
 *  (major<<16)|(minor)<<8|(minor2)
 * (if some of them are missing, they are set to 0)
 * if the parameters are not null they are set to the coresp. part 
 */
unsigned int get_sys_version(int* major, int* minor, int* minor2)
{
	struct utsname un;
	int m1;
	int m2;
	int m3;
	char* p;
	
	memset (&un, 0, sizeof(un));
	m1=m2=m3=0;
	/* get sys version */
	uname(&un);
	m1=strtol(un.release, &p, 10);
	if (*p=='.'){
		p++;
		m2=strtol(p, &p, 10);
		if (*p=='.'){
			p++;
			m3=strtol(p, &p, 10);
		}
	}
	if (major) *major=m1;
	if (minor) *minor=m2;
	if (minor2) *minor2=m3;
	return ((m1<<16)|(m2<<8)|(m3));
}



