/* $Id$
 *
 * User location support module
 *
 */


#include "usrloc.h"
#include "log.h"
#include "cache.h"
#include "utils.h"
#include "../../sr_module.h"
#include "to_parser.h"
#include "db.h"
#include "../../action.h"
#include "defs.h"
#include "../../data_lump_rpl.h"
#include "../../timer.h"


/* Per-child init function */
static int child_init(int rank);

/* Timer handler */
static void tr(unsigned int ticks, void* param);

/* Module's destroy function */
void destroy(void);

/* Build Contact HF for 200 responses */
static inline void build_contact_buf(char* _b, int* _len, location_t* _loc);

/* Send 200 OK response with properly created Contact HF */
static inline int send_200(struct sip_msg* _msg, location_t* _loc);

/*
 * Process request that contained a star, in that case, we will remove
 * all bindings with the given username from the database and memory
 * cache and return 200 OK response
 */
static inline int process_star_loc(struct sip_msg* _msg, cache_t* _c, location_t* _loc);

/*
 * This function will process requests that contained no contacts, that means
 * we will return a list of all contacts currently present in the database in
 * 200 OK response
 */
static inline int process_no_contacts(struct sip_msg* _msg, cache_t* _c, location_t* _loc);

/* This function will process request that contained some contacts */
static inline int process_contacts(struct sip_msg* _msg, cache_t* _c, location_t* _loc);

/* Process REGISTER requests */
static inline int process_loc(struct sip_msg* _msg, cache_t* _c, location_t* _loc, int star);

/* Parse REGISTER request and process it's contacts */
static int save_contact(struct sip_msg* _msg, char* _table, char* _str2);

/* Lookup contact in the database and rewrite Request-URI */
static int lookup_contact(struct sip_msg* _msg, char* _table, char* _str2);

/* Rewrite Request-URI */
static int rwrite(struct sip_msg* _msg, str* _s);

/* Module initialization function */
static int mod_init(void);


/* --- Parameter variables --- */

/* Structure that represents database connection */
db_con_t* db_con;

/* In-memory cache of contacts */
static cache_t* c;

/* Flag if we are using a database or not, default is yes */
int use_db = 1;

/* Database url */
char* db_url;

/* Database table name, default value is "location" */
char* db_table = "location";

/* User column name, default value is "user"  */
char* user_col = "user";

/* contact column name, default value is "contact"  */
char* contact_col = "contact";

/* expires column name, default value is "expires"  */
char* expires_col = "expires";

/* q column name, default value is "q"  */
char* q_col = "q";

/* callid column name, default value is "callid"  */
char* callid_col = "callid";

/* cseq column name, default value is "cseq"  */
char* cseq_col = "cseq";

/* Cache flush interval, default is 60 seconds */
int flush_interval = 60;


/*
 * sl_send_reply function pointer
 */
int (*sl_reply)(struct sip_msg* _m, char* _s1, char* _s2);


/*
 * Module exports structure
 */
#ifdef STATIC_USRLOC
struct module_exports usrloc_exports = {
#else
struct module_exports exports = {
#endif
	"usrloc", 
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

	(char*[]) { /* Module parameter names */
		"use_database",
		"table",
		"db_url",
		"user_column",
		"contact_column",
		"expires_column",
		"q_column",
		"callid_column",
		"cseq_column",
		"flush_interval"
	},
	(modparam_t[]) {   /* Module parameter types */
		INT_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&use_db,
		&db_table,
		&db_url,
		&user_col,
		&contact_col,
		&expires_col,
		&q_col,
		&callid_col,
		&cseq_col,
		&flush_interval
	},
	10,         /* Number of module paramers */

