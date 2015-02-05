#include "cdp_stats.h"

struct cdp_counters_h cdp_cnts_h;
enum sctp_info_req { CDP_AVG_RSP };

static counter_val_t cdp_internal_stats(counter_handle_t h, void* what);

counter_def_t cdp_cnt_defs[] = {
    {&cdp_cnts_h.timeout, "timeout", 0, 0, 0,
	"number of timeouts on CDP requests"},
    {&cdp_cnts_h.replies_received, "replies_received", 0, 0, 0,
	"total number of replies received"},
    {&cdp_cnts_h.replies_response_time, "replies_response_time", 0, 0, 0,
	"total time waiting for replies"},
    {&cdp_cnts_h.queuelength, "queuelength", 0, 0, 0,
	"current length of worker queue tasks"},
    {0, "average_response_time", 0,
	cdp_internal_stats, (void*) (long) CDP_AVG_RSP,
	"average response time for CDP replies"},
    {0, 0, 0, 0, 0, 0}
};

int cdp_init_counters() {
    if (counter_register_array("cdp", cdp_cnt_defs) < 0)
	goto error;
    return 0;
error:
    return -1;
}

void cdp_destroy_counters() {
    
}

/** helper function for some stats (which are kept internally).
 */
static counter_val_t cdp_internal_stats(counter_handle_t h, void* what) {
    enum sctp_info_req w;

    w = (int) (long) what;
    switch (w) {
	case CDP_AVG_RSP:
	    if (counter_get_val(cdp_cnts_h.replies_received) == 0) 
		return 0;
	    else
		return counter_get_val(cdp_cnts_h.replies_response_time)/counter_get_val(cdp_cnts_h.replies_received);
    };
    return 0;
}