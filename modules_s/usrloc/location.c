/* 
 * $Id$ 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "location.h"
#include "../../str.h"
#include "utils.h"
#include "../../dprint.h"
#include "../../msg_parser.h"
#include "const.h"
#include "to_parser.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "defs.h"

/*
 * Function prototypes
 */

static inline int get_expires_hf(struct sip_msg* _msg);
static inline int parse_all_headers(struct sip_msg* _msg);
static inline int make_to_copy(char** _to, struct sip_msg* _msg);
static inline int process_all_contacts(location_t* _loc, struct sip_msg* _msg, int _expires, int* _star,
				       const char* _callid, int _cseq);
static inline int get_CSeq(struct sip_msg* _msg, int* _cseq);
static inline int get_CallID(struct sip_msg* _msg, char** _callid);



/*
 * Get expires HF value, if there is no
 * Expires HF, use default value instead
 */
static inline int get_expires_hf(struct sip_msg* _msg)
{
	struct hdr_field* ptr;
	int expires;


	     /* Find the first Expires HF */
	ptr = _msg->headers;
	while(ptr) {
		if (ptr->type == HDR_OTHER) {
			if (!strcasecmp(ptr->name.s, "expires")) {
				     /* Convert string value to an integer */
				expires = atoi(ptr->body.s);
				     /* If the value is 0, we are probably dealing
				      * with a STAR contact, otherwise convert the
				      * value to absolute time
				      */
				if (expires != 0) expires += time(NULL);
				return expires;
			}
		}
		ptr = ptr->next;
	}
	     /* No Expires HF found, use default value */
	return time(NULL) + DEFAULT_EXPIRES;
}


/*
 * Parse the whole message header
 */
