#include "ims_charging_stats.h"

struct ims_charging_counters_h ims_charging_cnts_h;
enum sctp_info_req { IMS_CHARGING_AVG_RSP, IMS_CHARGING_FAILED_INITIAL, IMS_CHARGING_FAILED_FINAL, IMS_CHARGING_FAILED_INTERIM };

static counter_val_t ims_charging_internal_stats(counter_handle_t h, void* what);

counter_def_t ims_charging_cnt_defs[] = {
    {&ims_charging_cnts_h.active_ro_sessions,	    "active_ro_sessions",	0, 0, 0,			    "number of currently active Ro sessions"},
    {&ims_charging_cnts_h.billed_secs,		    "billed_secs",		0, 0, 0,			    "total number of seconds billed since start or reset"},
    {&ims_charging_cnts_h.ccr_avg_response_time,    "ccr_avg_response_time",	0, ims_charging_internal_stats, (void*) (long) IMS_CHARGING_AVG_RSP,	"avg response time for CCRs"},
    {&ims_charging_cnts_h.ccr_response_time,	    "ccr_response_time",	0, 0, 0,			    "total number of seconds waiting for CCR responses"},
    {&ims_charging_cnts_h.ccr_timeouts,		    "ccr_timeouts",		0, 0, 0,			    "total number of CCR timeouts"},
    {&ims_charging_cnts_h.failed_final_ccrs,	    "failed_final_ccrs",	0, 0, 0,			    "total number of failed final CCRs"},
    {&ims_charging_cnts_h.failed_initial_ccrs,	    "failed_initial_ccrs",	0, 0, 0,			    "total number of failed initial CCRs"},
    {&ims_charging_cnts_h.failed_interim_ccr,	    "failed_interim_ccrs",	0, 0, 0,			    "total number of failed interim CCRs"},
    {&ims_charging_cnts_h.final_ccrs,		    "final_ccrs",		0, 0, 0,			    "total number of final (terminating) CCRs"},
    {&ims_charging_cnts_h.initial_ccrs,		    "initial_ccrs",		0, 0, 0,			    "total number of initial CCRs"},
    {&ims_charging_cnts_h.interim_ccrs,		    "interim_ccrs",		0, 0, 0,			    "total number of interim CCRs"},
    {&ims_charging_cnts_h.killed_calls,		    "killed_calls",		0, 0, 0,			    "total number of killed calls"},
    {&ims_charging_cnts_h.successful_final_ccrs,    "successful_final_ccrs",	0, 0, 0,			    "total number of successful final CCRs"},
    {&ims_charging_cnts_h.successful_initial_ccrs,  "successful_initial_ccrs",	0, 0, 0,			    "total number of successful initial CCRs"},
    {&ims_charging_cnts_h.successful_interim_ccrs,   "successful_interim_ccrs",	0, 0, 0,			    "total number of successful interim CCRs"},
    {&ims_charging_cnts_h.ccr_replies_received,     "ccr_replies_received",     0, 0, 0,                            "total number of CCR replies received"},
    {0, 0, 0, 0, 0, 0}
};

int ims_charging_init_counters() {
    if (counter_register_array("ims_charging", ims_charging_cnt_defs) < 0)
	goto error;
    return 0;
error:
    return -1;
}

void ims_charging_destroy_counters() {
    
}

/** helper function for some stats (which are kept internally).
 */
static counter_val_t ims_charging_internal_stats(counter_handle_t h, void* what) {
    enum sctp_info_req w;

    w = (int) (long) what;
    switch (w) {
	case IMS_CHARGING_AVG_RSP:
	    if ((counter_get_val(ims_charging_cnts_h.initial_ccrs) + counter_get_val(ims_charging_cnts_h.interim_ccrs) + counter_get_val(ims_charging_cnts_h.final_ccrs)) == 0) 
		return 0;
	    else
		return counter_get_val(ims_charging_cnts_h.ccr_response_time)/
			(counter_get_val(ims_charging_cnts_h.initial_ccrs) + counter_get_val(ims_charging_cnts_h.interim_ccrs) + counter_get_val(ims_charging_cnts_h.final_ccrs));
	    break;
	case IMS_CHARGING_FAILED_INITIAL:
	    return (counter_get_val(ims_charging_cnts_h.initial_ccrs) - counter_get_val(ims_charging_cnts_h.successful_initial_ccrs));
	    break;
	case IMS_CHARGING_FAILED_INTERIM:
	    return (counter_get_val(ims_charging_cnts_h.interim_ccrs) - counter_get_val(ims_charging_cnts_h.successful_interim_ccrs));
	    break;
	case IMS_CHARGING_FAILED_FINAL:
	    return (counter_get_val(ims_charging_cnts_h.final_ccrs) - counter_get_val(ims_charging_cnts_h.successful_final_ccrs));
	    break;
	default:
	    return 0;
    };
    return 0;
}
