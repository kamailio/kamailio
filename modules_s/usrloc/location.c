/* 
 * $Id$ 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "location.h"
#include "../../str.h"
#include "utils.h"
#include "log.h"
#include "../../parser/msg_parser.h"
#include "const.h"
#include "to_parser.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "defs.h"
#include "contact_parser.h"
#include "sh_malloc.h"

/*
 * Function prototypes
 */

static inline int get_expires(struct sip_msg* _msg);
static inline int parse_all_headers(struct sip_msg* _msg);
static inline int process_all_contacts(location_t* _loc, struct sip_msg* _msg, int _expires, int* _star,
				       const char* _callid, int _cseq);
static inline int get_CSeq(struct sip_msg* _msg, int* _cseq);
static inline int get_CallID(struct sip_msg* _msg, char** _callid);



/*
 * Get expires HF value, if there is no
 * Expires HF, use default value instead
 */
static inline int get_expires(struct sip_msg* _msg)
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
		ERR("Invalid parameter value");
		return FALSE;
	}
#endif
	if (parse_headers(_msg, HDR_EOH) == -1) {
		ERR("Error while parsing headers");
		return FALSE;
	}

	     /* To: HF contains Addres of Record, this will be
	      * needed
	      */
	if (!_msg->to) {
		ERR("Unable to find To header field");
		return FALSE;
	}
	
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
		ERR("Invalid parameter value");
		return FALSE;
	}
#endif
	*_star = 0;
	contact = (char*)pkg_malloc(MAX_CONTACT_LEN);
	if (!contact) {
		ERR("No memory left");
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
				ERR("Error while parsing Contact header field");
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
		ERR("Invalid parameter value");
		return FALSE;
	}
#endif

	*_cseq = atoi(get_cseq(_msg)->number.s);

	return TRUE;
}


