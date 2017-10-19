/*
 * Accounting module
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
 */

/*! \file
 * \ingroup acc
 * \brief Acc::  Core module
 *
 * - \ref acc_mod.c
 * - Module: \ref acc
 */

#ifndef _ACC_MOD_H
#define _ACC_MOD_H

/* module parameter declaration */
extern int report_cancels;
extern int report_ack;
extern int early_media;
extern int failed_transaction_flag;
extern unsigned short failed_filter[];
extern int detect_direction;
extern int acc_prepare_flag;
extern int reason_from_hf;

extern int log_facility;
extern int log_level;
extern int log_flag;
extern int log_missed_flag;

extern int cdr_enable;
extern int cdr_start_on_confirmed;
extern int cdr_log_facility;
extern int cdr_expired_dlg_enable;

extern int db_flag;
extern int db_missed_flag;

extern str db_table_acc;
extern void *db_table_acc_data;
extern str db_table_mc;
extern void *db_table_mc_data;

extern str acc_method_col;
extern str acc_fromuri_col;
extern str acc_fromtag_col;
extern str acc_touri_col;
extern str acc_totag_col;
extern str acc_callid_col;
extern str acc_cseqno_col;
extern str acc_sipcode_col;
extern str acc_sipreason_col;
extern str acc_time_col;

extern int acc_db_insert_mode;

/* time mode */
extern int acc_time_mode;
extern str acc_time_attr;
extern str acc_time_exten;

extern int acc_prepare_always;

#endif
