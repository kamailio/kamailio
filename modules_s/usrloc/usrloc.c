/* $Id$
 *
 * User location support
 *
 */


#include "usrloc.h"
#include "../../dprint.h"
#include "cache.h"
#include "utils.h"
#include "../../sr_module.h"
#include "to_parser.h"
#include "db.h"
#include "../../action.h"
#include "defs.h"
#include "../../data_lump_rpl.h"
#include "../../timer.h"

static int  save_contact   (struct sip_msg*, char*, char*);
static int  lookup_contact (struct sip_msg*, char*, char*);
static int  rwrite         (struct sip_msg* _msg, str* _c);
static int  child_init     (int rank);
       int  (*sl_reply)    (struct sip_msg* _msg, char* _str1, char* _str2);
       int  child_init     (int rank);
       void destroy        (void);
static void tr          (unsigned int ticks, void* param);

db_con_t* db_con;
static cache_t* c;
static int use_db;


static struct module_exports usrloc_exports= {	"usrloc", 
						(char*[]) {
							"save_contact",
							"lookup_contact"
						},
						(cmd_function[]) {
							save_contact, 
							lookup_contact
						},
						(int[]){1, 1},
						(fixup_function[]){0, 0},
						2,
						0,
						destroy,
						0,          /* oncancel function */
						child_init
};


char* pokus;

/*
 * Initialize parent
 */
struct module_exports* mod_register()
{
	printf( "Registering user location module\n");

	sl_reply = find_export("sl_send_reply", 2);
	if (!sl_reply) {
		LOG(L_ERR, "mod_register(): This module requires sl module\n");
	}
	
	if (bind_dbmod()) {
		DBG("usrloc: Database module not found, using memory cache only\n");
		db_con = NULL;
		use_db = 0;
	} else {
		DBG("usrloc: Database module found, will be used for persistent storage\n");
		use_db = 1;
	}

 	c = create_cache(512, TABLE_NAME);
	if (c == NULL) {
		LOG(L_ERR, "mod_register(): Unable to create cache\n");
	}

	register_timer(tr, NULL, 60);

	if (use_db) {
		DBG("usrloc: Opening database connection for parent\n");
		db_con = db_init(DB_URL);
		if (!db_con) {
			LOG(L_ERR, "usrloc-parent: Error while connecting database\n");
		} else {
			db_use_table(db_con, TABLE_NAME);
			DBG("usrloc: Database connection opened successfuly\n");
		}

		preload_cache(c, db_con);
	}


	return &usrloc_exports;
}


/*
 * Initialize childs
 */
static int child_init(int rank)
{
	if (use_db) {
		DBG("usrloc: Opening database connection for child %d\n", rank);
		db_con = db_init(DB_URL);
		if (!db_con) {
			LOG(L_ERR, "usrloc-rank %d: Error while connecting database\n", rank);
			return -1;
		} else {
			db_use_table(db_con, TABLE_NAME);
			DBG("usrloc-rank %d: Database connection opened successfuly\n", rank);
		}
	}

	return 0;
}


static void tr(unsigned int ticks, void* param)
{
	DBG("timer(): Running timer\n");
	clean_cache(c, db_con);
	DBG("timer(): Timer finished\n");
}


/*
 * Do some cleanup
 */
void destroy(void)
{
	free_cache(c);
	if (db_con) db_close(db_con);
}


/*
 * FIXME: Don't use snprintf
 */
