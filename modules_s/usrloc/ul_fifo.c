/*
 *
 * $Id$
 *
 */

#include "../../fifo_server.h"
#include "../../dprint.h"
#include "ul_fifo.h"
#include <strings.h>
#include <stdio.h>


static int print_ul_stats(FILE *reply_file)
{

	/* PLACEHOLDER: fill in real things here */
	fprintf(reply_file, "registered (now): %d, "
		"expired (since boot time): %d\n",
		0, 0 );
	return 1;
}

int static ul_stats_cmd( FILE *pipe, char *response_file )
{
	FILE *reply_file;

	reply_file=open_reply_pipe(response_file);
	if (reply_file==0) {
		LOG(L_ERR, "ERROR: ul_stats: file not opened\n");
		return -1;
	}
	print_ul_stats( reply_file );
	fclose(reply_file);
	return 1;
}

int static ul_rm( FILE *pipe, char *response_file )
{
	char table[MAX_TABLE];
	char user[MAX_USER];
	int tlen, ulen;
	FILE *reply_file;

	if (!read_line(table, MAX_TABLE, pipe, &tlen) || tlen==0) {
		LOG(L_ERR, "ERROR: ul_rm: table name expected\n");
		return -1;
	}
	if (!read_line(user, MAX_TABLE, pipe, &ulen) || ulen==0) {
		LOG(L_ERR, "ERROR: ul_rm: user name expected\n");
		return -1;
	}
	/* PLACEHOLDER: fill in real things here */
	LOG(L_INFO, "INFO: deleting user-loc (%s,%s)\n",
		table, user );
	reply_file=open_reply_pipe(response_file);
	if (reply_file==0) {
		LOG(L_ERR, "ERROR: ul_rm: file not opened\n");
		return -1;
	}
	fprintf(reply_file, "User (%s,%s) deletion not implemented\n",
		table, user);
	fclose(reply_file);
	return 1;
}

int init_ul_fifo( void ) 
{
	if (register_fifo_cmd(ul_stats_cmd, UL_STATS, 0)<0) {
		LOG(L_CRIT, "cannot register ul_stats\n");
		return -1;
	}
	if (register_fifo_cmd(ul_rm, UL_RM, 0)<0) {
		LOG(L_CRIT, "cannot register ul_rm\n");
		return -1;
	}
	return 1;
}
