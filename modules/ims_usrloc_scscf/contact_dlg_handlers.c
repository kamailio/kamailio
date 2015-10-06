/*-------------------------------------------------------------------------------------------------------------------\
|Version           Date        Author              Description                                                       |
|--------------------------------------------------------------------------------------------------------------------|
|Initial Build     20150825   T.Ntamane            Contact dialog handler for expired|terminated|destroyed| etc      |
\-----------------------------------------------------------------------------------------------------------------TN*/


#include "impurecord.h"
#include "usrloc.h"
#include "ucontact.h"
#include "dlist.h"
#include "../../modules/dialog_ng/dlg_load.h"
#include "../../modules/dialog_ng/dlg_hash.h"

extern struct dlg_binds dlgb;

/*------------------------------------------------------------------------------------\
| Our call back handler function that handles |confirmed|expired|terminated|destroy.  |
\------------------------------------------------------------------------------* V1.1*/

static void contact_dlg_handler(struct dlg_cell* dlg, int cb_types, struct dlg_cb_params *dlg_params);

/*-----------------------------------------------------------------------------------------------------------\
| Our setup call back handler function primarily for create, that further register for other callback       .|
\------------------------------------------------------------------------------------------------------V1.1-*/
void contact_dlg_create_handler(struct dlg_cell* dlg, int cb_types, struct dlg_cb_params *dlg_params) {
    if (cb_types != DLGCB_CREATED) {
        LM_ERR("Unknown event type  %d", cb_types);
        return;
    }
    if ((dlgb.register_dlgcb(dlg, DLGCB_CONFIRMED | DLGCB_EXPIRED | DLGCB_TERMINATED | DLGCB_DESTROY | DLGCB_FAILED, contact_dlg_handler, 0, 0))

            ) {
        LM_ERR("Error registering dialog for contact caller id [%.*s] ", dlg->callid.len, dlg->callid.s);
        return;
    }
    LM_DBG("Successfully registered contact dialog handler\n");
}

/**
 * Search for a contact related to an IMPU based on an original contact string (uri)
 * @param impu impurecord we will search through
 * @param search_aor the raw uri we are using as the source of the search
 * @return 0 on success, anything else on failure
 */
static inline int find_contact_from_impu(impurecord_t* impu, str* search_aor, ucontact_t** scontact) {
    
    int iCount = 0;
    short i_searchlen;
    char *s_term;
    
    if (!search_aor)
        return 1;
    
    LM_DBG("Looking for contact [%.*s] for IMPU [%.*s]\n", search_aor->len, search_aor->s, impu->public_identity.len, impu->public_identity.s);
    
    s_term = memchr(search_aor->s,'@',search_aor->len);
    if (!s_term)
    {
        LM_DBG("Malformed contact...bailing search\n"); 
        return 1;
    }        
    i_searchlen = s_term - search_aor->s;
    for (;iCount < impu->num_contacts ; ++iCount)
    {                
        if( impu->newcontacts[iCount] && impu->newcontacts[iCount]->aor.s[i_searchlen] == '@' 
                                      && (memcmp(impu->newcontacts[iCount]->aor.s, search_aor->s, i_searchlen) == 0))           
        {
            *scontact = impu->newcontacts[iCount];  
            return 0;
        }        
        if (impu->newcontacts[iCount])
           LM_DBG("Skipping %.*s\n",impu->newcontacts[iCount]->aor.len,impu->newcontacts[iCount]->aor.s);
    }    
    return 1;
}

