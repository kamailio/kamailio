/*
 *
 * $Id$
 *
 */

#include "../../fifo_server.h"
#include "../../dprint.h"
#include "ul_fifo.h"
#include <string.h>
#include <stdio.h>
#include "dlist.h"
#include "udomain.h"
#include "utime.h"


#define MAX_CONTACT 128
#define MAX_EXPIRES 20
#define MAX_Q 20


/*
 * Dedicated to Douglas Adams, don't panic !
 */
#define FIFO_CALLID "The-Answer-To-The-Ultimate-Question-Of-Life-Universe-And-Everything"
#define FIFO_CSEQ 42 

#define FIFO_CALLID_LEN 67


/*
 * ASCII to integer
 */
static inline int atoi(str* _s, int* _r)
{
	int i;
	
	*_r = 0;
	for(i = 0; i < _s->len; i++) {
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			*_r *= 10;
			*_r += _s->s[i] - '0';
		} else {
			return -1;
		}
	}
	
	return 0;
}


/*
 * ASCII to float
 */
static inline int atof(str* _s, float* _r)
{
	int i, dot = 0;
	float order = 0.1;

	*_r = 0;
	for(i = 0; i < _s->len; i++) {
		if (_s->s[i] == '.') {
			if (dot) return -1;
			dot = 1;
			continue;
		}
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			if (dot) {
				*_r += (_s->s[i] - '0') * order;
				order /= 10;
			} else {
				*_r *= 10;
				*_r += _s->s[i] - '0';
			}
		} else {
			return -2;
		}
	}
	return 0;
}



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
	fifo_reply(response_file, "ul_flush completed" );
	return 1;
}


static inline void find_domain(str* _name, udomain_t** _d)
{
	dlist_t* ptr;

	ptr = root;
	while(ptr) {
		if ((ptr->name.len == _name->len) &&
		    !memcmp(ptr->name.s, _name->s, _name->len)) {
			break;
		}
		ptr = ptr->next;
	}
	
	if (ptr) {
		*_d = ptr->d;
	} else {
		*_d = 0;
	}
}


static inline int add_contact(udomain_t* _d, str* _u, str* _c, time_t _e, float _q)
{
	urecord_t* r;
	ucontact_t* c = 0;
	int res;
	str cid;
	
	if (_e == 0) {
		LOG(L_ERR, "fifo_add_contact(): expires == 0, giving up\n");
		return -1;
	}

	get_act_time();

	res = get_urecord(_d, _u, &r);
	if (res < 0) {
		LOG(L_ERR, "fifo_add_contact(): Error while getting record\n");
		return -2;
	}

	if (res >  0) { /* Record not found */
		if (insert_urecord(_d, _u, &r) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while creating new urecord\n");
			return -3;
		}
	} else {
		if (get_ucontact(r, _c, &c) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while obtaining ucontact\n");
			return -4;
		}
	}
		
	cid.s = FIFO_CALLID;
	cid.len = FIFO_CALLID_LEN;

	if (c) {
		if (update_ucontact(c, _e + act_time, _q, &cid, FIFO_CSEQ) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while updating contact\n");
			release_urecord(r);
			return -5;
		}
	} else {
		if (insert_ucontact(r, _c, _e + act_time, _q, &cid, FIFO_CSEQ, &c) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while inserting contact\n");
			release_urecord(r);
			return -6;
		}
	}
	
	release_urecord(r);
	return 0;
}


