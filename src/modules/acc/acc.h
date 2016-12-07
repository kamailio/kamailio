/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
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
 * History:
 * --------
 * 2003-04-04  grand acc cleanup (jiri)
 * 2003-11-04  multidomain support for mysql introduced (jiri)
 * 2004-06-06  cleanup: acc_db_{bind,init,close} added (andrei)
 * 2006-09-08  flexible multi leg accounting support added,
 *             code cleanup for low level functions (bogdan)
 * 2006-09-19  final stage of a masive re-structuring and cleanup (bogdan)
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Core accounting
 *
 * - See \ref acc.c
 * - Module: \ref acc
 */

#ifndef _ACC_ACC_H_
#define _ACC_ACC_H_

/* leading text for a request accounted from a script */
#define ACC "ACC: "
#define ACC_REQUEST ACC"request accounted: "
#define ACC_REQUEST_LEN (sizeof(ACC_REQUEST)-1)
#define ACC_MISSED ACC"call missed: "
#define ACC_MISSED_LEN (sizeof(ACC_MISSED)-1)
#define ACC_ANSWERED ACC"transaction answered: "
#define ACC_ANSWERED_LEN (sizeof(ACC_ANSWERED)-1)
#define ACC_ACKED ACC"request acknowledged: "
#define ACC_ACKED_LEN (sizeof(ACC_ACKED)-1)

/* syslog attribute names */
#define A_METHOD "method"
#define A_METHOD_LEN (sizeof(A_METHOD)-1)
#define A_FROMTAG "from_tag"
#define A_FROMTAG_LEN (sizeof(A_FROMTAG)-1)
#define A_TOTAG "to_tag"
#define A_TOTAG_LEN (sizeof(A_TOTAG)-1)
#define A_CALLID "call_id"
#define A_CALLID_LEN (sizeof(A_CALLID)-1)
#define A_CODE "code"
#define A_CODE_LEN (sizeof(A_CODE)-1)
#define A_STATUS "reason"
#define A_STATUS_LEN (sizeof(A_STATUS)-1)

#define A_SEPARATOR_CHR ';'
#define A_SEPARATOR_CHR_2 ' '
#define A_EQ_CHR '='

#define MAX_SYSLOG_SIZE  65536

#define MAX_FAILED_FILTER_COUNT 15

/* WARNING: This is a flag stored in the sip_msg structure, the flag is
 * temporarily defined here to make the module work with the sip-router core,
 * I assume it won't be needed once we merge acc implementations from both
 * projects. The value of the flag must be kept synchronized with other flags
 * defined in parser/msg_parser.h!
 */
#define FL_REQ_UPSTREAM (1<<29)

void acc_log_init(void);
int  acc_log_request( struct sip_msg *req);

int core2strar(struct sip_msg *req, str *c_vals, int *i_vals, char *t_vals);

#ifdef SQL_ACC
int  acc_db_init(const str* db_url);
int  acc_db_init_child(const str* db_url);
void acc_db_close(void);
int  acc_db_request( struct sip_msg *req);
int acc_get_db_handlers(void **vf, void **vh);
#endif

#ifdef RAD_ACC
int  init_acc_rad(char *rad_cfg, int srv_type);
int  acc_rad_request( struct sip_msg *req );
#endif

#ifdef DIAM_ACC
int  acc_diam_init(void);
int  acc_diam_request( struct sip_msg *req );
#endif

#endif
