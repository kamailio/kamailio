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

#define TRUE 1
#define FALSE 0

location_t* msg2loc(struct sip_msg* _msg)
{
	location_t* loc;
	char* to, *contact;
	char* to_url;

#ifdef PARANOID
	if (!_msg) return NULL;
#endif
	
	if (parse_headers(_msg, HDR_TO | HDR_CONTACT ) == -1) {
		LOG(L_ERR, "msg2loc(): Error while parsing headers\n");
		return NULL;
	} else {
		if (!_msg->to) {
			LOG(L_ERR, "msg2loc(): Unable to find To header field\n");
			return NULL;
		}

		if (!_msg->contact) {
			LOG(L_ERR, "msg2loc(): Unable to find Contact header field\n");
			return NULL;
		}

		to = (char*)malloc(_msg->to->name.len + 1);
		contact = (char*)malloc(_msg->contact->name.len + 1);
		if ((!to) || (!contact)) {
			LOG(L_ERR, "msg2loc(): No memory left\n");
			free(to);
			free(contact);
			return NULL;
		}

		_msg->to = remove_crlf(_msg->to);
		_msg->contact = remove_crlf(_msg->contact);

		memcpy(to, _msg->to->name.s, _msg->to->name.len);
		memcpy(contact, _msg->contact->name.s, _msg->contact->name.len);

		to_url = parse_to(to);
		if (!to_url) {
			LOG(L_ERR, "msg2loc(): Error while parsing To header field \n");
			free(to);
			free(contact);
			return NULL;
		}
			
		loc = create_location(to_url, FALSE, time(NULL) + DEFAULT_EXPIRES);
		if (!loc) {
			LOG(L_ERR, "msg2loc(): Unable to create location\n");
			free(to);
			free(contact);
			return NULL;
		}

		if (parse_contact_field(contact, loc) == FALSE) {
			LOG(L_ERR, "msg2loc(): Error while parsing Contact header field\n");
			free(to);
			free(contact);
			free(loc);
			return NULL;
		}
		free(contact);
		free(to);
		return loc;
	}
}


location_t* create_location(const char* _user, int _star, time_t _expires)
{
	location_t* ptr;
	int len;
#ifdef PARANOID
	if ((!_user) || (!_user)) {
		LOG(L_ERR, "create_loc(): Invalid _user parameter\n");
		return FALSE;
	}
#endif

	ptr = (location_t*)malloc(sizeof(location_t));

	if (!ptr) {
		LOG(L_ERR, "create_loc(): No memory left\n");
		return FALSE;
	}

	len = strlen(_user);
	ptr->user.s = (char*)malloc(len + 1);
	memcpy(ptr->user.s, _user, len + 1);
	strlower(ptr->user.s, len);
	ptr->user.len = len;

	ptr->star = _star;
	ptr->expires = _expires;
	ptr->contacts = NULL;
	return ptr;
}


int add_contact(location_t* _loc, const char* _contact, time_t _expire, float _q,
		unsigned char _new, unsigned char _dirty)
{
	contact_t* c, *ptr, *prev;
#ifdef PARANOID
	if (!_loc) return -1;
#endif
	c = create_contact(&(_loc->user), _contact, _expire, _q, _new, _dirty, FALSE);
	if (!c) {
		DBG("add_contact(): Can't create contact");
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


int remove_contact(location_t* _loc, const char* _contact)
{
	contact_t* ptr, *prev = NULL;
#ifdef PARANOID
	if ((!_loc) || (!_contact)) {
		LOG(L_ERR, "remove_contact(): Invalid parameter(s)\n");
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
 * Free memory allocated for given location
 */
void free_location(location_t* _loc)
{
	contact_t* ptr;

	if (!_loc) return;

	if (_loc->user.s) free (_loc->user.s);  /* Free address of record string */

	ptr = _loc->contacts;          /* If there are any contacts */
	
	while(_loc->contacts) {        /* Free them all */
		ptr = _loc->contacts;
		_loc->contacts = ptr->next;
		free_contact(ptr);
	}
}


/*
 * Print location to stdout
 */
void print_location(const location_t* _loc)
{
	contact_t* ptr = _loc->contacts;

	printf("Address of record = \"%s\"\n", _loc->user.s);
	printf("    Expires = \"%s\"\n", ctime(&(_loc->expires)));
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



int merge_location(location_t* _new, const location_t* _old)
{
	contact_t* ptr1, *ptr2;
	
	ptr1 = _old->contacts;

	while(ptr1) {
		ptr2 = _new->contacts;
		while(ptr2) {
			if (!cmp_contact(ptr1, ptr2)) {
				ptr2->new = ptr1->new;
				if ((ptr1->expire != ptr2->expire) ||
				    (ptr1->q != ptr2->q)) {
					ptr2->dirty = TRUE;
				}
				break;
			}

			ptr2 = ptr2->next;
		}
		if (!ptr2) {
			if (add_contact(_new, ptr1->c.s, ptr1->expire,
					ptr1->q, ptr1->new, ptr1->dirty) == FALSE) {
				LOG(L_ERR, "merge_location(): Error while merging locations\n");
				
			}
		}
		ptr1 = ptr1->next;
	}

	return TRUE;
}
