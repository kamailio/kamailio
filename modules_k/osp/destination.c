/*
 * openser osp module.
 *
 * This module enables openser to communicate with an Open Settlement
 * Protocol (OSP) server.  The Open Settlement Protocol is an ETSI
 * defined standard for Inter-Domain VoIP pricing, authorization
 * and usage exchange.  The technical specifications for OSP
 * (ETSI TS 101 321 V4.1.1) are available at www.etsi.org.
 *
 * Uli Abend was the original contributor to this module.
 *
 * Copyright (C) 2001-2005 Fhg Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../usr_avp.h"
#include "destination.h"
#include "usage.h"

/* Name of AVP item of OSP */
str OSP_ORIGDEST_LABEL = {"_orig_osp_dests_", 16};
str OSP_TERMDEST_LABEL = {"_term_osp_dests_", 16};

static int ospSaveDestination(osp_dest* dest, str* label);
static osp_dest* ospGetLastOrigDestination(void);
static void ospRecordCode(int code, osp_dest* dest);
static int ospIsToReportUsage(int code);
    
/*
 * Initialize destination structure
 * param dest Destination data structure
 * return initialized destination sturcture
 */
osp_dest* ospInitDestination(
    osp_dest* dest)
{
    LOG(L_DBG, "osp: ospInitDestion\n");

    memset(dest, 0, sizeof(osp_dest));

    dest->callidsize = sizeof(dest->callid);
    dest->tokensize = sizeof(dest->token);

    LOG(L_DBG, "osp: callidsize '%d' tokensize '%d'\n", dest->callidsize, dest->tokensize);

    return dest;
}

/* 
 * Save destination as an AVP
 *     name - OSP_ORIGDEST_LABEL / OSP_TERMDEST_LABEL
 *     value - osp_dest wrapped in a string
 * param dest Destination structure
 * param label Name
 * return 0 success, -1 failure
 */
static int ospSaveDestination(
    osp_dest* dest, 
    str* label)
{
    str wrapper;
    int result = -1;

    LOG(L_DBG, "osp: ospSaveDestination\n");

    wrapper.s = (char*)dest;
    wrapper.len = sizeof(osp_dest);

    /* 
     * add_avp will make a private copy of both the name and value in shared memory 
     * which will be released by TM at the end of the transaction
     */
    if (add_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)*label, (int_str)wrapper) == 0) {
        result = 0;
        LOG(L_DBG, "osp: saved\n");
    } else {
        LOG(L_ERR, "osp: ERROR: failed to save destination\n");
    }

    return result;
}

/*
 * Save originate destination
 * param dest Originate destination structure
 * return 0 success, -1 failure
 */
int ospSaveOrigDestination(
    osp_dest* dest
    )
{
    LOG(L_DBG, "osp: ospSaveOrigDestination\n");

    return ospSaveDestination(dest, &OSP_ORIGDEST_LABEL);
}

/*
 * Save terminate destination
 * param dest Terminate destination structure
 * return 0 success, -1 failure
 */
int ospSaveTermDestination(
    osp_dest* dest)
{
    LOG(L_DBG, "osp: ospSaveTermDestination\n");

    return ospSaveDestination(dest, &OSP_TERMDEST_LABEL);
}

/* 
 * Check if there is an unused and supported originate destination from an AVP
 *     name - OSP_ORIGDEST_LABEL
 *     value - osp_dest wrapped in a string
 *     search unused (used==0) & supported (support==1)
 * return 0 success, -1 failure
 */
