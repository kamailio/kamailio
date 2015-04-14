/* 
 * File:   ims_qos_stats.h
 * Author: jaybeepee
 *
 * Created on 24 February 2015, 11:15 AM
 */

#ifndef IMS_QOS_STATS_H
#define	IMS_QOS_STATS_H

#include "../../counters.h"

struct ims_qos_counters_h {
    counter_handle_t registration_aar_avg_response_time;
    counter_handle_t registration_aar_response_time;
    counter_handle_t registration_aar_timeouts;
    counter_handle_t failed_registration_aars;
    counter_handle_t registration_aars;
    counter_handle_t asrs;
    counter_handle_t successful_registration_aars;
    counter_handle_t active_registration_rx_sessions;
    counter_handle_t media_aar_avg_response_time;
    counter_handle_t media_aar_response_time;
    counter_handle_t media_aar_timeouts;
    counter_handle_t failed_media_aars;
    counter_handle_t media_aars;
    counter_handle_t successful_media_aars;
    counter_handle_t active_media_rx_sessions;
    counter_handle_t media_aar_replies_received;
    counter_handle_t registration_aar_replies_received;
};

int ims_qos_init_counters();
void ims_qos_destroy_counters();

#endif	/* IMS_QOS_STATS_H */



