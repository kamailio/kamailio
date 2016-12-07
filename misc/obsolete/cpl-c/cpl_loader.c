/*
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * History:
 * -------
 * 2003-08-21: cpl_remove() added (bogdan)
 * 2003-06-24: file created (bogdan)
 */


#include <stdio.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "cpl_db.h"
#include "cpl_parser.h"
#include "cpl_loader.h"


#define MAX_STATIC_BUF 256

extern db_con_t* db_hdl;



#if 0
/* debug function -> write into a file the content of a str struct. */
int write_to_file(char *filename, str *buf)
{
	int fd;
	int ret;

	fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
	if (!fd) {
		LOG(L_ERR,"ERROR:cpl-c:write_to_file: cannot open file : %s\n",
			strerror(errno));
		goto error;
	}

	while ( (ret=write( fd, buf->s, buf->len))!=buf->len) {
		if ((ret==-1 && errno!=EINTR)|| ret!=-1) {
			LOG(L_ERR,"ERROR:cpl-c:write_to_file:cannot write to file:"
				"%s write_ret=%d\n",strerror(errno), ret );
			goto error;
		}
	}
	close(fd);

	return 0;
error:
	return -1;
}
#endif



/* Loads a file into a buffer; first the file length will be determined for
 * allocated an exact buffer len for storing the file content into.
 * Returns:  1 - success
 *          -1 - error
 */
int load_file( char *filename, str *xml)
{
	int n;
	int offset;
	int fd;

	xml->s = 0;
	xml->len = 0;

	/* open the file for reading */
	fd = open(filename,O_RDONLY);
	if (fd==-1) {
		LOG(L_ERR,"ERROR:cpl-c:load_file: cannot open file for reading:"
			" %s\n",strerror(errno));
		goto error;
	}

	/* get the file length */
	if ( (xml->len=lseek(fd,0,SEEK_END))==-1) {
		LOG(L_ERR,"ERROR:cpl-c:load_file: cannot get file length (lseek):"
			" %s\n", strerror(errno));
		goto error;
	}
	DBG("DEBUG:cpl-c:load_file: file size = %d\n",xml->len);
	if ( lseek(fd,0,SEEK_SET)==-1 ) {
		LOG(L_ERR,"ERROR:cpl-c:load_file: cannot go to beginning (lseek):"
			" %s\n",strerror(errno));
		goto error;
	}

	/* get some memory */
	xml->s = (char*)pkg_malloc( xml->len+1/*null terminated*/ );
	if (!xml->s) {
		LOG(L_ERR,"ERROR:cpl-c:load_file: no more free pkg memory\n");
		goto error;
	}

	/*start reading */
	offset = 0;
	while ( offset<xml->len ) {
		n=read( fd, xml->s+offset, xml->len-offset);
		if (n==-1) {
			if (errno!=EINTR) {
				LOG(L_ERR,"ERROR:cpl-c:load_file: read failed:"
					" %s\n", strerror(errno));
				goto error;
			}
		} else {
			if (n==0) break;
			offset += n;
		}
	}
	if (xml->len!=offset) {
		LOG(L_ERR,"ERROR:cpl-c:load_file: couldn't read all file!\n");
		goto error;
	}
	xml->s[xml->len] = 0;

	close(fd);
	return 1;
error:
	if (fd!=-1) close(fd);
	if (xml->s) pkg_free( xml->s);
	return -1;
}



/* Writes an array of texts into the given response file.
 * Accepts also empty texts, case in which it will be created an empty
 * response file.
 */
void write_to_file( char *file, str *txt, int n )
{
	int fd;

	/* open file for write */
	fd = open( file, O_WRONLY|O_CREAT|O_TRUNC/*|O_NOFOLLOW*/, 0600 );
	if (fd==-1) {
		LOG(L_ERR,"ERROR:cpl-c:write_to_file: cannot open response file "
			"<%s>: %s\n", file, strerror(errno));
		return;
	}

	/* write the txt, if any */
	if (n>0) {
again:
		if ( writev( fd, (struct iovec*)txt, n)==-1) {
			if (errno==EINTR) {
				goto again;
			} else {
				LOG(L_ERR,"ERROR:cpl-c:write_logs_to_file: writev failed: "
					"%s\n", strerror(errno) );
			}
		}
	}

	/* close the file*/
	close( fd );
	return;
}



static inline int check_userhost( char *p, char *end)
{
	char *p1;
	int  dot;

	/* parse user name */
	p1 = p;
	while (p<end && (isalnum((int)*p) || *p=='-' || *p=='_' || *p=='.' ))
		p++;
	if (p==p1 || p==end || *p!='@')
		return -1;
	p++;
	/* parse the host part */
	dot = 1;
	p1 = p;
	while (p<end) {
		if (*p=='.') {
			if (dot) return -1; /* dot after dot */
			dot = 1;
		} else if (isalnum((int)*p) || *p=='-' || *p=='_' ) {
			dot = 0;
		} else {
			return -1;
		}
		p++;
	}
	if (p1==p || dot)
		return -1;

	return 0;
}

#undef MAX_STATIC_BUF

