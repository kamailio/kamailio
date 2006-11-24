/*
 * $Id$
 *
 * Copyright (C) 2004 FhG FOKUS
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
#include "../../qvalue.h"
#include "dlist.h"
#include "ul_mod.h"
#include "ul_fifo.h"
#include "utime.h"
#include "ul_unixsock.h"

/*
 * Dedicated to Douglas Adams, don't panic !
 */
#define UNIXSOCK_CALLID "The-Answer-To-The-Ultimate-Question-Of-Life-Universe-And-Everything"
#define UNIXSOCK_CSEQ 42
#define UNIXSOCK_UA "OpenSER Server UNIXSOCK"

static str unix_cid = str_init(UNIXSOCK_CALLID);
static str unix_ua  = str_init(UNIXSOCK_UA);


static inline int add_contact(udomain_t* _d, str* _u, str* _c,
														ucontact_info_t *_ci)
{
	urecord_t* r;
	ucontact_t* c = 0;
	int res;

	res = get_urecord(_d, _u, &r);
	if (res < 0) {
		LOG(L_ERR, "fifo_add_contact(): Error while getting record\n");
		goto error0;
	}

	if (res >  0) { /* Record not found */
		if (insert_urecord(_d, _u, &r) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while creating "
				"new urecord\n");
			goto error0;
		}
	} else {
		if (get_ucontact(r, _c, &unix_cid, UNIXSOCK_CSEQ+1, &c) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while obtaining ucontact\n");
			goto error0;
		}
	}

	get_act_time();

	_ci->callid = &unix_cid;
	_ci->user_agent = &unix_ua;
	_ci->cseq = UNIXSOCK_CSEQ;
	/* 0 expires means permanent contact */
	if (_ci->expires!=0)
		_ci->expires += act_time;

	if (c) {
		if (update_ucontact( r, c, _ci) < 0) {
			LOG(L_ERR, "fifo_add_contact(): Error while updating contact\n");
			goto error1;
		}
	} else {
		if ( insert_ucontact( r, _c, _ci, &c) < 0 ) {
			LOG(L_ERR, "fifo_add_contact(): Error while inserting contact\n");
			goto error1;
		}
	}

	release_urecord(r);
	return 0;
error1:
	release_urecord(r);
error0:
	return -1;
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
		lock_udomain(d, &user);
		if (delete_urecord(d, &user, 0) < 0) {
			LOG(L_ERR, "ul_rm(): Error while deleting user %.*s\n", user.len, ZSW(user.s));
			unlock_udomain(d, &user);
			unixsock_reply_printf("500 Error while deleting user %.*s\n", user.len, ZSW(user.s));
			goto err;
		}
		unlock_udomain(d, &user);
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
		lock_udomain(d, &user);

		res = get_urecord(d, &user, &r);
		if (res < 0) {
			unixsock_reply_printf("500 Error while looking for username %.*s in table %.*s\n",
					      user.len, ZSW(user.s), table.len, ZSW(table.s));
			unlock_udomain(d, &user);
			goto err;
		}
		
		if (res > 0) {
			unixsock_reply_printf("404 Username %.*s in table %.*s not found\n",
					      user.len, ZSW(user.s), table.len, ZSW(table.s));
			unlock_udomain(d, &user);
			goto err;
		}

		res = get_ucontact(r, &contact, &unix_cid, UNIXSOCK_CSEQ+1, &con);
		if (res < 0) {
			unixsock_reply_printf("500 Error while looking for contact %.*s\n", contact.len, ZSW(contact.s));
			unlock_udomain(d, &user);
			goto err;
		}			

		if (res > 0) {
			unixsock_reply_printf("404 Contact %.*s in table %.*s not found\n", 
					      contact.len, ZSW(contact.s), table.len, ZSW(table.s));
			unlock_udomain(d, &user);
			goto err;
		}

		if (delete_ucontact(r, con) < 0) {
			unixsock_reply_printf("500 ul_rm_contact: Error while deleting contact %.*s\n", 
					      contact.len, ZSW(contact.s));
			unlock_udomain(d, &user);
			goto err;
		}

		release_urecord(r);
		unlock_udomain(d, &user);
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
	     /* This function is not implemented in unixsock interface,
	      * it produces a lot of data which would not fit into a single
	      * message
	      */
	unixsock_reply_asciiz( "500 Not Implemented\n");
	return 0;
}


