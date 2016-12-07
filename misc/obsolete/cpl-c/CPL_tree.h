/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _CPL_TREE_DEFINITION_H
#define _CPL_TREE_DEFINITION_H



#define              CPL_NODE   1
#define         INCOMING_NODE   2
#define         OUTGOING_NODE   3
#define        ANCILLARY_NODE   4
#define        SUBACTION_NODE   5
#define   ADDRESS_SWITCH_NODE   6
#define          ADDRESS_NODE   7
#define             BUSY_NODE   8
#define          DEFAULT_NODE   9
#define          FAILURE_NODE  10
#define              LOG_NODE  11
#define           LOOKUP_NODE  12
#define         LOCATION_NODE  13
#define         LANGUAGE_NODE  14
#define  LANGUAGE_SWITCH_NODE  15
#define             MAIL_NODE  16
#define         NOTFOUND_NODE  17
#define         NOANSWER_NODE  18
#define            PROXY_NODE  19
#define         PRIORITY_NODE  20
#define  PRIORITY_SWITCH_NODE  21
#define           REJECT_NODE  22
#define         REDIRECT_NODE  23
#define      REDIRECTION_NODE  24
#define  REMOVE_LOCATION_NODE  25
#define              SUB_NODE  26
#define          SUCCESS_NODE  27
#define           STRING_NODE  28
#define    STRING_SWITCH_NODE  29
#define             TIME_NODE  30
#define      TIME_SWITCH_NODE  31
#define        OTHERWISE_NODE  32
#define      NOT_PRESENT_NODE  33



/* attributes and values fro ADDRESS-SWITCH node */
#define  FIELD_ATTR                  0       /*shared with STRING_SWITCH*/
#define  SUBFIELD_ATTR               1
#define  ORIGIN_VAL                  0
#define  DESTINATION_VAL             1
#define  ORIGINAL_DESTINATION_VAL    2
#define  ADDRESS_TYPE_VAL            0
#define  USER_VAL                    1
#define  HOST_VAL                    2
#define  PORT_VAL                    3
#define  TEL_VAL                     4
#define  DISPLAY_VAL                 5

/* attributes and values for ADDRESS node */
#define  IS_ATTR                     0    /*shared with STRING*/
#define  CONTAINS_ATTR               1    /*shared with STRING*/
#define  SUBDOMAIN_OF_ATTR           2

/* attributes and values for STRING-SWITCH node */
#define  SUBJECT_VAL                 0
#define  ORGANIZATION_VAL            1
#define  USER_AGENT_VAL              2
#define  DISPALY_VAL                 3

/* attributes and values for LANGUAGE node */
#define  MATCHES_TAG_ATTR            0
#define  MATCHES_SUBTAG_ATTR         1

/* attributes and values for TIME-SWITCH node */
#define  TZID_ATTR                   0
#define  TZURL_ATTR                  1

/* attributes and values for TIME node */
#define  DTSTART_ATTR                0
#define  DTEND_ATTR                  1
#define  DURATION_ATTR               2
#define  FREQ_ATTR                   3
#define  INTERVAL_ATTR               4
#define  UNTIL_ATTR                  5
#define  COUNT_ATTR                  6
#define  BYSECOND_ATTR               7
#define  BYMINUTE_ATTR               8
#define  BYHOUR_ATTR                 9
#define  BYDAY_ATTR                 10
#define  BYMONTHDAY_ATTR            11
#define  BYYEARDAY_ATTR             12
#define  BYWEEKNO_ATTR              13
#define  BYMONTH_ATTR               14
#define  WKST_ATTR                  15
#define  BYSETPOS_ATTR              16

/* attributes and values for PRIORITY node */
#define  LESS_ATTR                   0
#define  GREATER_ATTR                1
#define  EQUAL_ATTR                  2
#define  PRIOSTR_ATTR                3
#define  EMERGENCY_VAL               0
#define  EMERGENCY_STR               "emergency"
#define  EMERGENCY_STR_LEN           (sizeof(EMERGENCY_STR)-1)
#define  URGENT_VAL                  1
#define  URGENT_STR                  "urgent"
#define  URGENT_STR_LEN              (sizeof(URGENT_STR)-1)
#define  NORMAL_VAL                  2
#define  NORMAL_STR                  "normal"
#define  NORMAL_STR_LEN              (sizeof(NORMAL_STR)-1)
#define  NON_URGENT_VAL              3
#define  NON_URGENT_STR              "non-urgent"
#define  NON_URGENT_STR_LEN          (sizeof(NON_URGENT_STR)-1)
#define  UNKNOWN_PRIO_VAL            4