	mod_init,   /* module initialization function */
	0,
	destroy,    /* destroy function */
	0,          /* oncancel function */
	child_init  /* Per-child init function */
};


/*
 * Initialize parent
 */
static int mod_init(void)
{
	printf( "Initializing usrloc module\n");

	     /*
	      * We will need sl_send_reply from stateless
	      * module for sending replies
	      */
	sl_reply = find_export("sl_send_reply", 2);
	if (!sl_reply) {
		ERR("This module requires sl module");
		return -1;
	}
	
	     /*
	      * Try to find some database module, if
	      * there is no database module, we will
	      * use memory cache only, in this case
	      * bindings will be not persistent and
	      * will not survive server restarts
	      */
	if (use_db) {
		if (bind_dbmod()) {
			ERR("Database module not found");
			return -1;
		}
	}

	     /*
	      * Create a user location cache
	      * that will speed up database
	      * writes and lookups, database
	      * is used for purpose of data
	      * persistence across restarts
	      * only, all reads and writes
	      * are memory-cached
	      */
 	c = create_cache(DEFAULT_CACHE_SIZE, db_table);
	if (c == NULL) {
		ERR("Unable to create cache");
		return -2;
	}

	     /* 
	      * Register a clean up timer,
	      * that fires every 60 seconds
	      */
	register_timer(tr, NULL, flush_interval);

	     /* 
	      * If database module was found, preload cache
	      * with contacts from database
	      */
	if (use_db) {
		ERR("Opening database connection for parent");
		db_con = db_init(db_url);
		if (!db_con) {
			ERR("Error while connecting database");
			free_cache(c);
			return -3;
		} else {
			db_use_table(db_con, db_table);
			INFO("Database connection opened successfuly");
		}

		     /*
		      * We will use database, so preload memory
		      * cache with contacts from the database
		      */
		preload_cache(c, db_con);
	}

	return 0;
}


/*
 * Initialize childs
 */
static int child_init(int rank)
{
	     /*
	      * If we found a database module, open
	      * a new database connection for each
	      * child processs and let the database
	      * do locking and synchronization burden
	      * otherwise we would have to lock every
	      * time we use database connection file
	      * descriptor
	      */
	if (use_db) {
		INFO("Opening database connection for child %d", rank);
		db_con = db_init(db_url);
		if (!db_con) {
			ERR("child %d: Error while connecting database", rank);
			return -1;
		} else {
			db_use_table(db_con, db_table);
			INFO("child %d: Database connection opened successfuly", rank);
		}
	}

	return 0;
}


/*
 * Timer handler, every 60 seconds clean up of
 * cache and database is performed, i. e. expired
 * entries are purged.
 */
static void tr(unsigned int ticks, void* param)
{
	DBG("Starting cache synchronization timer");
	clean_cache(c, db_con);
	DBG("Cache synchronization timer finished");
}


/*
 * Do some cleanup
 */
void destroy(void)
{
	free_cache(c);
	if (db_con) {
		DBG("Closing database connection");
		db_close(db_con);
	}
}



/*
 * FIXME: Replace sprintf, check if there is still enough
 *        place in the buffer
 *        Check what happens if all contacts expired already
 *        and the function will return zero length
 */
static inline void build_contact_buf(char* _b, int* _len, location_t* _loc)
{
	time_t t;
	int l;
	contact_t* ptr;

	t = time(NULL);
	l = 0;

	ptr = LOC_CONTACTS_FIRST(_loc);

	while(ptr) {
		if (ptr->expires >= t) {
			memcpy(_b + l, "Contact: <", 10);
			l += 10;
			
			memcpy(_b + l, ptr->c.s, ptr->c.len);
			l += ptr->c.len;
	
			//			memcpy(_b + l, ">", 1);  intel hack
			//			l += 1;

		
			memcpy(_b + l, ">;q=", 4);
			l += 4;
			
			l += sprintf(_b + l, "%-3.2f", ptr->q);
			
			memcpy(_b + l, ";expires=", 9);
			l += 9;
			
			l += sprintf(_b + l, "%d", (int)(ptr->expires - t));
			
			*(_b + l++) = '\r';
			*(_b + l++) = '\n';
		}

		ptr = ptr->next;
	}

	//	memcpy(_b + l, "Expires: 3600\r\n", 15);  intel hack
	//	l += 15;

	*(_b + l) = '\0';
	*_len = l;

	DBG("Created Contact HF: %s", _b);
}


/*
 * Send 200 OK response using stateless module
 * Contact HF will be cenostructed if _loc != NULL
 */
static inline int send_200(struct sip_msg* _msg, location_t* _loc)
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


/*
 * Process request that contained a star, in that case, we will remove
 * all bindings with the given username from the database and memory
 * cache and return 200 OK response
 */
static inline int process_star_loc(struct sip_msg* _msg, cache_t* _c, location_t* _loc)
{
	DBG("Processing * Contact");
	     /* Remove all contacts for the given username from
	      * the database
	      */
	if (cache_remove(_c, db_con, &(_loc->user)) == FALSE) {
		ERR("Error while removing cache entries");
		return FALSE;
	}
	
	DBG("All bindings removed, sending 200 OK");

	     /* Send 200 OK response with no Contact HF */
	if (send_200(_msg, NULL) == FALSE) {
		ERR("Error while sending 200 response");
		return FALSE;
	}
	
	return TRUE;
}


/*
 * This function will process requests that contained no contacts, that means
 * we will return a list of all contacts currently present in the database in
 * a 200 OK response
 */
static inline int process_no_contacts(struct sip_msg* _msg, cache_t* _c, location_t* _loc)
{
	c_elem_t* el;

	     /* Lookup the database and try to find
	      * any contacts for the given username
	      */
	DBG("Processing REGISTER request with no contacts");
	el = cache_get(c, &(_loc->user));
	if (!el) {    /* There are no such contacts */
		DBG("No bindings found, sending 200 OK");
		     /* Send 200 OK response without Contact HF */
		if (send_200(_msg, NULL) == FALSE) {
			ERR("Error while sending 200 OK");
			return FALSE;
		}
		return TRUE;
	} else {  /* Some contacts were found */
		DBG("Bindings found, sending 200 OK with bindings");
		     /* Send 200 OK and list all contacts currently
		      * present in the database
		      */
		if (send_200(_msg, ELEM_LOC(el)) == FALSE) {
			ERR("Error while sending 200 response");
			cache_release_elem(el);
			return FALSE;
		}
		     /* Release the cache mutex */
		cache_release_elem(el);
		return TRUE;
	}
}


/* 
 * This function will process request that contained some contacts
 */
static inline int process_contacts(struct sip_msg* _msg, cache_t* _c, location_t* _loc)
{
	c_elem_t* el;
	int send_rep;

	DBG("Processing REGISTER request with Contact(s)");

	     /* Try to find contacts with the same 
	      * address of record in the database
	      */
	el = cache_get(_c, &(_loc->user));
	if (el) {  /* Some contacts were found */
		DBG("Location found in cache, updating");

		     /* Update location structure in the database with new values
		      * present in _loc parameter
		      */
		if (cache_update_unsafe(_c, db_con, &el, _loc, &send_rep) == FALSE) {
			ERR("Error while updating bindings in cache");
			cache_release_elem(el);
			return FALSE;
		}
		
		if (send_rep) {
			DBG("Sending 200 OK");

			     /* Send 200 OK response with actual list of contacts
			      * in the database if there are any
			      */
			if (send_200(_msg, (el) ? (ELEM_LOC(el)) : (NULL)) == FALSE) {
				ERR("Error while sending 200 OK response");
				if (el) cache_release_elem(el);
				return FALSE;
			}
		}
		
		if (el) cache_release_elem(el);
		return TRUE;
	} else {  /* No contacts in the database were found */
		DBG("Location not found in cache, inserting");

		     /* Remove all contacts that have expires set to zero */
		remove_zero_expires(_loc);
		if (!IS_EMPTY(_loc)) { /* And insert the rest into database */
			if (cache_insert(_c, db_con, _loc) == FALSE) {
			        ERR("Error while inserting location");
				return FALSE;
			}
		}
		DBG("Sending 200 OK");
		if (send_200(_msg, _loc) == FALSE) {
			ERR("Error while sending 200 response");
			return FALSE;
		}
	}
	return TRUE;
}


/*
 * Process REGISTER requests
 */
static inline int process_loc(struct sip_msg* _msg, cache_t* _c, location_t* _loc, int star)
{
	if (star == 1) {
		DBG("star = 1, processing");
		return process_star_loc(_msg, _c, _loc);
	} else {
		if (IS_EMPTY(_loc)) {
			DBG("No contacts found in REGISTER message, processing");
			return process_no_contacts(_msg, _c, _loc);
		} else {
			DBG("Contacts found in REGISTER message, processing");
			return process_contacts(_msg, _c, _loc);
		}
	}
}


/*
 * Process REGISTER request and save it's contacts
 */
static int save_contact(struct sip_msg* _msg, char* _table, char* _str2)
{
	location_t* loc;
	int star, expires, valid;

	if (sip_to_loc(_msg, &loc, &star, &expires) == FALSE) {
		ERR("Unable to convert SIP message to location structure");
		return -1;
	}

	     /* Check, if the request is sane */
	DBG("Validating request");
	if (validate_location(loc, expires, star, &valid) == FALSE) {
		ERR("Error while validating request");
		free_location(loc);
		return -1;
	}

	if (!valid) { /* Request is invalid, send 400 Bad Request */
		DBG("Request not validated, sending 400 Bad Request");
		free_location(loc);
		if (sl_reply(_msg, (char*)400, "Bad Request") == -1) {
			ERR("Error while sending 400 Bad Request response");
		}
		return -1;
	}

	DBG("Request validated, processing");
	if (process_loc(_msg, c, loc, star) == FALSE) {
		ERR("Error while processing location");
		free_location(loc);
		return -1;
	}

	return 1;
}


/*
 * Lookup contact in the database and rewrite Request-URI
 */
static int lookup_contact(struct sip_msg* _msg, char* _table, char* _str2)
{
	c_elem_t* el;
	str user;
	time_t t;
	contact_t* ptr;
	
	if (!_msg->to) {
		if (parse_headers(_msg, HDR_TO) == -1) {
			ERR("Error while parsing headers");
			return -1;
		}
	}

	if (!_msg->to) {
	        ERR("Unable to find To HF");
		return -1;
	}
	
	if (_msg->new_uri.s) {
		user.s = _msg->new_uri.s;
		user.len = _msg->new_uri.len;
	} else {
		user.s = _msg->first_line.u.request.uri.s;
		user.len = _msg->first_line.u.request.uri.len;
	}
	get_username(&user);
	
	if (user.len == 0) {
		ERR("Unable to get username");
		return -1;
	}
	
	t = time(NULL);
	el = cache_get(c, &user);
	if (el) { 
		ptr = LOC_CONTACTS_FIRST(ELEM_LOC(el));
		while ((ptr) && (CONTACT_EXPIRES(ptr) < t)) ptr = CONTACT_NEXT(ptr);
		
		if (ptr) {
			DBG("Binding found, rewriting Request-URI");
			if (rwrite(_msg, &(CONTACT_CON(ptr))) == FALSE) {
				ERR("Unable to rewrite request URI");
				cache_release_elem(el);
				return -1;
			}
			cache_release_elem(el);
			return 1;	
		} else {
			DBG("Binding has expired or not found");
			cache_release_elem(el);
			return -1;
		}
	} else {
		DBG("No binding found in database");
		return -2;
	}
}


/*
 * Rewrite Request-URI
 */
static int rwrite(struct sip_msg* _msg, str* _s)
{
	char buffer[256];
	struct action act;

	memcpy(buffer, _s->s, _s->len);
	buffer[_s->len] = '\0';

	DBG("Rewriting with %s", buffer);
	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = buffer;
	act.next = NULL;

	if (do_action(&act, _msg) < 0) {
		ERR("Error in do_action");
		return FALSE;
	}
	return TRUE;
}