static void contact_dlg_handler(struct dlg_cell* dlg, int cb_types, struct dlg_cb_params *dlg_params) {
    struct ucontact *ucontact_caller = 0x00,
            *ucontact_callee = 0x00;
    udomain_t *_d;
    impurecord_t* from_impu, *to_impu;
    str from_uri_clean, to_uri_clean;
    char *p;
    short iFoundCaller = 0,
          iFoundCallee  = 0;
    static unsigned int i_confirmed_count  = 0,
                        i_terminated_count =0;

    if ((cb_types == DLGCB_CONFIRMED) ||
            (cb_types == DLGCB_EXPIRED) ||
            (cb_types == DLGCB_TERMINATED) ||
            (cb_types == DLGCB_DESTROY) ||
            (cb_types == DLGCB_FAILED)) {

        //for now we will abort if there is no dlg_out.... TODO maybe we can only do the caller side....
        if (dlg->dlg_entry_out.first == 0x00) {
            LM_ERR("no dlg out... ignoring!!! for type [%d]\n",cb_types);
            return;
        }
        register_udomain("location", &_d);
        
        from_uri_clean.s = dlg->from_uri.s;
        from_uri_clean.len = dlg->from_uri.len;
        p = memchr(dlg->from_uri.s, ';', dlg->from_uri.len);
        if (p)
            from_uri_clean.len = p - from_uri_clean.s;
        
        lock_udomain(_d, &from_uri_clean);
        if (get_impurecord(_d, &from_uri_clean, &from_impu) != 0) {
            LM_DBG("Could not find caller impu for [%.*s]\n", from_uri_clean.len, from_uri_clean.s);
            unlock_udomain(_d, &from_uri_clean);
            return;
        }
        
        if (find_contact_from_impu(from_impu, &dlg->caller_contact, &ucontact_caller) !=0) {
            LM_DBG("Unable to find caller contact from dialog.... continuing\n");
            //unlock_udomain(_d, &from_uri_clean);
            //return;
        }
        else {
           iFoundCaller = 1;
        }
        unlock_udomain(_d, &from_uri_clean);
        
        to_uri_clean.s = dlg->dlg_entry_out.first->to_uri.s;
        to_uri_clean.len = dlg->dlg_entry_out.first->to_uri.len;
        p = memchr(dlg->dlg_entry_out.first->to_uri.s, ';', dlg->dlg_entry_out.first->to_uri.len);
        if (p)
            to_uri_clean.len = p - to_uri_clean.s;
        
        lock_udomain(_d, &to_uri_clean);
        if (get_impurecord(_d, &to_uri_clean, &to_impu) != 0) {
            LM_DBG("Could not find callee impu for [%.*s]\n", to_uri_clean.len, to_uri_clean.s);
            unlock_udomain(_d, &to_uri_clean);
            return;
        }
        if (find_contact_from_impu(to_impu, &dlg->dlg_entry_out.first->callee_contact, &ucontact_callee) !=0) {
            LM_DBG("Unable to find callee contact from dialog.... continuing\n");
            //unlock_udomain(_d, &to_uri_clean);
            //return;
        }
        else{            
           iFoundCallee = 1;
        }
        unlock_udomain(_d, &to_uri_clean);
        
    } else {
        LM_ERR("Unknown event type [%d] for callid [%.*s] ", cb_types, dlg->callid.len, dlg->callid.s);
        return;
    }
    if(!iFoundCaller && !iFoundCallee)
    {
        LM_ERR("No Contacts found for both caller && callee ... bailing\n");
        return;
    }
    switch (cb_types) {
        case DLGCB_CONFIRMED:
            LM_DBG("Confirmed contact of type [%d] ,caller_id [%.*s] from handler ", cb_types, dlg->callid.len, dlg->callid.s);
            if (iFoundCaller)
              add_dialog_data_to_contact(ucontact_caller, dlg->h_entry, dlg->h_id);
            if(iFoundCallee)
              add_dialog_data_to_contact(ucontact_callee, dlg->h_entry, dlg->h_id);//dlg->dlg_entry_out.first->h_entry, dlg->dlg_entry_out.first->h_id);
            i_confirmed_count++;
            break;
        case DLGCB_FAILED:
        case DLGCB_DESTROY:
        case DLGCB_EXPIRED:
        case DLGCB_TERMINATED:
            LM_DBG("Terminated contact of type [%d] , caller_id [%.*s] from handler ", cb_types, dlg->callid.len, dlg->callid.s);
            if(iFoundCaller)
               remove_dialog_data_from_contact(ucontact_caller, dlg->h_entry, dlg->h_id);
            if(iFoundCallee)
              //if (dlg->dlg_entry_out.first) {
                remove_dialog_data_from_contact(ucontact_callee, dlg->h_entry, dlg->h_id);//dlg->dlg_entry_out.first->h_entry, dlg->dlg_entry_out.first->h_id);
            //}
            i_terminated_count++;
            break;
    }
    if (i_confirmed_count > 100 && !(i_confirmed_count % 100))
    {
        LM_ERR("Curent confirmed count %d terminated count %d \n",i_confirmed_count, i_terminated_count);
    }
}
