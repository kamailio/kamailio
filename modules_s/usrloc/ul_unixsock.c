/*
 * $Id$
 *
 * Copyright (C) 2004 FhG FOKUS
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

#include "../../unixsock_server.h"
#include "../../ut.h"
#include "../../str.h"
#include "dlist.h"
#include "ul_mod.h"
#include "ul_fifo.h"
#include "utime.h"
#include "ul_unixsock.h"

/*
 * Dedicated to Douglas Adams, don't panic !
 */
#define UNIXSOCK_CALLID "The-Answer-To-The-Ultimate-Question-Of-Life-Universe-And-Everything"
#define UNIXSOCK_CALLID_LEN (sizeof(UNIXSOCK_CALLID)-1)
#define UNIXSOCK_CSEQ 42


static inline int add_contact(udomain_t* _d, str* _u, str* _c, time_t _e, float _q, int _r, int _f)
{
	urecord_t* r;
	ucontact_t* c = 0;
	int res;
	str cid;
	
	if (_e == 0 && !(_f & FL_PERMANENT)) {
		LOG(L_ERR, "fifo_add_contact(): expires == 0 and not persistent contact, giving up\n");
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
		
	cid.s = UNIXSOCK_CALLID;
	cid.len = UNIXSOCK_CALLID_LEN;

	if (c) {
		if (update_ucontact_rep(c, _e + act_time, _q, &cid, UNIXSOCK_CSEQ, _r, _f, FL_NONE) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while updating contact\n");
			release_urecord(r);
			return -5;
		}
	} else {
		if (insert_ucontact_rep(r, _c, _e + act_time, _q, &cid, UNIXSOCK_CSEQ, _f, _r, &c) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while inserting contact\n");
			release_urecord(r);
			return -6;
		}
	}
	
	release_urecord(r);
	return 0;
}


static inline void unixsock_find_domain(str* _name, udomain_t** _d)
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


static int ul_stats_cmd(str* msg)
{
	dlist_t* ptr;

	unixsock_reply_asciiz("200 OK\n");
	unixsock_reply_asciiz("Domain Registered Expired\n");
	
	ptr = root;
	while(ptr) {
		if (unixsock_reply_printf("'%.*s' %d %d\n",
					  ptr->d->name->len, ZSW(ptr->d->name->s),
					  ptr->d->users,
					  ptr->d->expired
					  ) < 0) {
			unixsock_reply_reset();
			unixsock_reply_asciiz("500 Buffer Too Small\n");
			unixsock_reply_send();
			return -1;
		}
		ptr = ptr->next;
	}
	
	unixsock_reply_send();
	return 0;
}


static int ul_rm(str* msg)
{
	udomain_t* d;
	str table, user;
	char* at;

	if (unixsock_read_line(&table, msg) != 0) {
		unixsock_reply_asciiz("400 Table name expected\n");
		goto err;
	}

	if (unixsock_read_line(&user, msg) != 0) {
		unixsock_reply_asciiz("400 User name expected\n");
		goto err;
	}

	at = q_memchr(user.s, '@', user.len);

	if (use_domain) {
		if (!at) {
			unixsock_reply_asciiz("400 Domain missing\n");
			goto err;
		}
	} else {
		if (at) {
			user.len = at - user.s;
		}
	}

	strlower(&user);

	unixsock_find_domain(&table, &d);

	LOG(L_INFO, "INFO: Deleting entry (%.*s,%.*s)\n",
	    table.len, ZSW(table.s), user.len, ZSW(user.s));
	
	if (d) {
		lock_udomain(d);
		if (delete_urecord(d, &user) < 0) {
			LOG(L_ERR, "ul_rm(): Error while deleting user %.*s\n", user.len, ZSW(user.s));
			unlock_udomain(d);
			unixsock_reply_printf("500 Error while deleting user %.*s\n", user.len, ZSW(user.s));
			goto err;
		}
		unlock_udomain(d);
		unixsock_reply_printf("200 user (%.*s, %.*s) deleted\n",
				      table.len, ZSW(table.s), user.len, ZSW(user.s));
		unixsock_reply_send();
		return 0;
	} else {
		unixsock_reply_printf("400 table (%.*s) not found\n", table.len, ZSW(table.s));
		return 0;
	}
 err:
	unixsock_reply_send();
	return -1;
}


static int ul_rm_contact(str* msg)
{
	udomain_t* d;
	urecord_t* r;
	ucontact_t* con;
	str table, user, contact;
	int res;
	char* at;

	if (unixsock_read_line(&table, msg) != 0) {
		unixsock_reply_asciiz("400 Table name expected\n");
		goto err;
	}

	if (unixsock_read_line(&user, msg) != 0) {
		unixsock_reply_asciiz("400 Username expected\n");
		goto err;
	}

	at = q_memchr(user.s, '@', user.len);

	if (use_domain) {
		if (!at) {
			unixsock_reply_asciiz("400 Domain missing\n");
			goto err;
		}
	} else {
		if (at) {
			user.len = at - user.s;
		}
	}

	if (unixsock_read_line(&contact, msg) != 0) {
		unixsock_reply_asciiz("400 Contact expected\n");
		goto err;
	}

	strlower(&user);

	unixsock_find_domain(&table, &d);

	LOG(L_INFO, "INFO: Deleting contact (%.*s,%.*s,%.*s)\n",
	    table.len, ZSW(table.s), 
	    user.len, ZSW(user.s), 
	    contact.len, ZSW(contact.s));

	if (d) {
		lock_udomain(d);

		res = get_urecord(d, &user, &r);
		if (res < 0) {
			unixsock_reply_printf("500 Error while looking for username %.*s in table %.*s\n",
					      user.len, ZSW(user.s), table.len, ZSW(table.s));
			goto err_unlock;
		}
		
		if (res > 0) {
			unixsock_reply_printf("404 Username %.*s in table %.*s not found\n",
					      user.len, ZSW(user.s), table.len, ZSW(table.s));
			goto err_unlock;
		}

		res = get_ucontact(r, &contact, &con);
		if (res < 0) {
			unixsock_reply_printf("500 Error while looking for contact %.*s\n", contact.len, ZSW(contact.s));
			goto err_unlock;
		}			

		if (res > 0) {
			unixsock_reply_printf("404 Contact %.*s in table %.*s not found\n", 
					      contact.len, ZSW(contact.s), table.len, ZSW(table.s));
			goto err_unlock;
		}

		if (delete_ucontact(r, con) < 0) {
			unixsock_reply_printf("500 ul_rm_contact: Error while deleting contact %.*s\n", 
					      contact.len, ZSW(contact.s));
			goto err_unlock;
		}

		release_urecord(r);
		unlock_udomain(d);
		unixsock_reply_printf("200 Contact (%.*s, %.*s) deleted from table %.*s\n", 
				      user.len, ZSW(user.s), 
				      contact.len, ZSW(contact.s), 
				      table.len, ZSW(table.s));
		unixsock_reply_send();
		return 0;
	} else {
		unixsock_reply_printf("400 table (%.*s) not found\n", table.len, ZSW(table.s));
		goto err;
	}

 err_unlock:
	unlock_udomain(d);
 err:
	unixsock_reply_send();
	return -1;
}


static int ul_flush(str* msg)
{
	synchronize_all_udomains();
	unixsock_reply_printf("200 ul_flush completed\n");
	unixsock_reply_send();
	return 0;
}


static int ul_dump(str* msg)
{
	unixsock_reply_asciiz( "200 OK\n");
	     /* FIXME */
	return 0;
}


static int ul_add(str* msg)
{
	udomain_t* d;
	float q_f;
	int exp_i, rep_i, flags_i;
	char* at;
	str table, user, contact, expires, q, rep, flags;

	if (unixsock_read_line(&table, msg) != 0) {
		unixsock_reply_asciiz("400 Table name expected\n");
		goto err;
	}
	
	if (unixsock_read_line(&user, msg) != 0) {
		unixsock_reply_asciiz("400 AOR name expected\n");
		goto err;
	}

	at = q_memchr(user.s, '@', user.len);

	if (use_domain) {
		if (!at) {
			unixsock_reply_asciiz("400 Username@domain expected\n");
			goto err;
		}
	} else {
		if (at) {
			user.len = at - user.s;
		}
	}

	if (unixsock_read_line(&contact, msg) != 0) {
		unixsock_reply_asciiz("400 Contact expected\n");
		goto err;
	}
	
	if (unixsock_read_line(&expires, msg) != 0) {
		unixsock_reply_asciiz("400 Expires expected\n");
		goto err;
	}
	
	if (unixsock_read_line(&q, msg) != 0) {
		unixsock_reply_asciiz("400 q value expected\n");
		goto err;
	}
	
	if (unixsock_read_line(&rep, msg) != 0) {
		unixsock_reply_asciiz("400 Replicate expected\n");
		goto err;
	}

	if (unixsock_read_line(&flags, msg) != 0) {
		unixsock_reply_asciiz("400 Flags expected\n");
		goto err;
	}
	
	strlower(&user);
	unixsock_find_domain(&table, &d);
	
	if (d) {
		if (str2int(&expires, (unsigned int*)&exp_i) < 0) {
			unixsock_reply_asciiz("400 Invalid expires format\n");
			goto err;
		}
		
		if (str2float(&q, &q_f) < 0) {
			unixsock_reply_asciiz("400 Invalid q format\n");
			goto err;
		}

		if (str2int(&rep, (unsigned int*)&rep_i) < 0) {
			unixsock_reply_asciiz("400 Invalid replicate format\n");
			goto err;
		}

		if (str2int(&flags, (unsigned int*)&flags_i) < 0) {
			unixsock_reply_asciiz("400 Invalid flags format\n");
			goto err;
		}
		
		lock_udomain(d);
		
		if (add_contact(d, &user, &contact, exp_i, q_f, rep_i, flags_i) < 0) {
			unlock_udomain(d);
			LOG(L_ERR, "ul_add(): Error while adding contact ('%.*s','%.*s') in table '%.*s'\n",
			    user.len, ZSW(user.s), contact.len, ZSW(contact.s), table.len, ZSW(table.s));
			unixsock_reply_printf("500 Error while adding contact\n"
					      " ('%.*s','%.*s') in table '%.*s'\n",
					      user.len, ZSW(user.s), contact.len, ZSW(contact.s), table.len, ZSW(table.s));
			goto err;
		}
		unlock_udomain(d);
		
		unixsock_reply_printf("200 Added to table\n"
				      "('%.*s','%.*s') to '%.*s'\n",
				      user.len, ZSW(user.s), contact.len, ZSW(contact.s), table.len, ZSW(table.s));
		unixsock_reply_send();
		return 0;
	} else {
		unixsock_reply_printf("400 Table '%.*s' not found in memory, use save(\"%.*s\") or lookup(\"%.*s\") in the configuration script first\n", 
				      table.len, ZSW(table.s), table.len, ZSW(table.s), table.len, ZSW(table.s));
		unixsock_reply_send();
		return 0;
	}
 err:
	unixsock_reply_send();
	return -1;
}


static inline int ul_show_contact(str* msg)
{
	udomain_t* d;
	urecord_t* r;
	int res;
	str table, aor;
	char* at;

	if (unixsock_read_line(&table, msg) != 0) {
		unixsock_reply_asciiz("400 Table name expected\n");
		goto err;
	}

	if (unixsock_read_line(&aor, msg) != 0) {
		unixsock_reply_asciiz("400 Address of Record expected\n");
		goto err;
	}
	
	at = q_memchr(aor.s, '@', aor.len);

	if (use_domain) {
		if (!at) {
			unixsock_reply_asciiz("400 User@domain expected\n");
			goto err;
		}
	} else {
		if (at) {
			aor.len = at - aor.s;
		}
	}

	strlower(&aor);
	unixsock_find_domain(&table, &d);

	if (d) {
		lock_udomain(d);	

		res = get_urecord(d, &aor, &r);
		if (res < 0) {
			unixsock_reply_printf("500 Error while looking for username %.*s in table %.*s\n", 
					      aor.len, ZSW(aor.s), table.len, ZSW(table.s));
			goto err_unlock;
		}
		
		if (res > 0) {
			unixsock_reply_printf("404 Username %.*s in table %.*s not found\n", 
					      aor.len, ZSW(aor.s), table.len, ZSW(table.s));
			goto err_unlock;
		}
		
		get_act_time();

		     /* FIXME */
		     /*
		if (!print_contacts(reply_file, r->contacts)) {
			unlock_udomain(d);
			fprintf(reply_file, "404 No registered contacts found\n");
			fclose(reply_file);
			return 1;
		}
		     */

		unlock_udomain(d);
		unixsock_reply_send();
		return 0;
	} else {
		unixsock_reply_printf("400 table (%.*s) not found\n", table.len, ZSW(table.s));
		goto err;
	}

 err_unlock:
	unlock_udomain(d);
 err:
	unixsock_reply_send();
	return -1;
}


int init_ul_unixsock(void) 
{
	if (unixsock_register_cmd(UL_STATS, ul_stats_cmd) < 0) {
		LOG(L_CRIT, "init_ul_unixsock: cannot register ul_stats\n");
		return -1;
	}

	if (unixsock_register_cmd(UL_RM, ul_rm) < 0) {
		LOG(L_CRIT, "init_ul_unixsock: cannot register ul_rm\n");
		return -1;
	}

	if (unixsock_register_cmd(UL_RM_CONTACT, ul_rm_contact) < 0) {
		LOG(L_CRIT, "init_ul_unixsock: cannot register ul_rm_contact\n");
		return -1;
	}
	
	if (unixsock_register_cmd(UL_DUMP, ul_dump) < 0) {
		LOG(L_CRIT, "init_ul_unixsock: cannot register ul_dump\n");
		return -1;
	}

	if (unixsock_register_cmd(UL_FLUSH, ul_flush) < 0) {
		LOG(L_CRIT, "init_ul_unixsock: cannot register ul_flush\n");
		return -1;
	}

	if (unixsock_register_cmd(UL_ADD, ul_add) < 0) {
		LOG(L_CRIT, "init_ul_unixsock: cannot register ul_add\n");
		return -1;
	}

	if (unixsock_register_cmd(UL_SHOW_CONTACT, ul_show_contact) < 0) {
		LOG(L_CRIT, "init_ul_unixsock: cannot register ul_show_contact\n");
		return -1;
	}

	return 0;
}