void build_contact_buf(char* _buf, int* _len, location_t* _loc)
{
	int l;
	contact_t* ptr;
	time_t t;

	t = time(NULL);
	
	memcpy(_buf, "Contact: <", 10);
	l = 10;

	ptr = _loc->contacts;
	while(ptr) {
		memcpy(_buf + l, ptr->c.s, ptr->c.len);
		l += ptr->c.len;
		memcpy(_buf + l, ">;q=", 4);
		l += 4;
		l += sprintf(_buf + l, "%-3.2f", ptr->q);
		memcpy(_buf + l, ";expires=", 9);
		l += 9;
		     /* FIXME: %d signed ? */
		l += sprintf(_buf + l, "%d", (int)(ptr->expires - t));

		ptr = ptr->next;
		if (ptr) {
			memcpy(_buf + l, ",\r\n         ", 12);
			l += 12;
		} else {
			memcpy(_buf + l, "\r\n", 2);
			l += 2;
		}
		
	}

	*(_buf + l) = '\0';
	DBG("build_contact_buf(): %s\n", _buf);

	*_len = l;
}


static int send_200(struct sip_msg* _msg, location_t* _loc)
{
	struct lump_rpl* ptr;
	char buffer[MAX_CONTACT_BUFFER];
	int len;

	if (_loc) {
		build_contact_buf(buffer, &len, _loc);
		ptr = build_lump_rpl(buffer, len);
		add_lump_rpl(_msg, ptr);
	}

	sl_reply(_msg, (char*)200, "OK");
	return TRUE;
}


static inline int process_star_loc(struct sip_msg* _msg, cache_t* _c, location_t* _loc)
{
	     /* Remove all bindings with the same address
	      * of record
	      */
	DBG("process_star_loc(): Removing all bindings from cache\n");
	if (cache_remove(_c, db_con, &(_loc->user)) == FALSE) {
		LOG(L_ERR, "process_star_loc(): Error while removing cache entry\n");
		return FALSE;
	}
	
	DBG("process_star_loc(): All bindings removed, sending 200 OK\n");
	if (send_200(_msg, NULL) == FALSE) {
		LOG(L_ERR, "process_star_loc(): Error while sending 200 response\n");
		return FALSE;
	}
	
	return TRUE;
}


static inline int process_no_contacts(struct sip_msg* _msg, cache_t* _c, location_t* _loc)
{
	c_elem_t* el;

	el = cache_get(c, &(_loc->user));
	if (!el) {
		DBG("process_no_contacts(): No bindings found, sending 200 OK\n");
		if (send_200(_msg, NULL) == FALSE) {
			LOG(L_ERR, "process_no_contacts(): Error while sending 200 OK\n");
			return FALSE;
		}
		return TRUE;
	} else {
		DBG("process_no_contacts(): Bindings found, sending 200 OK with bindings\n");
		if (send_200(_msg, el->loc) == FALSE) {
			LOG(L_ERR, "process_no_contact(): Error while sending 200 response\n");
			cache_release_elem(el);
			return FALSE;
		}
		cache_release_elem(el);
		return TRUE;
	}
}


static inline int process_contacts(struct sip_msg* _msg, cache_t* _c, location_t* _loc)
{
	c_elem_t* el;

	el = cache_get(_c, &(_loc->user));
	if (el) {
		DBG("process_contacts(): Location found in cache, updating\n");
		if (cache_update(_c, db_con, el, _loc) == FALSE) {
			LOG(L_ERR, "process_contacts(): Error while updating bindings in cache\n");
			cache_release_elem(el);
			return FALSE;
		}
		
		DBG("process_contacts(): Sending 200 OK\n");
		if (send_200(_msg, (el) ? (el->loc) : (NULL)) == FALSE) {
			LOG(L_ERR, "process_contacts(): Error while sending 200 response\n");
			if (el) cache_release_elem(el);
			return FALSE;
		}
		
		if (el) cache_release_elem(el);
		return TRUE;
	} else {
		DBG("process_contacts(): Location not found in cache, inserting\n");
		remove_zero_expires(_loc);
		if (_loc->contacts) {
			if (cache_put(_c, db_con, _loc) == FALSE) {
				LOG(L_ERR, "process_contacts(): Error while inserting location\n");
				return FALSE;
			}
		}
		DBG("process_contacts(): Sending 200 OK\n");
		if (send_200(_msg, _loc) == FALSE) {
			LOG(L_ERR, "process_contacts(): Error while sending 200 response\n");
			return FALSE;
		}
	}
	return TRUE;
}



