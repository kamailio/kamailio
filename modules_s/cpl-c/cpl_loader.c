/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *
 * History:
 * -------
 * 2003-06-24: file created (bogdan)
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../fifo_server.h"
#include "../../mem/mem.h"
#include "cpl_db.h"
#include "cpl_parser.h"
#include "cpl_loader.h"


#define MAX_STAIC_BUF 256

extern char *dtd_file;
extern db_con_t* db_hdl;

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
		LOG(L_ERR,"ERROR:cpl-c:load_file: cannot go to begining (lseek):"
			" %s\n",strerror(errno));
		goto error;
	}

	/* get some memory */
	xml->s = (char*)pkg_malloc( xml->len);
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
		LOG(L_ERR,"ERROR:cpl:load_file: couldn't read all file!\n");
		goto error;
	}

	return 1;
error:
	if (xml->s) pkg_free( xml->s);
	return -1;
}






int cpl_loader( FILE *fifo_stream, char *response_file )
{
	static char user[MAX_STAIC_BUF];
	static char cpl_file[MAX_STAIC_BUF];
	int user_len;
	int cpl_file_len;
	str xml;
	str bin;

	DBG("DEBUG:cpl_loader: FIFO commnad received!\n");

	/* first line must be the username */
	if (read_line( user, MAX_STAIC_BUF-1 , fifo_stream, &user_len )!=1 ||
	user_len<=0) {
		LOG(L_ERR,"ERROR:cpl:cpl_loader: unable to read username from "
			"FIFO command\n");
		goto error;
	}
	user[user_len] = 0;
	DBG("DEBUG:cpl_loader: user=%.*s\n",user_len,user);

	/* second line must be the cpl file */
	if (read_line( cpl_file, MAX_STAIC_BUF-1, fifo_stream, &cpl_file_len)!=1 ||
	cpl_file_len<=0) {
		LOG(L_ERR,"ERROR:cpl:cpl_loader: unable to read cpl_file name from "
			"FIFO command\n");
		goto error;
	}
	cpl_file[cpl_file_len] = 0;
	DBG("DEBUG:cpl_loader: cpl file=%.*s\n",cpl_file_len,cpl_file);

	/* load the xml file */
	if (load_file( cpl_file, &xml)!=1)
		return -1;

	/* get the binary coding for the XML file */
	if ( encodeXML( &xml, dtd_file , &bin)!=1)
		return -1;

	/* write both the XML and binary formats into database */
	if (write_to_db( db_hdl, user, &xml, &bin)!=1)
		return -1;

	/* free the memory used for storing the cpl script in XML format */
	pkg_free( xml.s );

	//write_to_file("cript.ccc", bin.s, bin.len);

	return 1;
error:
	return -1;
}

#undef MAX_STATIC_BUF