static int ul_add(str* msg)
{
	ucontact_info_t ci;
	udomain_t* d;
	char* at;
	str table, user, contact, expires, q, rep, flags, methods;

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
	
	/* Kept for backwards compatibility */
	if (unixsock_read_line(&rep, msg) != 0) {
		unixsock_reply_asciiz("400 Replicate expected\n");
		goto err;
	}
	
	if (unixsock_read_line(&flags, msg) != 0) {
		unixsock_reply_asciiz("400 Flags expected\n");
		goto err;
	}
	
	if (unixsock_read_line(&methods, msg) != 0) {
		unixsock_reply_asciiz("400 Methods expected\n");
		goto err;
	}
	
	strlower(&user);
	unixsock_find_domain(&table, &d);
	
	if (d) {
		memset( &ci, 0, sizeof(ucontact_info_t));
		
		if (str2int(&expires, (unsigned int*)&ci.expires) < 0) {
			unixsock_reply_asciiz("400 Invalid expires format\n");
			goto err;
		}
		
		if (str2q(&ci.q, q.s, q.len) < 0) {
			unixsock_reply_asciiz("400 invalid q value\n");
			goto err;
		}
		
		if (str2int(&flags, (unsigned int*)&ci.flags1) < 0) {
			unixsock_reply_asciiz("400 Invalid flags format\n");
			goto err;
		}
		if (str2int(&methods, (unsigned int*)&ci.methods) < 0) {
			unixsock_reply_asciiz("400 Invalid methods format\n");
			goto err;
		}
		
		lock_udomain(d, &user);
		
		if (add_contact(d, &user, &contact, &ci) < 0) {
			unlock_udomain(d, &user);
			LOG(L_ERR, "ul_add(): Error while adding contact ('%.*s','%.*s') "
				"in table '%.*s'\n", user.len, ZSW(user.s), contact.len, 
				ZSW(contact.s), table.len, ZSW(table.s));
			unixsock_reply_printf("500 Error while adding contact\n"
				" ('%.*s','%.*s') in table '%.*s'\n", user.len, ZSW(user.s),
				contact.len, ZSW(contact.s), table.len, ZSW(table.s));
			goto err;
		}
		unlock_udomain(d, &user);
		
		unixsock_reply_printf("200 Added to table\n"
			"('%.*s','%.*s') to '%.*s'\n", user.len, ZSW(user.s), 
			contact.len, ZSW(contact.s), table.len, ZSW(table.s));
		unixsock_reply_send();
		return 0;
	} else {
		unixsock_reply_printf("400 Table '%.*s' not found in memory, use "
			"save(\"%.*s\") or lookup(\"%.*s\") in the configuration script "
			"first\n", table.len, ZSW(table.s), table.len, ZSW(table.s), 
			table.len, ZSW(table.s));
		unixsock_reply_send();
		return 0;
	}
err:
	unixsock_reply_send();
	return -1;
}


/*
 * Build Contact HF for reply
 */
static inline int print_contacts(ucontact_t* _c)
{
	int cnt = 0;
	int n;

	while(_c) {
		if (VALID_CONTACT(_c, act_time)) {
			cnt++;
			if (cnt == 1) {
				unixsock_reply_asciiz("200 OK\n");
			}
			n = unixsock_reply_printf("<%.*s>;q=%s;expires=%d;flags=0x%X;"
				"socket=<%.*s>;methods=0x%X"
				"%s%.*s%s" /*received*/
				"%s%.*s%s" /*user-agent*/
				"%s%.*s%s\n", /*path*/
				_c->c.len, ZSW(_c->c.s),
				q2str(_c->q, 0), (int)(_c->expires - act_time), _c->flags,
								_c->sock?_c->sock->sock_str.len:3,
					_c->sock?_c->sock->sock_str.s:"NULL",
				_c->methods,
				_c->received.len?";received=<":"",_c->received.len,
					ZSW(_c->received.s), _c->received.len?">":"",
				_c->user_agent.len?";user_agent=<":"",_c->user_agent.len,
					ZSW(_c->user_agent.s), _c->user_agent.len?">":"",
				_c->path.len?";path=<":"",_c->path.len,
					ZSW(_c->path.s), _c->path.len?">":""
				);
			if (n < 0) {
				return -1;
			}
		}
		_c = _c->next;
	}
	return cnt == 0;
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
		lock_udomain(d, &aor);	

		res = get_urecord(d, &aor, &r);
		if (res < 0) {
			unlock_udomain(d, &aor);
			unixsock_reply_printf("500 Error while looking for username %.*s in table %.*s\n", 
					      aor.len, ZSW(aor.s), table.len, ZSW(table.s));
			goto err;
		}
		
		if (res > 0) {
			unlock_udomain(d, &aor);
			unixsock_reply_printf("404 Username %.*s in table %.*s not found\n", 
					      aor.len, ZSW(aor.s), table.len, ZSW(table.s));
			goto err;
		}
		
		get_act_time();
		
		res = print_contacts(r->contacts);
		if (res > 0) {
			unixsock_reply_asciiz("404 No registered contacts found\n");
			res = 1;
		} else if (res < 0) {
			unlock_udomain(d, &aor);
			unixsock_reply_reset();
			unixsock_reply_asciiz("500 Buffer too small\n");
			goto err;
		} else {
			res = 0;
		}

		unlock_udomain(d, &aor);
		unixsock_reply_send();
		return res;
	} else {
		unixsock_reply_printf("400 table (%.*s) not found\n", table.len, ZSW(table.s));
		goto err;
	}

 err:
	unixsock_reply_send();
	return -1;
}


int init_ul_unixsock(void) 
{
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