/* attributes and values for LOCATION node */
#define  URL_ATTR                    0
#define  PRIORITY_ATTR               1
#define  CLEAR_ATTR                  2    /*shared with LOOKUP node*/
#define  NO_VAL                      0    /*shared with LOOKUP node*/
#define  YES_VAL                     1    /*shared with LOOKUP node*/

/* attributes and values for LOOKUP node */
#define  SOURCE_ATTR                 0
#define  TIMEOUT_ATTR                1    /*shared with PROXY node*/
#define  SOURCE_REG_STR              "registration"
#define  SOURCE_REG_STR_LEN          (sizeof("registration")-1)

/* attributes and values for REMOVE_LOCATION node */
#define  LOCATION_ATTR               0
#define  PARAM_ATTR                  1
#define  VALUE_ATTR                  2

/* attributes and values for PROXY node */
#define  RECURSE_ATTR                2
#define  ORDERING_ATTR               3
#define  PARALLEL_VAL                0
#define  SEQUENTIAL_VAL              1
#define  FIRSTONLY_VAL               2

/* attributes and values for REDIRECT node */
#define  PERMANENT_ATTR              0

/* attributes and values for REJECT node */
#define  STATUS_ATTR                 0
#define  REASON_ATTR                 1
#define  BUSY_VAL                    486
#define  BUSY_STR                    "busy"
#define  BUSY_STR_LEN                (sizeof(BUSY_STR)-1)
#define  NOTFOUND_VAL                404
#define  NOTFOUND_STR                "notfound"
#define  NOTFOUND_STR_LEN            (sizeof(NOTFOUND_STR)-1)
#define  REJECT_VAL                  603
#define  REJECT_STR                  "reject"
#define  REJECT_STR_LEN              (sizeof(REJECT_STR)-1)
#define  ERROR_VAL                   500
#define  ERROR_STR                   "error"
#define  ERROR_STR_LEN               (sizeof(ERROR_STR)-1)

/* attributes and values for LOG node */
#define  NAME_ATTR                   0
#define  MAX_NAME_SIZE               32
#define  COMMENT_ATTR                1
#define  MAX_COMMENT_SIZE            128

/* attributes and values for EMAIL node */
#define  TO_ATTR                     0
#define  SUBJECT_ATTR                1
#define  SUBJECT_EMAILHDR_STR        "subject"
#define  SUBJECT_EMAILHDR_LEN        (sizeof(SUBJECT_EMAILHDR_STR)-1)
#define  BODY_ATTR                   2
#define  BODY_EMAILHDR_STR           "body"
#define  BODY_EMAILHDR_LEN           (sizeof(BODY_EMAILHDR_STR)-1)
#define  URL_MAILTO_STR              "mailto:"
#define  URL_MAILTO_LEN              (sizeof(URL_MAILTO_STR)-1)

/* attributes and values for SUB node */
#define  REF_ATTR                    0



/* node = | type(1) | nr_kids(1) | nr_attrs(1) | unused(1) |
 *        | x*kids_offset(2) | y*attrs(2*n) |
 */

#define      NODE_TYPE(_p)            ( *((unsigned char*)(_p)) )
#define      NR_OF_KIDS(_p)           ( *((unsigned char* )((_p)+1)) )
#define      NR_OF_ATTR(_p)           ( *((unsigned char* )((_p)+1+1)) )
#define      KID_OFFSET_PTR(_p,_n)    ( (unsigned short*)((_p)+4+2*(_n)) )
#define      ATTR_PTR(_p)             ( (_p)+4+2*NR_OF_KIDS(_p) )
#define      SIMPLE_NODE_SIZE(_p)     ( 4+2*NR_OF_KIDS(_p) )
#define      GET_NODE_SIZE(_n)        ( 4+2*(_n) )
#define      BASIC_ATTR_SIZE          4

#define      SET_KID_OFFSET(_p,_n,_o) *KID_OFFSET_PTR(_p,_n)=htons(_o)
#define      KID_OFFSET(_p,_n)        ntohs(*KID_OFFSET_PTR(_p,_n))




#endif