static inline int get_CallID(struct sip_msg* _msg, char** _callid)
{
	int len;
#ifdef PARANOID
	if ((!_msg) || (!_callid)) {
		ERR("Invalid parameter value");
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
int sip_to_loc(struct sip_msg* _msg, location_t** _loc, int* _star, int* _expires)
{
	char* callid;
	int cseq;
	str to;
	
#ifdef PARANOID
	if ((!_msg) || (!_loc) || (!_star) || (!_expires)) {
		ERR("Invalid parameter value");
		return FALSE;
	}
#endif

	if (parse_all_headers(_msg) == FALSE) {
		ERR("Error while parsing message headers");
		return FALSE;
	}

	     /* Extract username from To URI */
	to.s = get_to(_msg)->uri.s;
	to.len = get_to(_msg)->uri.len;
	get_username(&to);
	     /* Not needed anymore */
	    
	if (!to.len) {
		ERR("Error while parsing To header field");
		return FALSE;
	}

	if (create_location(_loc, &to) == FALSE) {
		ERR("Unable to create location structure");
		return FALSE;
	}

	*_expires = get_expires(_msg);
	
	if (get_CSeq(_msg, &cseq) == FALSE) {
		ERR("Unable to get CSeq value");
		free_location(*_loc);
		return FALSE;
	}

	if (get_CallID(_msg, &callid) == FALSE) {
		ERR("Unable to get Call-ID value");
		free_location(*_loc);
		return FALSE;
	}

	if (process_all_contacts(*_loc, _msg, *_expires, _star, callid, cseq) == FALSE) {
		ERR("Error while processing Contact field");
		free_location(*_loc);
		return FALSE;
	}
		
	return TRUE;
}


/*
 * Create a new location structure
 */
int create_location(location_t** _loc, str* _user)
{
#ifdef PARANOID
	if ((!_loc) || (!_user)) {
		ERR("Invalid parameter value");
		return FALSE;
	}
#endif
	*_loc = (location_t*)sh_malloc(sizeof(location_t));
	if (!(*_loc)) {
		ERR("No memory left");
		return FALSE;
	}

	(*_loc)->user.s = (char*)sh_malloc(_user->len + 1);
	if (!((*_loc)->user.s)) {
		ERR("No memory left");
		sh_free(*_loc);
		return FALSE;
	}

	memcpy((*_loc)->user.s, _user->s, _user->len);
	(*_loc)->user.s[_user->len] = '\0';

	     /* We are case insensitive */
	strlower((*_loc)->user.s, _user->len);
	(*_loc)->user.len = _user->len;

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
		ERR("Invalid _loc parameter value");
		return FALSE;
	}
	
	if (!_contact) {
		ERR("Invalid _contact parameter value");
		return FALSE;
	}
#endif

	if (create_contact(&c, &(_loc->user), _contact, _expires, _q, _callid, _cseq) == FALSE) {
		ERR("Can't create contact structure");
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
		ERR("Invalid _loc parameter value");
		return FALSE;
	}

	if (!_contact) {
		ERR("Invalid _contact parameter value");
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
	if (_loc->user.s) sh_free (_loc->user.s);  /* Free address of record string */

	ptr = _loc->contacts;          /* If there are any contacts */
	
	while(_loc->contacts) {        /* Free them all */
		ptr = _loc->contacts;
		_loc->contacts = ptr->next;
		free_contact(ptr);
	}

	sh_free(_loc);
}


/*
 * Print location to stdout
 */
void print_location(const location_t* _loc)
{
	contact_t* ptr = _loc->contacts;

	INFO("Address of record = \"%s\"", _loc->user.s);
	if (ptr) {
		INFO("    Contacts:");
	} else {
		INFO("    No contacts.");
		return;
	}

	while(ptr) {
		print_contact(ptr);
		ptr = ptr->next;
	}
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
		ERR("Invalid parameter value");
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
	}	
	     /* Anything is valid */
	*_result = TRUE;
	return TRUE;
}


int remove_zero_expires(location_t* _loc)
{
	contact_t* ptr, *prev, *tmp;
#ifdef PARANOID
	if (!_loc) {
		ERR("remove_zero_expires(): Invalid parameter value");
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

			tmp = ptr;
			ptr = ptr->next;
			free_contact(tmp);
		} else {
			prev = ptr;
			ptr = ptr->next;
		}
	}
	return TRUE;
}


int db_insert_location(db_con_t* _c, location_t* _loc)
{
	contact_t* ptr, *p;
#ifdef PARANOID
	if (!_loc) {
		ERR("Invalid parameter value");
		return FALSE;
	}

#endif
	ptr = _loc->contacts;

	while(ptr) {
		if (db_insert_contact(_c, ptr) == FALSE) {

			p = _loc->contacts;
			while(p != ptr) {
				db_remove_contact(_c, p);
				p = p->next;
			}

			ERR("Error while inserting location");
			return FALSE;
		}
		ptr = ptr->next;
	}

	return TRUE;
}


/*
 * Removes all bindings associated with the given
 * address of record
 */
int db_remove_location(db_con_t* _c, location_t* _loc)
{
	db_key_t key[1] = {"user"};
	db_val_t val[1] = {{DB_STRING,   0, {.string_val = NULL}}};

#ifdef PARANOID
	if (!_loc) {
		ERR("Invalid parameter value");
		return FALSE;
	}

#endif
	
	val[0].val.string_val = _loc->user.s;
	if (db_delete(_c, key, val, 1) == FALSE) {
		ERR("Error while inserting binding");
		return FALSE;
	}

	return TRUE;
}