int ospCheckOrigDestination(void)
{
    struct usr_avp* destavp = NULL;
    int_str destval;
    osp_dest* dest = NULL;
    int result = -1;

    LOG(L_DBG, "osp: ospCheckOrigDestination\n");

    for (destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_ORIGDEST_LABEL, NULL, 0);
        destavp != NULL;
        destavp = search_next_avp(destavp, NULL))
    {
        get_avp_val(destavp, &destval);

        /* OSP destintaion is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        if (dest->used == 0) {
            if (dest->supported == 1) {
                LOG(L_DBG, "osp: found\n");
                result = 0;
                break;
            } else {
                LOG(L_DBG, "osp: this destination does not been supported\n");
            }
        } else {
            LOG(L_DBG, "osp: this destination has already been used\n");
        }
    }

    if (result == -1) {
        LOG(L_DBG, "osp: there is no unused destination\n");
    }

    return result;
}

/* 
 * Retrieved an unused originate destination from an AVP
 *     name - OSP_ORIGDEST_LABEL
 *     value - osp_dest wrapped in a string
 *     There can be 0, 1 or more originate destinations. 
 *     Find the 1st unused destination (used==0), return it, 
 *     and mark it as used (used==1).
 * return NULL on failure
 */
osp_dest* ospGetNextOrigDestination(void)
{
    struct usr_avp* destavp = NULL;
    int_str destval;
    osp_dest* dest = NULL;
    osp_dest* result = NULL;

    LOG(L_DBG, "osp: ospGetNextOrigDestination\n");

    for (destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_ORIGDEST_LABEL, NULL, 0);
        destavp != NULL;
        destavp = search_next_avp(destavp, NULL))
    {
        get_avp_val(destavp, &destval);

        /* OSP destintaion is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        if (dest->used == 0) {
            if (dest->supported == 1) {
                LOG(L_DBG, "osp: found\n");
                dest->used = 1;
                result = dest;
                break;
            } else {
                /* Make it looks like used */
                dest->used = 1;
                /* 111 means wrong protocol */
                dest->lastcode = 111;
                LOG(L_DBG, "osp: this destination does not been supported\n");
            }
        } else {
            LOG(L_DBG, "osp: this destination has already been used\n");
        }
    }

    if (result == NULL) {
        LOG(L_DBG, "osp: there is no unused destination\n");
    }

    return result;
}

/* 
 * Retrieved the terminate destination from an AVP
 *     name - OSP_TERMDEST_LABEL
 *     value - osp_dest wrapped in a string
 *     There can be 0 or 1 term destinations. Find and return it.
 *  return NULL on failure (no terminate destination)
 */
