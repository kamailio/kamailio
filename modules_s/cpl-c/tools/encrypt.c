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
 */


#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "../cpl_db.h"
#include "../cpl_parser.h"

char *DB_URL = "sql://cpl:47cpl11@fox.iptel.org/jcidb";
char *DB_TABLE = "user";


int write_to_file(char *filename, char *s, int len)
{
	int fd;

	fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
	if (!fd) {
		printf("ERROR: cannot open file : %s",strerror(errno));
		goto error;
	}

	if (write( fd, s, len)!=len) {
		printf("ERROR: cannot write to file : %s",strerror(errno));
		goto error;
	}
	close(fd);

	return 0;
error:
	return -1;
}




unsigned char* load_file( char *filename, unsigned int *l)
{
	static char buf[4096];
	unsigned int len;
	int n;
	int fd;

	fd = open(filename,O_RDONLY);
	if (!fd) {
		printf("ERROR: cannot open file for reading: %s",strerror(errno));
		goto error;
	}

	len = 0;
	while ( (n=read(fd, buf+len, 256))>0 )
		len += n;

	if (l) *l=len;
	return buf;
error:
	return 0;
}




int main(int argc, char **argv)
{
	unsigned char *buf_txt;
	unsigned int  len_txt;
	unsigned char *buf_bin;
	unsigned int  len_bin;

	if (argc <= 3) {
		printf("Usage: %s user_name cpl_file dtd_file\n", argv[0]);
		return(0);
	}

	if ((buf_txt=load_file(argv[2], &len_txt))==0)
		return -1;

	if ((buf_bin=encryptXML(buf_txt, len_txt, argv[3], &len_bin))==0)
		return -1;

	if (write_to_db( argv[1], buf_bin, len_bin, buf_txt, len_txt)==-1)
		return -1;
	write_to_file("cript.ccc", buf_bin, len_bin);

	return 0;
}
