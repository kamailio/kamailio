#ifndef CDP_STATS_H
#define	CDP_STATS_H

#include "../../counters.h"

struct cdp_counters_h {
    counter_handle_t timeout;
    counter_handle_t replies_received;
    counter_handle_t replies_response_time;
    counter_handle_t avg_response_time;
    counter_handle_t queuelength;
};

int cdp_init_counters();
void cdp_destroy_counters();

#endif	/* CDP_STATS_H */

