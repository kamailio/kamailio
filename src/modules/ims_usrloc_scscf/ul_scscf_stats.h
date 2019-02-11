/* 
 * File:   ul_scscf_stats.h
 * Author: jaybeepee
 *
 * Created on 24 May 2015, 11:15 AM
 */

#ifndef UL_SCSCF_STATS_H
#define	UL_SCSCF_STATS_H

#include "../../core/counters.h"

extern struct ul_scscf_counters_h ul_scscf_cnts_h;

struct ul_scscf_counters_h {
    counter_handle_t active_subscriptions;
    counter_handle_t active_impus;
    counter_handle_t active_contacts;
    counter_handle_t expired_contacts;
    counter_handle_t subscription_collisions;
    counter_handle_t impu_collisions;
    counter_handle_t contact_collisions;
};

int ul_scscf_init_counters();
void ul_scscf_destroy_counters();

#endif	/* UL_SCSCF_STATS_H */



