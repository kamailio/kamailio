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
