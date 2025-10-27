/*-------------------------------------------------------------------------------------------------------------------\
|Version           Date        Author              Description                                                       |
|--------------------------------------------------------------------------------------------------------------------|
|Initial Build     20150825   T.Ntamane            Contact dialog handler for expired|terminated|destroyed| etc      |
\-----------------------------------------------------------------------------------------------------------------TN*/


#include "impurecord.h"
#include "usrloc.h"
#include "ucontact.h"
#include "dlist.h"
#include "../../modules/ims_dialog/dlg_load.h"
#include "../../modules/ims_dialog/dlg_hash.h"

extern ims_dlg_api_t dlgb;

/*------------------------------------------------------------------------------------\
| Our call back handler function that handles |confirmed|expired|terminated|destroy.  |
\------------------------------------------------------------------------------* V1.1*/

static void contact_dlg_handler(
		struct dlg_cell *dlg, int cb_types, struct dlg_cb_params *dlg_params);

/*-----------------------------------------------------------------------------------------------------------\
| Our setup call back handler function primarily for create, that further register for other callback       .|
\------------------------------------------------------------------------------------------------------V1.1-*/
void contact_dlg_create_handler(
		struct dlg_cell *dlg, int cb_types, struct dlg_cb_params *dlg_params)
{
	if(cb_types != DLGCB_CREATED) {
		LM_ERR("Unknown event type  %d", cb_types);
		return;
	}
	if((dlgb.register_dlgcb(dlg,
			   DLGCB_CONFIRMED | DLGCB_EXPIRED | DLGCB_TERMINATED
					   | DLGCB_DESTROY | DLGCB_FAILED,
			   contact_dlg_handler, 0, 0))

	) {
		LM_ERR("Error registering dialog for contact caller id [%.*s] ",
				dlg->callid.len, dlg->callid.s);
		return;
	}
	LM_DBG("Successfully registered contact dialog handler\n");
}


/** removes the default port ':5060' from the string in order compare later
 *  returns:
 *  0 => found
 *  1 => not found
 *  < 0 => error
 */
static int filter_default_port(str *src, str *dst)
{
	str default_port = str_init(":5060");
	char *port;

	if((port = str_search(src, &default_port))) {
		dst->len = port - src->s;
		memcpy(dst->s, src->s, dst->len);
		memcpy(dst->s + dst->len, port + 5, src->len - dst->len - 5);
		dst->len = src->len - 5;
		return 0;
	}
	if(dst->len < src->len) {
		LM_BUG("dst len < src len\n");
		return -1;
	}
	dst->len = src->len;
	memcpy(dst->s, src->s, src->len);
	return 1;
}

/**
 * Search for a contact related to an IMPU based on an original contact string (uri)
 * @param impu impurecord we will search through
 * @param search_aor the raw uri we are using as the source of the search
 * @return 0 on success, anything else on failure
 */
static inline int find_contact_from_impu(
		impurecord_t *impu, str *search_aor, ucontact_t **scontact)
{
	impu_contact_t *impucontact;
	short i_searchlen, c_searchlen, alias_searchlen;
	char *s_term;
	char *c_term;
	char *alias_term;
	char sbuf[512], cbuf[512];
	str str_aor = {sbuf, 512}, str_c = {cbuf, 512};
	if(!search_aor)
		return 1;

	if(filter_default_port(search_aor, &str_aor) < 0) {
		return 1;
	}

	LM_DBG("Looking for contact [%.*s] for IMPU [%.*s]\n", STR_FMT(&str_aor),
			STR_FMT(&impu->public_identity));


	/* Filter out sip: and anything before @ from search URI */
	s_term = strstr(str_aor.s, "@");
	if(!s_term) {
		s_term = strstr(str_aor.s, ":");
	}
	s_term += 1;
	if(s_term - str_aor.s >= str_aor.len) {
		goto error;
	}
	i_searchlen = str_aor.len - (s_term - str_aor.s);

	/* Compare the entire contact including alias, if not until alias IP */
	alias_term = strstr(s_term, "~");
	if(!alias_term) {
		alias_searchlen = i_searchlen;
	} else {
		alias_term += 1;
		alias_searchlen = alias_term - s_term;
	}

	impucontact = impu->linked_contacts.head;

	while(impucontact) {
		if(impucontact->contact) {
			// clean up previous contact
			str_c.len = 512;
			memset(str_c.s, 0, 512);

			if(filter_default_port(&impucontact->contact->c, &str_c) < 0) {
				LM_DBG("Skipping %.*s\n", STR_FMT(&impucontact->contact->c));
				impucontact = impucontact->next;
				continue;
			}
			c_term = strstr(str_c.s, "@");
			if(!c_term) {
				c_term = strstr(str_c.s, ":");
			}
			c_term += 1;
			c_searchlen = str_c.len - (c_term - str_c.s);
			LM_DBG("Comparing [%.*s] and [%.*s]\n", i_searchlen, s_term,
					c_searchlen, c_term);
			if(strncmp(c_term, s_term, i_searchlen) == 0) {
				*scontact = impucontact->contact;
				return 0;
			}
			if(alias_term) {
				LM_DBG("Comparing [%.*s] and [%.*s]\n", alias_searchlen, s_term,
						c_searchlen, c_term);
				if(strncmp(c_term, s_term, alias_searchlen) == 0) {
					*scontact = impucontact->contact;
					return 0;
				}
			}
			LM_DBG("Skipping %.*s\n", STR_FMT(&impucontact->contact->c));
		}
		impucontact = impucontact->next;
	}
error:
	LM_INFO("malformed contact, bailing search\n");
	return 1;
}

