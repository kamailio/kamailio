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
#include "dlist.h"
#include "udomain.h"


static int print_ul_stats(FILE *reply_file)
{
	dlist_t* ptr;
	
	fprintf(reply_file, "Domain Registered Expired\n");
	
	ptr = root;
	while(ptr) {

		fprintf(reply_file, "\'%.*s\' %d %d\n",
			ptr->d->name->len, ptr->d->name->s,
			ptr->d->users,
			ptr->d->expired
			);
		ptr = ptr->next;
	}

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


int static ul_dump(FILE* pipe, char* response_file)
{
	FILE* reply_file;

	reply_file=open_reply_pipe(response_file);
	if (reply_file==0) {
		LOG(L_ERR, "ERROR: ul_dump: file not opened\n");
		return -1;
	}
	print_all_udomains(reply_file);
	fclose(reply_file);
	return 1;
}

int static ul_flush(FILE* pipe, char* response_file)
{
	timer_handler();
	return 1;
}


int static ul_rm( FILE *pipe, char *response_file )
{
	char table[MAX_TABLE];
	char user[MAX_USER];
	int tlen, ulen;
	FILE *reply_file;
	dlist_t* ptr;
	str aor, t;

	if (!read_line(table, MAX_TABLE, pipe, &tlen) || tlen==0) {
		LOG(L_ERR, "ERROR: ul_rm: table name expected\n");
		return -1;
	}
	if (!read_line(user, MAX_TABLE, pipe, &ulen) || ulen==0) {
		LOG(L_ERR, "ERROR: ul_rm: user name expected\n");
		return -1;
	}
	/* PLACEHOLDER: fill in real things here */

	aor.s = user;
	aor.len = strlen(user);

	t.s = table;
	t.len = strlen(table);

	ptr = root;
	while(ptr) {
		if ((ptr->name.len == t.len) &&
		    !memcmp(ptr->name.s, t.s, t.len)) {
			break;
		}
		ptr = ptr->next;
	}


	LOG(L_INFO, "INFO: deleting user-loc (%s,%s)\n",
	    table, user );
	reply_file=open_reply_pipe(response_file);
	if (reply_file==0) {
		LOG(L_ERR, "ERROR: ul_rm: file not opened\n");
		return -1;
	}

	if (ptr) {
		lock_udomain(ptr->d);
		if (delete_urecord(ptr->d, &aor) < 0) {
			LOG(L_ERR, "ul_rm(): Error while deleting user %s\n", user);
			fprintf(reply_file, "Error while deleting user (%s, %s)\n", table, user);
			unlock_udomain(ptr->d);
			fclose(reply_file);
			return -1;
		}
		unlock_udomain(ptr->d);
		fprintf(reply_file, "User (%s, %s) deleted\n", table, user);
	} else {
		fprintf(reply_file, "Table (%s) not found\n", table);
	}
	
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
	if (register_fifo_cmd(ul_dump, UL_DUMP, 0)<0) {
		LOG(L_CRIT, "cannot register ul_dump\n");
		return -1;
	}
	if (register_fifo_cmd(ul_flush, UL_FLUSH, 0)<0) {
		LOG(L_CRIT, "cannot register ul_flush\n");
		return -1;
	}
	return 1;
}