static inline int parse_all_headers(struct sip_msg* _msg)
{
#ifdef PARANOID
	if (!_msg) {
		LOG(L_ERR, "parser_all_headers(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	if (parse_headers(_msg, HDR_EOH) == -1) {
		LOG(L_ERR, "parse_all_headers(): Error while parsing headers\n");
		return FALSE;
	}

	     /* To: HF contains Addres of Record, this will be
	      * needed
	      */
	if (!_msg->to) {
		LOG(L_ERR, "parse_all_headers(): Unable to find To header field\n");
		return FALSE;
	}
	
	return TRUE;
}


/*
 * Make a temporary copy of To and Contact header fields
 * FIXME: Does not make copy of contact HF
 */
static inline int make_to_copy(char** _to, struct sip_msg* _msg)
{
#ifdef PARANOID
	if ((!_to) || (!_msg)) {
		LOG(L_ERR, "make_tmp_copy(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	*_to = (char*)pkg_malloc(_msg->to->body.len + 1);

	if (!(*_to)) {
		LOG(L_ERR, "make_tmp_copy(): No memory left\n");
		pkg_free((*_to));
		return FALSE;
	}

	     /* FIXME: Do I really need this ? */
	_msg->to = remove_crlf(_msg->to);

	     /* Make a temporary copy of To: header field */
	memcpy((*_to), _msg->to->body.s, _msg->to->body.len + 1);
	
	return TRUE;
}


/*
 * Proces all contact field in a message header
 */
static inline int process_all_contacts(location_t* _loc, struct sip_msg* _msg, int _expires, int* _star,
				       const char* _callid, int _cseq)
{
	char* contact;
	struct hdr_field* ptr;

#ifdef PARANOID
	if ((!_loc) || (!_msg) || (!_star) || (!_callid)) {
		LOG(L_ERR, "process_all_contacts(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	*_star = 0;
	contact = (char*)pkg_malloc(MAX_CONTACT_LEN);
	if (!contact) {
		LOG(L_ERR, "process_all_contacts(): No memory left\n");
		return FALSE;
	}

	ptr = _msg->headers;
	while(ptr) {
		if (ptr->type == HDR_CONTACT) {

			/* FIXME: Again, do I really need this ? */
			ptr = remove_crlf(ptr);
			ptr->body.s[ptr->body.len] = '\0';
			memcpy(contact, ptr->body.s, ptr->body.len + 1);
			
			if (parse_contact_hdr(contact, _loc, _expires, _star, _callid, _cseq) == FALSE) {
				LOG(L_ERR, "process_all_contacts(): Error while parsing Contact header field\n");
				pkg_free(contact);
				return FALSE;
			}
		}
		ptr = ptr->next;
	}
	
	pkg_free(contact);
	return TRUE;
}


/*
 * Get CSeq value
 */
static inline int get_CSeq(struct sip_msg* _msg, int* _cseq)
{
#ifdef PARANOID
	if ((!_msg) || (!_cseq)) {
		LOG(L_ERR, "get_cseq(): Invalid parameter value\n");
		return FALSE;
	}
#endif

	*_cseq = atoi(get_cseq(_msg)->number.s);

	return TRUE;
}


static inline int get_CallID(struct sip_msg* _msg, char** _callid)
{
	char* ptr;
	int len;
#ifdef PARANOID
	if ((!_msg) || (!_callid)) {
		LOG(L_ERR, "get_callid(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	_msg->callid->body.s[_msg->callid->body.len] = '\0';
	*_callid = trim(_msg->callid->body.s);
	len = strlen(*_callid);

	     /* FIXME*/
	while ((*(*_callid + len - 1) == '\r') || (*(*_callid + len - 1) == '\n')) len--;
	*(*_callid + len) = '\0';

	return TRUE;
}


/*
 * Convert REGISTER SIP message into location structure
 */
int msg2loc(struct sip_msg* _msg, location_t** _loc, int* _star, int* _expires)
{
	char* to_user, *callid;
	struct hdr_field* ptr;
	int cseq;
	
#ifdef PARANOID
	if ((!_msg) || (!_loc) || (!_star) || (!_expires)) {
		LOG(L_ERR, "msg2loc(): Invalid parameter value\n");
		return FALSE;
	}
#endif

	if (parse_all_headers(_msg) == FALSE) {
		LOG(L_ERR, "msg2loc(): Error while parsing message headers\n");
		return FALSE;
	}

	     /* Extract username from To URI */
	to_user = get_to_username(_msg->to->body.s, _msg->to->body.len);
	     /* Not needed anymore */
	    
	if (!to_user) {
		LOG(L_ERR, "msg2loc(): Error while parsing To header field \n");
		return FALSE;
	}

	if (create_location(_loc, to_user) == FALSE) {
		LOG(L_ERR, "msg2loc(): Unable to create location structure\n");
		return FALSE;
	}

	*_expires = get_expires_hf(_msg);
	
	if (get_CSeq(_msg, &cseq) == FALSE) {
		LOG(L_ERR, "msg2loc(): Unable to get CSeq value\n");
		return FALSE;
	}

	if (get_CallID(_msg, &callid) == FALSE) {
		LOG(L_ERR, "msg2loc(): Unable to get Call-ID value\n");
		return FALSE;
	}

	if (process_all_contacts(*_loc, _msg, *_expires, _star, callid, cseq) == FALSE) {
		LOG(L_ERR, "msg2loc(): Error while processing Contact field\n");
		pkg_free(*_loc);
		return FALSE;
	}
		
	return TRUE;
}


/*
 * Create a new location structure
 */
int create_location(location_t** _loc, const char* _user)
{
	int len;
#ifdef PARANOID
	if ((!_loc) || (!_user)) {
		LOG(L_ERR, "create_location(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	*_loc = (location_t*)pkg_malloc(sizeof(location_t));
	if (!(*_loc)) {
		LOG(L_ERR, "create_location(): No memory left\n");
		return FALSE;
	}

	len = strlen(_user);

	(*_loc)->user.s = (char*)pkg_malloc(len + 1);
	if (!((*_loc)->user.s)) {
		LOG(L_ERR, "create_location(): No memory left\n");
		return FALSE;
	}

	memcpy((*_loc)->user.s, _user, len + 1);

	     /* We are case insensitive */
	strlower((*_loc)->user.s, len);
	(*_loc)->user.len = len;

	(*_loc)->contacts = NULL;  /* We have no contacts yet */
	return TRUE;
}


/*
 * Add a contact into existing location structure
 * Contacts are sorted by q value, the highest q
 * value is the first
 */
int add_contact(location_t* _loc, const char* _contact, time_t _expires, float _q,
		const char* _callid, int _cseq)
{
	contact_t* c, *ptr, *prev;
#ifdef PARANOID
	if (!_loc) {
		LOG(L_ERR, "add_contact(): Invalid _loc parameter value\n");
		return FALSE;
	}
	
	if (!_contact) {
		LOG(L_ERR, "add_contact(): Invalid _contact parameter value\n");
		return FALSE;
	}
#endif

	if (create_contact(&c, &(_loc->user), _contact, _expires, _q, _callid, _cseq) == FALSE) {
		LOG(L_ERR, "add_contact(): Can't create contact structure\n");
		return FALSE;
	}
	
	ptr = _loc->contacts;
	prev = NULL;

	while (ptr) {
		if (ptr->q < _q) break;
		prev = ptr;
		ptr = ptr->next;
	}
	if (prev) {
		prev->next = c;
		c->next = ptr;
	} else {
		c->next = _loc->contacts;
		_loc->contacts = c;
	}
	
	return TRUE;
}


/*
 * Remove contact from existing location structure
 */
int remove_contact(location_t* _loc, const char* _contact)
{
	contact_t* ptr, *prev = NULL;
#ifdef PARANOID
	if (!_loc) {
		LOG(L_ERR, "remove_contact(): Invalid _loc parameter value\n");
		return FALSE;
	}

	if (!_contact) {
		LOG(L_ERR, "remove_contact(): Invalid _contact parameter value\n");
		return FALSE;
	}
#endif
	ptr = _loc->contacts;
	while(ptr) {
		if (!memcmp(_contact, ptr->c.s, ptr->c.len)) {
			if (prev) {
				prev->next = ptr->next;
			} else {
				_loc->contacts = ptr->next;
			}
			break;
		}
		prev = ptr;
		ptr = ptr->next;
	}

	if (ptr) {
		free_contact(ptr);
		return TRUE;
	} else {
		return FALSE;
	}
}


/*
 * Free memory allocated for given location structure
 */
void free_location(location_t* _loc)
{
	contact_t* ptr;

#ifdef PARANOID
	if (!_loc) return;
#endif
	if (_loc->user.s) pkg_free (_loc->user.s);  /* Free address of record string */

	ptr = _loc->contacts;          /* If there are any contacts */
	
	while(_loc->contacts) {        /* Free them all */
		ptr = _loc->contacts;
		_loc->contacts = ptr->next;
		free_contact(ptr);
	}

	pkg_free(_loc);
}


/*
 * Print location to stdout
 */
void print_location(const location_t* _loc)
{
	contact_t* ptr = _loc->contacts;

	printf("Address of record = \"%s\"\n", _loc->user.s);
	if (ptr) {
		printf("    Contacts:\n");
	} else {
		printf("    No contacts.\n");
		return;
	}

	while(ptr) {
		print_contact(ptr);
		ptr = ptr->next;
	}

	printf("\n");
}



int cmp_location(location_t* _loc, const char* _aor)
{
	return (strcmp(_loc->user.s, _aor));  /* For now */
}



/*
 * Check if the originating REGISTER message was formed correctly
 */
int validate_location(location_t* _loc, int _expires, int _star, int* _result)
{
#ifdef PARANOID
	if ((!_loc) || (!_result)) {
		LOG(L_ERR, "validate_location(): Invalid parameter value\n");
		return FALSE;
	}
#endif

	if (_star == 1) {
		if (_expires != 0) {
			*_result = FALSE;
			return TRUE;
		}

		if (_loc->contacts) {
			*_result = FALSE;
			return TRUE;
		}
	} else {
		     /* Anything is valid */
		*_result = TRUE;
		return TRUE;
	}
}


int remove_zero_expires(location_t* _loc)
{
	contact_t* ptr, *prev;
#ifdef PARANOID
	if (!_loc) {
		LOG(L_ERR, "remove_zero_expires(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	ptr = _loc->contacts;
	prev = NULL;
	while(ptr) {
		if (ptr->expires == 0) {
			if (prev) {
				prev->next = ptr->next;
			} else {
				_loc->contacts = ptr->next;
			}
			free_contact(ptr);
		}
		prev = ptr;
		ptr = ptr->next;
	}
	return TRUE;
}


/* 
 * =============== Database related functions
 */

#ifdef USE_DB
int db_insert_location(db_con_t* _c, location_t* _loc)
{
	contact_t* ptr;
	db_key_t keys[] = { "user", "contact", "expires", "q", "callid", "cseq"};
	db_val_t vals[] = {{DB_STRING,   0, {.string_val = NULL}},
			  {DB_STRING,   0, {.string_val = NULL}},
			  {DB_DATETIME, 0, {.time_val = 0}},
			  {DB_DOUBLE,   0, {.double_val = 0}},
			  {DB_STRING,   0, {.string_val = NULL}},
			  {DB_INT,      0, {.int_val = 0}}
	};

#ifdef PARANOID
	if (!_loc) {
		LOG(L_ERR, "db_insert_location(): Invalid parameter value\n");
		return FALSE;
	}

#endif
	ptr = _loc->contacts;

	while(ptr) {
		if (db_insert_contact(_c, ptr) == FALSE) {
			LOG(L_ERR, "db_insert_contact(): Error while inserting contact\n");
			return FALSE;
		}
		ptr = ptr->next;
	}

	return TRUE;
}
#endif

/*
 * Removes all bindings associated with the given
 * address of record
 */
#ifdef USE_DB
int db_remove_location(db_con_t* _c, location_t* _loc)
{
	db_key_t key[1] = {"user"};
	db_val_t val[1] = {{DB_STRING,   0, {.string_val = NULL}}};

#ifdef PARANOID
	if (!_loc) {
		LOG(L_ERR, "db_remove_location(): Invalid parameter value\n");
		return FALSE;
	}

#endif
	
	val[0].val.string_val = _loc->user.s;
	if (db_delete(_c, key, val, 1) == FALSE) {
		LOG(L_ERR, "db_remove_location(): Error while inserting binding\n");
		return FALSE;
	}

	return TRUE;
}
#endif


static int check_request_order(contact_t* _old, contact_t* _new)
{
#ifdef PARANOID
	if ((!_old) || (!_new)) {
		LOG(L_ERR, "check_request_order(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	if (!strcmp(_old->callid, _new->callid)) {
		if (_old->cseq >= _new->cseq) {
			return FALSE;
		}
	}
	return TRUE;
}


int update_location(db_con_t* _c, location_t* _dst, location_t* _src)
{
	int res;
	contact_t* src_ptr, *dst_ptr, *dst_ptr_prev, *src_ptr_prev;
#ifdef PARANOID
	if ((!_dst) || (!_src)) {
		LOG(L_ERR, "update_location(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	if (!_src->contacts) return TRUE;

	src_ptr = _src->contacts;
	src_ptr_prev = NULL;
	while(src_ptr) {

		dst_ptr = _dst->contacts;
		dst_ptr_prev = NULL;
		while(dst_ptr) {
			if (cmp_contact(dst_ptr, src_ptr) == TRUE) {
				if (check_request_order(dst_ptr, src_ptr) == TRUE) {  /* FIXME */
					DBG("update_location(): Order OK, updating\n");
					if (src_ptr->expires == 0) {
#ifdef USE_DB
						if (db_remove_contact(_c, dst_ptr) == FALSE) {
							LOG(L_ERR, "update_location(): Error while removing binding\n");
							return FALSE;
						}
#endif
						     /* FIXME */
						if (remove_contact(_dst, dst_ptr->c.s) == FALSE) {
							LOG(L_ERR, "update_location(): Error while removing from cache\n");
							return FALSE;
						}
					} else {
#ifdef USE_DB
						if (db_update_contact(_c, src_ptr) == FALSE) {
							LOG(L_ERR, "update_location(): Error while updating database\n");
							return FALSE;
						}
#endif
						if (update_contact(dst_ptr, src_ptr) == FALSE) {
							LOG(L_ERR, "update_location(): Error while updating cache\n");
							return FALSE;
						}
						     // Update binding
					}
					return TRUE;
				} else {
					DBG("update_location(): Request for binding update is out of order\n");
					goto skip;
				}
			}
			
			dst_ptr_prev = dst_ptr;
			dst_ptr = dst_ptr->next;
		}
		if (src_ptr->expires != 0) {
#ifdef USE_DB
			if (db_insert_contact(_c, src_ptr) == FALSE) {
				LOG(L_ERR, "update_location(): Error inserting into database\n");
				return FALSE;
			}
#endif
			if (add_contact(_dst, src_ptr->c. s, src_ptr->expires, src_ptr->q, src_ptr->callid, src_ptr->cseq) == FALSE) {
				LOG(L_ERR, "update_location(): Error while adding contact\n");
				return FALSE;
			}
		}
	skip:
		src_ptr_prev = src_ptr;
		src_ptr = src_ptr->next;
	}
	return TRUE;
}
