/*
 * $Id$
 *
 * Usrloc interface
 */

#include "../usrloc/usrloc.h"
#include "../../sr_module.h"


struct usrloc_func ul_func;


int bind_usrloc(void)
{
	ul_register_domain = (register_domain_t)find_export("~ul_register_domain", 1);
	if (ul_register_domain == 0) return -1;

	ul_insert_record = (insert_record_t)find_export("~ul_insert_record", 1);
	if (ul_insert_record == 0) return -1;

	ul_delete_record = (delete_record_t)find_export("~ul_delete_record", 1);
	if (ul_delete_record == 0) return -1;

	ul_get_record = (get_record_t)find_export("~ul_get_record", 1);
	if (ul_get_record == 0) return -1;

        ul_release_record = (release_record_t)find_export("~ul_release_record", 1);
	if (ul_release_record == 0) return -1;

	ul_new_record = (new_record_t)find_export("~ul_new_record", 1);
	if (ul_new_record == 0) return -1;

	ul_free_record = (free_record_t)find_export("~ul_free_record", 1);
	if (ul_free_record == 0) return -1;

	ul_insert_contact = (insert_contact_t)find_export("~ul_insert_contact", 1);
	if (ul_insert_contact == 0) return -1;

	ul_delete_contact = (delete_contact_t)find_export("~ul_delete_contact", 1);
	if (ul_delete_contact == 0) return -1;

	ul_get_contact = (get_contact_t)find_export("~ul_get_contact", 1);
	if (ul_get_contact == 0) return -1;

	ul_update_contact = (update_contact_t)find_export("~ul_update_contact", 1);
	if (ul_update_contact == 0) return -1;

	return 0;
}