static int check_request_order(contact_t* _old, contact_t* _new)
{
#ifdef PARANOID
	if ((!_old) || (!_new)) {
		ERR("Invalid parameter value");
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


static int rem_cont(db_con_t* _c, location_t* _dst, contact_t* _dcon)
{
	if (_c) {
		if (db_remove_contact(_c, _dcon) == FALSE) {
			ERR("Error while removing binding");
			return FALSE;
		}
	}
	     /* FIXME, zbytecne se hleda */
	if (remove_contact(_dst, _dcon->c.s) == FALSE) {
		ERR("Error while removing from cache");
		return FALSE;
	}

	return TRUE;
}


static int upd_cont(db_con_t* _c, contact_t* _dcon, contact_t* _scon) 
{
	if (_c) {
		if (db_update_contact(_c, _scon) == FALSE) {
			ERR("Error while updating database");
			return FALSE;
		}
	}

	if (update_contact(_dcon, _scon) == FALSE) {
		ERR("Error while updating cache");
		return FALSE;
	}
	return TRUE;
}


static int ins_cont(db_con_t* _c, location_t* _dst, contact_t* _scon) 
{
	if (_c) {
		if (db_insert_contact(_c, _scon) == FALSE) {
			ERR("Error inserting into database");
			return FALSE;
		}
	}
	if (add_contact(_dst, _scon->c.s, _scon->expires, _scon->q, _scon->callid, _scon->cseq) == FALSE) {
		ERR("Error while adding contact");
		return FALSE;
	}
	return TRUE;
}



/* 
 * FIXME: Pravdepodobne by bylo jednodussi nevytvaret src location
 *        zpracovavat kontakty rovnou jakmile se parsujou
 */
int update_location(db_con_t* _c, location_t* _dst, location_t* _src, int* _sr)
{
	contact_t* src_ptr = _src->contacts, *dst_ptr, *dst_ptr_prev, *src_ptr_prev = NULL;


	*_sr = 1; /* By default send reply */
	while(src_ptr) {
		dst_ptr = _dst->contacts;
		dst_ptr_prev = NULL;
		while(dst_ptr) {
			if (cmp_contact(dst_ptr, src_ptr) == TRUE) {
				if (check_request_order(dst_ptr, src_ptr) == TRUE) {  /* FIXME */
					DBG("Order OK, updating");
					if (src_ptr->expires == 0) {
						if (rem_cont(_c, _dst, dst_ptr) == FALSE) return FALSE;
					} else {
						if (upd_cont(_c, dst_ptr, src_ptr) == FALSE) return FALSE;
					}
				} else {
					INFO("Request for binding update is out of order");
					*_sr = 0; /* Request is out of order, do not send reply */
				}
				goto skip;
			}
			
			dst_ptr_prev = dst_ptr;
			dst_ptr = dst_ptr->next;
		}
		if (src_ptr->expires != 0) {
			if (ins_cont(_c, _dst, src_ptr) == FALSE) return FALSE;
		}
	skip:
		src_ptr_prev = src_ptr;
		src_ptr = src_ptr->next;
	}
	return TRUE;
}



int clean_location(location_t* _l, db_con_t* _c, time_t _t)
{
	contact_t* ptr, *prev, *tmp;

#ifdef PARANOID
	if (!_l) {
		ERR("Invalid parameter value");
		return FALSE;
	}
#endif

	ptr = _l->contacts;
	prev = NULL;

	while(ptr) {
	        DBG("Checking entry: \"%s\"", _l->user.s);
		if (ptr->expires < _t) {
			DBG("Contact %s,%s expired, removing", ptr->aor->s, ptr->c.s);
			if (_c) {
				if (db_remove_contact(_c, ptr) == FALSE) {
					ERR("Error while removing contact from database");
					return FALSE;
				}
				DBG("Contact removed from database");
			}
			if (prev) {
				prev->next = ptr->next;
			} else {
				_l->contacts = ptr->next;
			}

			tmp = ptr;
			ptr = ptr->next;
			free_contact(tmp);
			DBG("Contact removed from cache\n");
		} else {
			DBG("Contact is still fresh");
			prev = ptr;
			ptr = ptr->next;
		}
	}
	return TRUE;
}
