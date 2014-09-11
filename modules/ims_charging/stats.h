/*
 * stats.h
 *
 *  Created on: Sep 30, 2013
 *      Author: carlos
 */

#ifndef STATS_H_
#define STATS_H_

#include "../../lib/kcore/statistics.h"

extern stat_var *initial_ccrs;
extern stat_var *interim_ccrs;
extern stat_var *final_ccrs;
extern stat_var *successful_initial_ccrs;
extern stat_var *successful_interim_ccrs;
extern stat_var *successful_final_ccrs;
extern stat_var *ccr_responses_time;
extern stat_var *billed_secs;
extern stat_var *killed_calls;
extern stat_var *ccr_timeouts;

unsigned long get_failed_initial_ccrs();
unsigned long get_failed_interim_ccrs();
unsigned long get_failed_final_ccrs();
unsigned long get_ccr_avg_response_time();

#endif /* STATS_H_ */