static int process_loc(struct sip_msg* _msg, cache_t* _c, location_t* _loc, int star)
{
	if (star == 1) {
		DBG("process_loc(): star = 1, processing\n");
		return process_star_loc(_msg, _c, _loc);
	} else {
		if (!_loc->contacts) {
			DBG("process_loc(): No contacts found\n");
			return process_no_contacts(_msg, _c, _loc);
		} else {
			DBG("process_loc(): Contacts found\n");
			return process_contacts(_msg, _c, _loc);
		}
	}
}



static int save_contact(struct sip_msg* _msg, char* _table, char* _str2)
{
	location_t* loc;
	int star, expires, valid;

	if (msg2loc(_msg, &loc, &star, &expires) == FALSE) {
		LOG(L_ERR, "save_contact(): Unable to convert SIP message to location_t\n");
		return -1;
	}

	DBG("save_contact(): Validating request\n");
	if (validate_location(loc, expires, star, &valid) == FALSE) {
		LOG(L_ERR, "save_contact(): Error while validating request\n");
		free_location(loc);
		return -1;
	}

	if (!valid) {
		DBG("save_contact(): Request not validated, sending 400\n");
		free_location(loc);
		if (sl_reply(_msg, (char*)400, "Bad Request") == -1) {
			LOG(L_ERR, "save_contact(): Error while sending 400 response\n");
		}
		return -1;
	}

	DBG("save_contact(): Request validated, processing\n");
	if (process_loc(_msg, c, loc, star) == FALSE) {
		LOG(L_ERR, "save_contact(): Error while processing location\n");
		free_location(loc);
		return -1;
	}

	print_cache(c);

	return 1;
}



/*
 * FIXME: Pouzit uz parsovanou To hlavicku
 */
static int lookup_contact(struct sip_msg* _msg, char* _table, char* _str2)
{
	c_elem_t* el;
	str user;
	time_t t;
	
	if (!_msg->to) {
		if (parse_headers(_msg, HDR_TO) == -1) {
			LOG(L_ERR, "lookup_contact(): Error while parsing headers\n");
			return -1;
		}
	}

	if (!_msg->to) {
		LOG(L_ERR, "lookup_contact(): Unable to find To header field\n");
		return -1;
	}
	
	user.s = _msg->to->body.s;
	user.len = _msg->to->body.len;
	get_to_username(&user);
	
	if (user.len == 0) {
		LOG(L_ERR, "lookup_contact(): Unable to get user name\n");
		return -1;
	}
	
	t = time(NULL);
	el = cache_get(c, &user);
	if (el) {
		if ((el->loc->contacts) && (el->loc->contacts->expires >= t)) {
			DBG("lookup_contact(): Binding find, rewriting Request-URI\n");
			if (rwrite(_msg, &(el->loc->contacts->c)) == FALSE) {
				LOG(L_ERR, "lookup_contact(): Unable to rewrite request URI\n");
				cache_release_elem(el);
				return -1;
			}
			cache_release_elem(el);
		        return 1;	
		} else {
                        DBG("lookup_contact(): Binding has expired\n");
			cache_release_elem(el);
			return -1;
	        };
	} else {
		DBG("lookup_contact(): No binding found in database\n");
		return -2;
	}
}



static int rwrite(struct sip_msg* _msg, str* _s)
{
	char buffer[256];
	struct action act;

	memcpy(buffer, _s->s, _s->len);
	buffer[_s->len] = '\0';

	DBG("rwrite(): Rewriting with %s\n", buffer);
	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = buffer;
	act.next = NULL;

	if (do_action(&act, _msg) < 0) {
		LOG(L_ERR, "rwrite(): Error in do_action\n");
		return FALSE;
	}
	return TRUE;
}
