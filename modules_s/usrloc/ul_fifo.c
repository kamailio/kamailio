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
	synchronize_all_udomains();
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
	char table_s[MAX_TABLE];
	char user_s[MAX_USER];
	char contact_s[MAX_CONTACT];
	char expires_s[MAX_EXPIRES];
	char q_s[MAX_Q];
	udomain_t* d;
	float q_f;
	int exp_i;

	str table, user, contact, expires, q;

	if (!read_line(table_s, MAX_TABLE, pipe, &table.len) || table.len == 0) {
		fifo_reply(response_file,
			   "ERROR: ul_add: table name expected\n");
		LOG(L_ERR, "ERROR: ul_add: table name expected\n");
		return -1;
	}
	
	if (!read_line(user_s, MAX_USER, pipe, &user.len) || user.len  == 0) {
		fifo_reply(response_file,
			   "ERROR: ul_add: aor name expected\n");
		LOG(L_ERR, "ERROR: ul_add: aor expected\n");
		return -1;
	}
	
	if (!read_line(contact_s, MAX_CONTACT, pipe, &contact.len) || contact.len == 0) {
		fifo_reply(response_file,
			   "ERROR: ul_add: contact expected\n");
		LOG(L_ERR, "ERROR: ul_add: contact expected\n");
		return -1;
	}
	
	if (!read_line(expires_s, MAX_EXPIRES, pipe, &expires.len) || expires.len == 0) {
		fifo_reply(response_file,
			   "ERROR: ul_add: expires expected\n");
		LOG(L_ERR, "ERROR: ul_add: expires expected\n");
		return -1;
	}
	
	if (!read_line(q_s, MAX_Q, pipe, &q.len) || q.len == 0) {
		fifo_reply(response_file,
			   "ERROR: ul_add: q expected\n");
		LOG(L_ERR, "ERROR: ul_add: q expected\n");
		return -1;
	}
	
	table.s = table_s;
	user.s = user_s;
	contact.s = contact_s;
	expires.s = expires_s;
	q.s = q_s;
	
	find_domain(&table, &d);
	
	if (d) {
		if (atoi(&expires, &exp_i) < 0) {
			fifo_reply(response_file, "Invalid expires format\n");
			return -1;
		}
		
		if (atof(&q, &q_f) < 0) {
			fifo_reply(response_file, "Invalid q format\n");
			return -1;
		}

		lock_udomain(d);
		
		if (add_contact(d, &user, &contact, exp_i, q_f) < 0) {
			unlock_udomain(d);
			LOG(L_ERR, "ul_add(): Error while adding contact (\'%.*s\',\'%.*s\') in table \'%.*s\'\n",
			    user.len, user.s, contact.len, contact.s, table.len, table.s);
			fifo_reply(response_file, "Error while adding contact (\'%.*s\',\'%.*s\') in table \'%.*s\'\n",
				   user.len, user.s, contact.len, contact.s, table.len, table.s);
			return -1;
		}
		unlock_udomain(d);
		
		fifo_reply(response_file, "(\'%.*s\',\'%.*s\') Added to table \'%.*s\'\n",
			   user.len, user.s, contact.len, contact.s, table.len, table.s);
		return 1;
	} else {
		fifo_reply(response_file, "Table \'%.*s\' Not Found\n", table.len, table.s);
		return -1;
	}
}


int static ul_rm( FILE *pipe, char *response_file )
{
	char table[MAX_TABLE];
	char user[MAX_USER];
	udomain_t* d;
	str aor, t;

	if (!read_line(table, MAX_TABLE, pipe, &t.len) || t.len ==0) {
		fifo_reply(response_file, 
			   "ERROR: ul_rm: table name expected\n");
		LOG(L_ERR, "ERROR: ul_rm: table name expected\n");
		return -1;
	}
	if (!read_line(user, MAX_USER, pipe, &aor.len) || aor.len==0) {
		fifo_reply(response_file, 
			   "ERROR: ul_rm: user name expected\n");
		LOG(L_ERR, "ERROR: ul_rm: user name expected\n");
		return -1;
	}

	aor.s = user;
	t.s = table;

	find_domain(&t, &d);

	LOG(L_INFO, "INFO: deleting user-loc (%s,%s)\n",
	    table, user );
	
	if (d) {
		lock_udomain(d);
		if (delete_urecord(d, &aor) < 0) {
			LOG(L_ERR, "ul_rm(): Error while deleting user %s\n", user);
			unlock_udomain(d);
			fifo_reply(response_file, "Error while deleting user %s\n", user);
			return -1;
		}
		unlock_udomain(d);
		fifo_reply(response_file, "user (%s, %s) deleted\n", table, user);
		return 1;
	} else {
		fifo_reply(response_file, "table (%s) not found\n", table);
		return -1;
	}
}


