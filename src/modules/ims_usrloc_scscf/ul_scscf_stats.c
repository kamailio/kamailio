#include "ul_scscf_stats.h"
#include "usrloc.h"
#include "dlist.h"

extern struct ims_subscription_list* ims_subscription_list;
extern struct contact_list* contact_list;

struct ul_scscf_counters_h ul_scscf_cnts_h;
enum ul_scscf_info_req { ULSCSF_SUBSCRIPTIONCOLLISIONS, ULSCSCF_CONTACT_COLLISIONS, ULSCSCF_IMPUCOLLISIONS };

static counter_val_t ims_usrloc_scscf_internal_stats(counter_handle_t h, void* what);

counter_def_t ul_scscf_cnt_defs[] = {
    {&ul_scscf_cnts_h.active_subscriptions, "active_subscriptions", 0, 0, 0, "active_subscriptions"},
    {&ul_scscf_cnts_h.subscription_collisions, "subscription_collisions", 0,
                ims_usrloc_scscf_internal_stats, (void*) (long) ULSCSF_SUBSCRIPTIONCOLLISIONS, "number of collisions in subscription hash"},
    {&ul_scscf_cnts_h.impu_collisions, "impu_collisions", 0,
                ims_usrloc_scscf_internal_stats, (void*) (long) ULSCSCF_IMPUCOLLISIONS, "number of collisions in impu hash"},
    {&ul_scscf_cnts_h.contact_collisions, "contact_collisions", 0,
                ims_usrloc_scscf_internal_stats, (void*) (long) ULSCSCF_CONTACT_COLLISIONS, "number of collisions in contact hash"},                
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

static str domain_str={"location", 8};
/** helper function for some stats (which are kept internally). to be used in future...
 */
static counter_val_t ims_usrloc_scscf_internal_stats(counter_handle_t h, void* what) {
    enum ul_scscf_info_req w;
    udomain_t* _d;

    w = (int) (long) what;
    switch (w) {
	case ULSCSF_SUBSCRIPTIONCOLLISIONS:
	    return ims_subscription_list->max_collisions;
        case ULSCSCF_CONTACT_COLLISIONS:
            return contact_list->max_collisions;
        case ULSCSCF_IMPUCOLLISIONS:
            if (find_domain(&domain_str, &_d) !=0) {
                return -1;
                //This is terrible - hardwiring domain to "location" but right now IMS is single domain but has the ability to extend to multiple domains later...
            };
            return _d->max_collisions;
    };
    return 0;
}