static void contact_dlg_handler(
		struct dlg_cell *dlg, int cb_types, struct dlg_cb_params *dlg_params)
{
	struct ucontact *ucontact_caller = 0x00, *ucontact_callee = 0x00;
	udomain_t *_d;
	impurecord_t *from_impu, *to_impu;
	str from_uri_clean, to_uri_clean;
	char *p;
	short iFoundCaller = 0, iFoundCallee = 0;
	static unsigned int i_confirmed_count = 0, i_terminated_count = 0;

	if((cb_types == DLGCB_CONFIRMED) || (cb_types == DLGCB_EXPIRED)
			|| (cb_types == DLGCB_TERMINATED) || (cb_types == DLGCB_DESTROY)
			|| (cb_types == DLGCB_FAILED)) {

		//for now we will abort if there is no dlg_out.... TODO maybe we can only do the caller side....
		if(dlg->dlg_entry_out.first == 0x00) {
			LM_DBG("no dlg out... ignoring!!! for type [%d] - usually happens "
				   "on failure response in dialog\n",
					cb_types);
			return;
		}
		register_udomain("location", &_d);

		from_uri_clean.s = dlg->from_uri.s;
		from_uri_clean.len = dlg->from_uri.len;
		p = memchr(dlg->from_uri.s, ';', dlg->from_uri.len);
		if(p)
			from_uri_clean.len = p - from_uri_clean.s;

		lock_udomain(_d, &from_uri_clean);
		if(get_impurecord(_d, &from_uri_clean, &from_impu) != 0) {
			LM_DBG("Could not find caller impu for [%.*s]\n",
					from_uri_clean.len, from_uri_clean.s);
			unlock_udomain(_d, &from_uri_clean);
			return;
		}

		if(find_contact_from_impu(
				   from_impu, &dlg->caller_contact, &ucontact_caller)
				!= 0) {
			LM_DBG("Unable to find caller contact from dialog.... "
				   "continuing\n");
			//unlock_udomain(_d, &from_uri_clean);
			//return;
		} else {
			iFoundCaller = 1;
		}
		unlock_udomain(_d, &from_uri_clean);

		to_uri_clean.s = dlg->dlg_entry_out.first->to_uri.s;
		to_uri_clean.len = dlg->dlg_entry_out.first->to_uri.len;
		p = memchr(dlg->dlg_entry_out.first->to_uri.s, ';',
				dlg->dlg_entry_out.first->to_uri.len);
		if(p)
			to_uri_clean.len = p - to_uri_clean.s;

		lock_udomain(_d, &to_uri_clean);
		if(get_impurecord(_d, &to_uri_clean, &to_impu) != 0) {
			LM_DBG("Could not find callee impu for [%.*s]\n", to_uri_clean.len,
					to_uri_clean.s);
			unlock_udomain(_d, &to_uri_clean);
			return;
		}
		if(find_contact_from_impu(to_impu,
				   &dlg->dlg_entry_out.first->callee_contact, &ucontact_callee)
				!= 0) {
			LM_DBG("Unable to find callee contact from dialog.... "
				   "continuing\n");
			//unlock_udomain(_d, &to_uri_clean);
			//return;
		} else {
			iFoundCallee = 1;
		}
		unlock_udomain(_d, &to_uri_clean);

	} else {
		LM_ERR("Unknown event type [%d] for callid [%.*s] ", cb_types,
				dlg->callid.len, dlg->callid.s);
		return;
	}
	if(!iFoundCaller && !iFoundCallee) {
		LM_ERR("No Contacts found for both caller && callee ... bailing\n");
		return;
	}
	switch(cb_types) {
		case DLGCB_CONFIRMED:
			LM_DBG("Confirmed contact of type [%d] ,caller_id [%.*s] from "
				   "handler ",
					cb_types, dlg->callid.len, dlg->callid.s);
			if(iFoundCaller)
				add_dialog_data_to_contact(
						ucontact_caller, dlg->h_entry, dlg->h_id);
			if(iFoundCallee)
				add_dialog_data_to_contact(ucontact_callee, dlg->h_entry,
						dlg->h_id); //dlg->dlg_entry_out.first->h_entry, dlg->dlg_entry_out.first->h_id);
			i_confirmed_count++;
			break;
		case DLGCB_FAILED:
		case DLGCB_DESTROY:
		case DLGCB_EXPIRED:
		case DLGCB_TERMINATED:
			LM_DBG("Terminated contact of type [%d] , caller_id [%.*s] from "
				   "handler ",
					cb_types, dlg->callid.len, dlg->callid.s);
			if(iFoundCaller)
				remove_dialog_data_from_contact(
						ucontact_caller, dlg->h_entry, dlg->h_id);
			if(iFoundCallee)
				//if (dlg->dlg_entry_out.first) {
				remove_dialog_data_from_contact(ucontact_callee, dlg->h_entry,
						dlg->h_id); //dlg->dlg_entry_out.first->h_entry, dlg->dlg_entry_out.first->h_id);
			//}
			i_terminated_count++;
			break;
	}
}
