/*
 * $Id$
 *
 * Convenience usrloc header file
 */

#ifndef USRLOC_H
#define USRLOC_H


#include "dlist.h"
#include "udomain.h"
#include "urecord.h"
#include "ucontact.h"


/* dlist.h interface */
typedef int  (*register_domain_t) (const char* _n, udomain_t** _d);

/* domain.h interface */
typedef int  (*insert_record_t)   (udomain_t* _d, struct urecord* _r);
typedef int  (*delete_record_t)   (udomain_t* _d, str* _a);
typedef int  (*get_record_t)      (udomain_t* _d, str* _a, struct urecord** _r);
typedef void (*release_record_t)  (struct urecord* _r);

/* record.h interface */
typedef int  (*new_record_t)      (str* _a, urecord_t** _r);
typedef void (*free_record_t)     (urecord_t* _r); 
typedef int  (*insert_contact_t)  (urecord_t* _r, str* _c, time_t _e, float _q, str* _cid, int _cs);
typedef int  (*delete_contact_t)  (urecord_t* _r, ucontact_t* _c);
typedef int  (*get_contact_t)     (urecord_t* _r, str* _c, ucontact_t** _co);

/* contact.h interface */
typedef int  (*update_contact_t)  (ucontact_t* _c, time_t _e, float _q, str* _cid, int _cs);


struct usrloc_func {
	register_domain_t register_domain_f;

	insert_record_t   insert_record_f;
	delete_record_t   delete_record_f;
	get_record_t      get_record_f;
	release_record_t  release_record_f;

	new_record_t      new_record_f;
	free_record_t     free_record_f;
	insert_contact_t  insert_contact_f;
	delete_contact_t  delete_contact_f;
	get_contact_t     get_contact_f;

	update_contact_t  update_contact_f;
};


extern struct usrloc_func ul_func;

#define ul_register_domain (ul_func.register_domain_f)

#define ul_insert_record   (ul_func.insert_record_f)
#define ul_delete_record   (ul_func.delete_record_f)
#define ul_get_record      (ul_func.get_record_f)
#define ul_release_record  (ul_func.release_record_f)

#define ul_new_record      (ul_func.new_record_f)
#define ul_free_record     (ul_func.free_record_f)
#define ul_insert_contact  (ul_func.insert_contact_f)
#define ul_delete_contact  (ul_func.delete_contact_f)
#define ul_get_contact     (ul_func.get_contact_f)

#define ul_update_contact  (ul_func.update_contact_f)

int bind_usrloc(void);

#endif /* USRLOC_H */
