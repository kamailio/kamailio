#include "ul_scscf_stats.h"

struct ul_scscf_counters_h ul_scscf_cnts_h;

counter_def_t ul_scscf_cnt_defs[] = {
    {&ul_scscf_cnts_h.active_subscriptions,	    "active_subscriptions",	0, 0, 0,			    "number of registered subscribers (IMPIs)"},
    {&ul_scscf_cnts_h.active_impus,                 "active_impus",             0, 0, 0,			    "number of registered IMPUs"},
    {&ul_scscf_cnts_h.active_contacts,	    "active_contacts",	0, 0, 0,			    "number of registered contacts"},
    {0, 0, 0, 0, 0, 0}
};

int ul_scscf_init_counters() {
    if (counter_register_array("ims_usrloc_scscf", ul_scscf_cnt_defs) < 0)
	goto error;
    return 0;
error:
    return -1;
}

void ul_scscf_destroy_counters() {
    
}
