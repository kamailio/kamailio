/*
 * stats.c
 *
 *  Created on: Sep 30, 2013
 *      Author: carlos
 */

#include "stats.h"

unsigned long get_failed_initial_ccrs() {

	unsigned long success_number = get_stat_val(successful_initial_ccrs),
		 total_number	= get_stat_val(initial_ccrs);

	return total_number - success_number;
}

unsigned long get_failed_interim_ccrs() {

	unsigned long success_number = get_stat_val(successful_interim_ccrs),
		 total_number	= get_stat_val(interim_ccrs);

	return total_number - success_number;
}

unsigned long get_failed_final_ccrs() {

	unsigned long success_number = get_stat_val(successful_final_ccrs),
		 total_number	= get_stat_val(final_ccrs);

	return total_number - success_number;
}

unsigned long get_ccr_avg_response_time() {

	unsigned long responses_time = get_stat_val(ccr_responses_time),
				  ccrs	= get_stat_val(initial_ccrs)
				  	  	  + get_stat_val(interim_ccrs)
				  	  	  + get_stat_val(final_ccrs);

	if (responses_time == 0 || ccrs == 0)
		return 0;

	return responses_time / ccrs;
}