osp_dest* ospGetTermDestination(void)
{
    struct usr_avp* destavp = NULL;
    int_str destval;
    osp_dest* dest = NULL;

    LOG(L_DBG, "osp: ospGetTermDestination\n");

    destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_TERMDEST_LABEL, NULL, 0);

    if (destavp) {
        get_avp_val(destavp, &destval);

        /* OSP destination is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        LOG(L_DBG, "osp: found\n");
    }

    return dest;
}

/*
 * Retrieved the last used originate destination from an AVP
 *    name - OSP_ORIGDEST_LABEL
 *    value - osp_dest wrapped in a string
 *    There can be 0, 1 or more destinations. 
 *    Find the last used destination (used==1),
 *    and return it.
 *  return NULL on failure
 */
static osp_dest* ospGetLastOrigDestination(void)
{
    struct usr_avp* destavp = NULL;
    int_str destval;
    osp_dest* dest = NULL;
    osp_dest* lastdest = NULL;

    LOG(L_DBG, "osp: ospGetLastOrigDesintaion\n");

    for (destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_ORIGDEST_LABEL, NULL, 0);
        destavp != NULL;
        destavp = search_next_avp(destavp, NULL))
    {
        get_avp_val(destavp, &destval);

        /* OSP destination is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        if (dest->used == 1) {
            lastdest = dest;
            LOG(L_DBG, "osp: curent destination '%s'\n", lastdest->host);
        } else {
            break;
        }
    }

    return lastdest;
}

/*
 * Retrieved the forked destination from an AVP
 *    name - OSP_ORIGDEST_LABEL
 *    value - osp_dest wrapped in a string
 *    There can be 0, 1 or more destinations. 
 *    Find the used (used==1), has the same dest item as the parameter fork,
 *    and return it.
 *    Note, there maybe multiple result destinations. osp_fork can be exten to
 *    deal with it in the future. 
 * param fork Forked destination index
 * return NULL on failure
 */
osp_dest* ospGetForkedDestination(
    osp_fork* fork)
{
    struct usr_avp* destavp = NULL;
    int_str destval;
    osp_dest* dest = NULL;
    osp_dest* forked = NULL;
    char* host;

    LOG(L_DBG, "osp: ospGetForkedDesintaion\n");

    for (destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_ORIGDEST_LABEL, NULL, 0);
        destavp != NULL;
        destavp = search_next_avp(destavp, NULL))
    {
        get_avp_val(destavp, &destval);

        /* OSP destination is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        if (dest->used == 1) {
            if (*dest->host == '[') {
                host = dest->host + 1;
            } else {
                host = dest->host ;
            }
            if (memcmp(host, fork->host, strlen(fork->host)) == 0) {
                LOG(L_DBG, "osp: found\n");
                forked = dest;
                break;
            }
        }
    }

    return forked;
}

/*
 * Record destination status
 * param code Destination status
 * param dest Destination
 */
static void ospRecordCode(
    int code, 
    osp_dest* dest)
{
    LOG(L_DBG, "osp: ospRecordCode\n");
    LOG(L_DBG, "osp: code '%d'\n", code);

    dest->lastcode = code;

    switch (code) {
        case 100:
            if (!dest->time100) {
                dest->time100 = time(NULL);
            } else {
                LOG(L_DBG, "osp: 100 has already been recorded\n");
            }
            break;
        case 180:
        case 181:
        case 182:
        case 183:
            if (!dest->time180) {
                dest->time180 = time(NULL);
            } else {
                LOG(L_DBG, "osp: 180, 181, 182 or 183 has allready been recorded\n");
            }
            break;
        case 200:
        case 202:
            if (!dest->time200) {
                dest->time200 = time(NULL);
            } else {
                LOG(L_DBG, "osp: 200 or 202 has allready been recorded\n");
            }
            break;
        default:
            LOG(L_DBG, "osp: will not record time for '%d'\n", code);
    }
}

/*
 * Check destination status for reporting usage
 * param code Destination status
 * return 
 */
static int ospIsToReportUsage(
    int code)
{
    int istime = -1;

    LOG(L_DBG, "osp: ospIsToReportUsage\n");
    LOG(L_DBG, "osp: code '%d'\n", code);

    if (code >= 200) {
        istime = 0;
    }

    return istime;
}

/*
 * Report call setup usage for both client and server side
 * param clientcode Client status
 * param servercode Server status
 */
void ospRecordEvent(
    int clientcode, 
    int servercode)
{
    osp_dest* dest;

    LOG(L_DBG, "osp: ospRecordEvent\n");
    LOG(L_DBG, "osp: client status '%d' server status '%d'\n", clientcode, servercode);

    if ((clientcode != 0) && (dest = ospGetLastOrigDestination())) {
        ospRecordCode(clientcode, dest);

        if (ospIsToReportUsage(servercode) == 0) {
            ospReportOrigSetupUsage();
        }
    }

    if ((servercode != 0) && (dest = ospGetTermDestination())) {
        ospRecordCode(servercode, dest);

        if (ospIsToReportUsage(servercode) == 0) {
            ospReportTermSetupUsage();
        }
    }
}

/*
 * Dump destination information
 * param dest Destination
 */
void ospDumpDestination(osp_dest* dest)
{
    LOG(L_DBG, "osp: dest->host..........'%s'\n", dest->host);
    LOG(L_DBG, "osp: dest->used..........'%d'\n", dest->used);
    LOG(L_DBG, "osp: dest->lastcode......'%d'\n", dest->lastcode);
    LOG(L_DBG, "osp: dest->time100.......'%d'\n", (unsigned int)dest->time100);
    LOG(L_DBG, "osp: dest->time180.......'%d'\n", (unsigned int)dest->time180);
    LOG(L_DBG, "osp: dest->time200.......'%d'\n", (unsigned int)dest->time200);
}

/*
 * Dump all destination information
 */
void ospDumpAllDestination(void)
{
    struct usr_avp* destavp = NULL;
    int_str destval;
    osp_dest* dest = NULL;
    int count = 0;

    LOG(L_DBG, "osp: ospDumpAllDestination\n");

    for (destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_ORIGDEST_LABEL, NULL, 0);
        destavp != NULL;
        destavp = search_next_avp(destavp,NULL))
    {
        get_avp_val(destavp, &destval);

        /* OSP destination is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        LOG(L_DBG, "osp: ....originate '%d'....\n", count++);

        ospDumpDestination(dest);
    }
    if (count == 0) {
        LOG(L_DBG, "osp: there is no originate destination AVP\n");
    }

    destavp = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, (int_str)OSP_TERMDEST_LABEL, NULL, 0);

    if (destavp) {
        get_avp_val(destavp, &destval);

        /* OSP destination is wrapped in a string */
        dest = (osp_dest*)destval.s.s;

        LOG(L_DBG, "osp: ....destination....\n");

        ospDumpDestination(dest);
    } else {
        LOG(L_DBG, "osp: there is no terminate destination AVP\n");
    }
}

