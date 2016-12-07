/* 
 * File:   ims_charging_stats.h
 * Author: jaybeepee
 *
 * Created on 24 February 2015, 11:15 AM
 */

#ifndef IMS_CHARGING_STATS_H
#define	IMS_CHARGING_STATS_H

#include "../../counters.h"

struct ims_charging_counters_h {
    counter_handle_t billed_secs;
    counter_handle_t ccr_avg_response_time;
    counter_handle_t ccr_response_time;
    counter_handle_t ccr_timeouts;
    counter_handle_t failed_final_ccrs;
    counter_handle_t failed_initial_ccrs;
    counter_handle_t failed_interim_ccr;
    counter_handle_t final_ccrs;
    counter_handle_t initial_ccrs;
    counter_handle_t interim_ccrs;
    counter_handle_t killed_calls;
    counter_handle_t successful_final_ccrs;
    counter_handle_t successful_initial_ccrs;
    counter_handle_t successful_interim_ccrs;
    counter_handle_t active_ro_sessions;
    counter_handle_t ccr_replies_received;
};

int ims_charging_init_counters();
void ims_charging_destroy_counters();

#endif	/* IMS_CHARGING_STATS_H */