static int ul_rm_contact(FILE* pipe, char* response_file)
{
	char table[MAX_TABLE];
	char user[MAX_USER];
	char contact[MAX_CONTACT];
	udomain_t* d;
	urecord_t* r;
	ucontact_t* con;
	str aor, t, c;
	int res;

	if (!read_line(table, MAX_TABLE, pipe, &t.len) || t.len ==0) {
		fifo_reply(response_file, 
			   "ERROR: ul_rm_contact: table name expected\n");
		LOG(L_ERR, "ERROR: ul_rm_contact: table name expected\n");
		return -1;
	}
	if (!read_line(user, MAX_USER, pipe, &aor.len) || aor.len==0) {
		fifo_reply(response_file, 
			   "ERROR: ul_rm_contact: user name expected\n");
		LOG(L_ERR, "ERROR: ul_rm_contact: user name expected\n");
		return -1;
	}

	if (!read_line(contact, MAX_CONTACT, pipe, &c.len) || c.len == 0) {
		fifo_reply(response_file,
			   "ERROR: ul_rm_contact: contact expected\n");
		LOG(L_ERR, "ERROR: ul_rm_contact: contact expected\n");
		return -1;
	}

	aor.s = user;
	t.s = table;
	c.s = contact;

	find_domain(&t, &d);

	LOG(L_INFO, "INFO: deleting user-loc contact (%s,%s,%s)\n",
	    table, user, contact );


	if (d) {
		lock_udomain(d);

		res = get_urecord(d, &aor, &r);
		if (res < 0) {
			fifo_reply(response_file, "ERROR: Error while looking for username %s in table %s\n", user, table);
			LOG(L_ERR, "ERROR: ul_rm_contact: Error while looking for username %s in table %s\n", user, table);
			unlock_udomain(d);
			return -1;
		}
		
		if (res > 0) {
			fifo_reply(response_file, "Username %s in table %s not found\n", user, table);
			unlock_udomain(d);
			return -1;
		}

		res = get_ucontact(r, &c, &con);
		if (res < 0) {
			fifo_reply(response_file, "ERROR: Error while looking for contact %s\n", contact);
			LOG(L_ERR, "ERROR: ul_rm_contact: Error while looking for contact %s\n", contact);
			unlock_udomain(d);
			return -1;
		}			

		if (res > 0) {
			fifo_reply(response_file, "Contact %s in table %s not found\n", contact, table);
			unlock_udomain(d);
			return -1;
		}

		if (delete_ucontact(r, con) < 0) {
			fifo_reply(response_file, "ERROR: ul_rm_contact: Error while deleting contact %s\n", contact);
			unlock_udomain(d);
			return -1;
		}

		release_urecord(r);
		unlock_udomain(d);
		fifo_reply(response_file, "Contact (%s, %s) deleted from table %s\n", user, contact, table);
		return 1;
	} else {
		fifo_reply(response_file, "table (%s) not found\n", table);
		return -1;
	}

}


/*
 * Build Contact HF for reply
 */
static inline int print_contacts(FILE* _o, ucontact_t* _c)
{
	int ok = 0;

	while(_c) {
		if (_c->expires > act_time) {
			ok = 1;
			fprintf(_o, "<%.*s>;q=%-3.2f;expires=%d\n",
				_c->c.len, _c->c.s,
				_c->q, (int)(_c->expires - act_time));
		}

		_c = _c->next;
	}

	return ok;
}


static inline int ul_show_contact(FILE* pipe, char* response_file)
{
	char table[MAX_TABLE];
	char user[MAX_USER];
	FILE* reply_file;
	udomain_t* d;
	urecord_t* r;
	int res;
	str t, aor;

	if (!read_line(table, MAX_TABLE, pipe, &t.len) || t.len ==0) {
		fifo_reply(response_file, 
			   "ERROR: ul_show_contact: table name expected\n");
		LOG(L_ERR, "ERROR: ul_show_contact: table name expected\n");
		return -1;
	}
	if (!read_line(user, MAX_USER, pipe, &aor.len) || aor.len==0) {
		fifo_reply(response_file, 
			   "ERROR: ul_show_contact: user name expected\n");
		LOG(L_ERR, "ERROR: ul_show_contact: user name expected\n");
		return -1;
	}

	aor.s = user;
	t.s = table;
	
	find_domain(&t, &d);

	if (d) {
		lock_udomain(d);	

		res = get_urecord(d, &aor, &r);
		if (res < 0) {
			fifo_reply(response_file, "ERROR: Error while looking for username %s in table %s\n", user, table);
			LOG(L_ERR, "ERROR: ul_show_contact: Error while looking for username %s in table %s\n", user, table);
			unlock_udomain(d);
			return -1;
		}
		
		if (res > 0) {
			fifo_reply(response_file, "Username %s in table %s not found\n", user, table);
			unlock_udomain(d);
			return -1;
		}
		
		get_act_time();

		reply_file=open_reply_pipe(response_file);
		if (reply_file==0) {
			LOG(L_ERR, "ERROR: ul_show_contact: file not opened\n");
			unlock_udomain(d);
			return -1;
		}

		if (!print_contacts(reply_file, r->contacts)) {
			unlock_udomain(d);
			fprintf(reply_file, "No registered contacts found\n");
			fclose(reply_file);
			return -1;
		}

		fclose(reply_file);
		unlock_udomain(d);
		return 1;
	} else {
		fifo_reply(response_file, "table (%s) not found\n", table);
		return -1;
	}
}



int init_ul_fifo( void ) 
{
	if (register_fifo_cmd(ul_stats_cmd, UL_STATS, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_stats\n");
		return -1;
	}

	if (register_fifo_cmd(ul_rm, UL_RM, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_rm\n");
		return -1;
	}

	if (register_fifo_cmd(ul_rm_contact, UL_RM_CONTACT, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_rm_contact\n");
		return -1;
	}
	

	if (register_fifo_cmd(ul_dump, UL_DUMP, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_dump\n");
		return -1;
	}

	if (register_fifo_cmd(ul_flush, UL_FLUSH, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_flush\n");
		return -1;
	}

	if (register_fifo_cmd(ul_add, UL_ADD, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_add\n");
		return -1;
	}

	if (register_fifo_cmd(ul_show_contact, UL_SHOW_CONTACT, 0) < 0) {
		LOG(L_CRIT, "cannot register ul_show_contact\n");
		return -1;
	}


	return 1;
}
