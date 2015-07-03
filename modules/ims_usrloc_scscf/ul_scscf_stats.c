#include "ul_scscf_stats.h"
#include "usrloc.h"

extern struct ims_subscription_list* ims_subscription_list;

struct ul_scscf_counters_h ul_scscf_cnts_h;
enum ul_scscf_info_req { ULSCSF_SUBSCRIPTIONCOUNT };

//static counter_val_t ims_usrloc_scscf_internal_stats(counter_handle_t h, void* what);

counter_def_t ul_scscf_cnt_defs[] = {
    {&ul_scscf_cnts_h.active_subscriptions, "active_subscriptions", 0, 0, 0, "active_subscriptions"},
//    {&ul_scscf_cnts_h.active_subscriptions, "active_subscriptions", 0,
    //            ims_usrloc_scscf_internal_stats, (void*) (long) ULSCSF_SUBSCRIPTIONCOUNT, "number of registered subscribers (IMPIs)"},
    {&ul_scscf_cnts_h.active_impus, "active_impus", 0, 0, 0, "number of registered IMPUs"},
    {&ul_scscf_cnts_h.active_contacts, "active_contacts", 0, 0, 0, "number of registered contacts"},
    {0, 0, 0, 0, 0, 0}};

int ul_scscf_init_counters() {
    if (counter_register_array("ims_usrloc_scscf", ul_scscf_cnt_defs) < 0)
	goto error;
    return 0;
error:
    return -1;
}

void ul_scscf_destroy_counters() {
    
}

/** helper function for some stats (which are kept internally). to be used in future...
 */
//static counter_val_t ims_usrloc_scscf_internal_stats(counter_handle_t h, void* what) {
//    enum ul_scscf_info_req w;
//
//    w = (int) (long) what;
//    switch (w) {
//	case ULSCSF_SUBSCRIPTIONCOUNT:
//	    return ims_subscription_list->subscriptions;
//    };
//    return 0;
//}