static int ul_add(FILE* pipe, char* response_file)
{
	/* FILE* reply_file; */
	char table_s[MAX_TABLE];
	char user_s[MAX_USER];
	char contact_s[MAX_CONTACT];
	char expires_s[MAX_EXPIRES];
	char q_s[MAX_Q];
	int tlen;
	udomain_t* d;
	float q_f;
	int exp_i;

	str table, user, contact, expires, q;

	if (!read_line(table_s, MAX_TABLE, pipe, &tlen) || tlen == 0) {
		fifo_reply(response_file,
			"ERROR: ul_add: table name expected\n");
		LOG(L_ERR, "ERROR: ul_add: table name expected\n");
		return -1;
	}
	
	if (!read_line(user_s, MAX_USER, pipe, &tlen) || tlen == 0) {
		fifo_reply(response_file,
			"ERROR: ul_add: aor name expected\n");
		LOG(L_ERR, "ERROR: ul_add: aor expected\n");
		return -1;
	}
	
	if (!read_line(contact_s, MAX_CONTACT, pipe, &tlen) || tlen == 0) {
		fifo_reply(response_file,
			"ERROR: ul_add: contact expected\n");
		LOG(L_ERR, "ERROR: ul_add: contact expected\n");
		return -1;
	}
	
	if (!read_line(expires_s, MAX_EXPIRES, pipe, &tlen) || tlen == 0) {
		fifo_reply(response_file,
			"ERROR: ul_add: expires expected\n");
		LOG(L_ERR, "ERROR: ul_add: expires expected\n");
		return -1;
	}
	
	if (!read_line(q_s, MAX_Q, pipe, &tlen) || tlen == 0) {
		fifo_reply(response_file,
			"ERROR: ul_add: q expected\n");
		LOG(L_ERR, "ERROR: ul_add: q expected\n");
		return -1;
	}
	
	table.s = table_s;
	table.len = strlen(table_s);

	user.s = user_s;
	user.len = strlen(user_s);
	
	contact.s = contact_s;
	contact.len = strlen(contact_s);

	expires.s = expires_s;
	expires.len = strlen(expires_s);

	q.s = q_s;
	q.len = strlen(q_s);
	
	find_domain(&table, &d);

	/*
	if (reply_file==0) {
		LOG(L_ERR, "ERROR: ul_add: file not opened\n");
		return -1;
	}
	*/

	if (d) {
		if (atoi(&expires, &exp_i) < 0) {
			fifo_reply(response_file, "Invalid expires format\n");
			/* fprintf(reply_file, "Invalid expires format\n");
			fclose(reply_file); */
			return -1;
		}

		if (atof(&q, &q_f) < 0) {
			fifo_reply(response_file, "Invalid q format\n");
			/* fprintf(reply_file, "Invalid q format\n");
			fclose(reply_file); */
			return -1;
		}

		lock_udomain(d);

		if (add_contact(d, &user, &contact, exp_i, q_f) < 0) {
			unlock_udomain(d);
			LOG(L_ERR, "ul_add(): Error while adding contact (\'%.*s\',\'%.*s\') in table \'%.*s\'\n",
			    user.len, user.s, contact.len, contact.s, table.len, table.s);
			/* fprintf(reply_file, "Error while adding contact (\'%.*s\',\'%.*s\') in table \'%.*s\'\n",
				user.len, user.s, contact.len, contact.s, table.len, table.s);
			fclose(reply_file); */
			fifo_reply(response_file, "Error while adding contact (\'%.*s\',\'%.*s\') in table \'%.*s\'\n",
				user.len, user.s, contact.len, contact.s, table.len, table.s);
			return -1;
		}
		unlock_udomain(d);
		
		/* fprintf(reply_file, "(\'%.*s\',\'%.*s\') Added to table \'%.*s\'\n",
			user.len, user.s, contact.len, contact.s, table.len, table.s);
		fclose(reply_file); */
		fifo_reply(response_file, "Added to table \'%.*s\'\n",
			user.len, user.s, contact.len, contact.s, table.len, table.s);
		return 1;
	} else {
		/* fprintf(reply_file, "Table \'%.*s\' not found\n", table.len, table.s);
		fclose(reply_file); */
		fifo_reply(response_file, "Table \'%.*s\' Not Found\n", table.len, table.s);
		return -1;
	}
}


int static ul_rm( FILE *pipe, char *response_file )
{
	char table[MAX_TABLE];
	char user[MAX_USER];
	int tlen, ulen;
	/* FILE *reply_file; */
	udomain_t* d;
	str aor, t;

	if (!read_line(table, MAX_TABLE, pipe, &tlen) || tlen==0) {
		fifo_reply(response_file, 
			"ERROR: ul_rm: table name expected\n");
		LOG(L_ERR, "ERROR: ul_rm: table name expected\n");
		return -1;
	}
	if (!read_line(user, MAX_TABLE, pipe, &ulen) || ulen==0) {
		fifo_reply(response_file, 
			"ERROR: ul_rm: user name expected\n");
		LOG(L_ERR, "ERROR: ul_rm: user name expected\n");
		return -1;
	}

	aor.s = user;
	aor.len = strlen(user);

	t.s = table;
	t.len = strlen(table);

	find_domain(&t, &d);

	LOG(L_INFO, "INFO: deleting user-loc (%s,%s)\n",
	    table, user );

	if (d) {
		lock_udomain(d);
		if (delete_urecord(d, &aor) < 0) {
			LOG(L_ERR, "ul_rm(): Error while deleting user %s\n", user);
			unlock_udomain(d);
			fifo_reply(response_file, "Error while deleting user %s\n", user);
			/* fprintf(reply_file, "Error while deleting user (%s, %s)\n", table, user); 
			fclose(reply_file); */
			return -1;
		}
		unlock_udomain(d);
		fifo_reply(response_file, "user (%s, %s) deleted\n", table, user);
		/* fprintf(reply_file, "User (%s, %s) deleted\n", table, user);
		fclose(reply_file); */
		return 1;
	} else {
		fifo_reply(response_file, "table (%s) not found\n", table);
		/* fprintf(reply_file, "Table (%s) not found\n", table);
		fclose(reply_file); */
		return -1;
	}
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

	if (register_fifo_cmd(ul_add, UL_ADD, 0)<0) {
		LOG(L_CRIT, "cannot register ul_add\n");
		return -1;
	}

	return 1;
}
