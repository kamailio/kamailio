/*
 * JSON Accounting module
 *
 * Copyright (C) 2018 Julien Chavanton (Flowroute.com)
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

#ifndef _ACC_JSON_MOD_H_
#define _ACC_JSON_MOD_H_

str acc_method_key = str_init("method");
str acc_fromtag_key = str_init("from_tag");
str acc_totag_key = str_init("to_tag");
str acc_callid_key = str_init("callid");
str acc_sipcode_key = str_init("sip_code");
str acc_sipreason_key = str_init("sip_reason");
str acc_time_key = str_init("time");

str cdr_start_str = str_init("start_time");
str cdr_end_str = str_init("end_time");
str cdr_duration_str = str_init("duration");

#define ACC_TIME_FORMAT_SIZE 128
static char acc_time_format_buf[ACC_TIME_FORMAT_SIZE];
char *acc_time_format = "%Y-%m-%d %H:%M:%S";

int acc_log_level = L_NOTICE;
int acc_log_facility = LOG_DAEMON;
int cdr_log_level = L_NOTICE;
int cdr_log_facility = LOG_DAEMON;

#endif
