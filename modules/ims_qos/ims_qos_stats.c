/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 *
 *
 *
 * History:
 * --------
 *  2011-02-02  initial version (jason.penton)
 */


#include "ims_qos_stats.h"

struct ims_qos_counters_h ims_qos_cnts_h;
enum sctp_info_req { IMS_QOS_REGISTRATION_AAR_AVG_RSP, IMS_QOS_MEDIA_AAR_AVG_RSP };

static counter_val_t ims_qos_internal_stats(counter_handle_t h, void* what);

counter_def_t ims_qos_cnt_defs[] = {
    {&ims_qos_cnts_h.active_registration_rx_sessions,	    "active_registration_rx_sessions",		0, 0, 0,	    "number of currently active registration Rx sessions"},
    {&ims_qos_cnts_h.registration_aar_avg_response_time,    "registration_aar_avg_response_time",	0, ims_qos_internal_stats, (void*) (long) IMS_QOS_REGISTRATION_AAR_AVG_RSP,	"avg response time for registration AARs"},
    {&ims_qos_cnts_h.registration_aar_timeouts,		    "registration_aar_timeouts",		0, 0, 0,	    "total number of registration AAR timeouts"},
    {&ims_qos_cnts_h.failed_registration_aars,		    "failed_registration_aars",			0, 0, 0,	    "total number of failed registration AARs"},
    {&ims_qos_cnts_h.registration_aars,			    "registration_aars",			0, 0, 0,	    "total number of registration AARs"},
    {&ims_qos_cnts_h.asrs,				    "asrs",					0, 0, 0,	    "total number of registration ASRs"},
    {&ims_qos_cnts_h.successful_registration_aars,	    "successful_registration_aars",		0, 0, 0,	    "total number of successful registration AARs"},
    {&ims_qos_cnts_h.registration_aar_response_time,	    "registration_aar_response_time",		0, 0, 0,	    "total number of seconds waiting for registration AAR responses"},
    {&ims_qos_cnts_h.registration_aar_replies_received,	    "registration_aar_replies_received",	0, 0, 0,            "total number of registration AAR replies received"},

    {&ims_qos_cnts_h.active_media_rx_sessions,		    "active_media_rx_sessions",		0, 0, 0,		    "number of currently active media Rx sessions"},
    {&ims_qos_cnts_h.media_aar_avg_response_time,	    "media_aar_avg_response_time",	0, ims_qos_internal_stats, (void*) (long) IMS_QOS_MEDIA_AAR_AVG_RSP,	"avg response time for media AARs"},
    {&ims_qos_cnts_h.media_aar_timeouts,		    "media_aar_timeouts",		0, 0, 0,		    "total number of media AAR timeouts"},
    {&ims_qos_cnts_h.failed_media_aars,			    "failed_media_aars",		0, 0, 0,		    "total number of failed media AARs"},
    {&ims_qos_cnts_h.media_aars,			    "media_aars",			0, 0, 0,		    "total number of media AARs"},
    {&ims_qos_cnts_h.successful_media_aars,		    "successful_media_aars",		0, 0, 0,		    "total number of successful media AARs"},
    {&ims_qos_cnts_h.media_aar_response_time,		    "media_aar_response_time",		0, 0, 0,		    "total number of seconds waiting for media AAR responses"},
    {&ims_qos_cnts_h.media_aar_replies_received,	    "media_aar_replies_received",	0, 0, 0,                    "total number of media AAR replies received"},
    {0, 0, 0, 0, 0, 0}
};


int ims_qos_init_counters() {
    if (counter_register_array("ims_qos", ims_qos_cnt_defs) < 0)
	goto error;
    return 0;
error:
    return -1;
}

void ims_qos_destroy_counters() {
    
}

/** helper function for some stats (which are kept internally).
 */
static counter_val_t ims_qos_internal_stats(counter_handle_t h, void* what) {
    enum sctp_info_req w;

    w = (int) (long) what;
    switch (w) {
	case IMS_QOS_REGISTRATION_AAR_AVG_RSP:
	    if (counter_get_val(ims_qos_cnts_h.registration_aars) == 0) 
		return 0;
	    else
		return counter_get_val(ims_qos_cnts_h.registration_aar_response_time)/counter_get_val(ims_qos_cnts_h.registration_aars);
	    break;
	case IMS_QOS_MEDIA_AAR_AVG_RSP:
	    if (counter_get_val(ims_qos_cnts_h.media_aars) == 0) 
		return 0;
	    else
		return counter_get_val(ims_qos_cnts_h.media_aar_response_time)/counter_get_val(ims_qos_cnts_h.media_aars);
	    break;
	default:
	    return 0;
    };
    return 0;
}
