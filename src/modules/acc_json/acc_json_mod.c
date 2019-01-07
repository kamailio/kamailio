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

#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_to.h"
#include "../../core/kemi.h"
#include "../../modules/acc/acc_api.h"
#include "acc_json_mod.h"
#include "../../modules/acc/acc_extra.h"
#include "../../modules/mqueue/api.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);
static int child_init(int rank);

int acc_json_init(acc_init_info_t *inf);
int acc_json_send_request(struct sip_msg *req, acc_info_t *inf);

// acc API
acc_api_t accb;
acc_engine_t _acc_json_engine;
// mqueue API
mq_api_t mq_api;

int acc_flag = -1;
int acc_missed_flag = -1;
int acc_time_mode = 0;
static char *acc_extra_str = 0;
acc_extra_t *acc_extra = 0;
int output_syslog = -1;
char *output_mqueue_str = 0; /* see mqueue module queue name */
str q_name = {0, 0};
static char *log_facility_str = 0;

static cmd_export_t cmds[] = {{0, 0, 0, 0, 0, 0}};


static param_export_t params[] = {{"acc_flag", INT_PARAM, &acc_flag},
		{"acc_missed_flag", INT_PARAM, &acc_missed_flag},
		{"acc_extra", PARAM_STRING, &acc_extra_str},
		{"acc_time_mode", INT_PARAM, &acc_time_mode},
		{"acc_time_format", PARAM_STRING, &acc_time_format},
		{"log_level", INT_PARAM, &log_level},
		{"log_facility", PARAM_STRING, &log_facility_str},
		{"output_mqueue", PARAM_STRING, &output_mqueue_str},
		{"output_syslog", INT_PARAM, &output_syslog}, {0, 0, 0}};


struct module_exports exports = {
		"acc_json", 
                DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,		 /* exported functions */
		params,		 /* exported params */
		0,		 /* exported RPC methods */
		0,		 /* exported pseudo-variables */
		0,		 /* response function */
		mod_init,	 /* initialization module */
		child_init,	 /* per-child init function */
		destroy	 	 /* destroy function */
};


static int mod_init(void)
{
	/* bind the ACC API */
	if(acc_load_api(&accb) < 0) {
		LM_ERR("cannot bind to ACC API\n");
		return -1;
	}

	LM_INFO("janson version : %s\n", JANSSON_VERSION);
#if JANSSON_VERSION_HEX >= 0x010300
/* Code specific to version 1.3 and above */
#endif

	if(log_facility_str) {
		int tmp = str2facility(log_facility_str);
		if(tmp != -1)
			log_facility = tmp;
		else {
			LM_ERR("invalid log facility configured");
			return -1;
		}
	}

	/* load the MQUEUE API */
	if(output_mqueue_str && (load_mq_api(&mq_api) != 0)) {
		LM_ERR("can't load mqueue module API, disabling json acc to mqueue\n");
		output_mqueue_str = NULL;
	}
	if(output_mqueue_str) {
		q_name.s = output_mqueue_str;
		q_name.len = strlen(output_mqueue_str);
	}

	/* parse the extra string, if any */
	if(acc_extra_str && (acc_extra = accb.parse_extra(acc_extra_str)) == 0) {
		LM_ERR("failed to parse acc_extra param\n");
		return -1;
	}

	memset(&_acc_json_engine, 0, sizeof(acc_engine_t));

	if(acc_flag != -1)
		_acc_json_engine.acc_flag = acc_flag;
	if(acc_missed_flag != -1)
		_acc_json_engine.missed_flag = acc_missed_flag;
	_acc_json_engine.acc_req = acc_json_send_request;
	_acc_json_engine.acc_init = acc_json_init;
	memcpy(_acc_json_engine.name, "json", 4);
	if(accb.register_engine(&_acc_json_engine) < 0) {
		LM_ERR("cannot register ACC JSON engine\n");
		return -1;
	}

	return 0;
}


static int child_init(int rank)
{
	if(rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	return 0;
}


static void destroy(void)
{
}


int acc_json_init(acc_init_info_t *inf)
{
	LM_DBG(" init ...\n");
	return 0;
}


void syslog_write(const char *acc)
{
	//setlogmask(LOG_UPTO (LOG_NOTICE));
	openlog("json_acc", LOG_CONS | LOG_PID | LOG_NDELAY, log_facility);
	syslog(log_level, "%s", acc);
	closelog();
}


int acc_json_send_request(struct sip_msg *req, acc_info_t *inf)
{
	int attr_cnt;
	int i;
	int m = 0;
	int o = 0;
	/* core fields */
	attr_cnt = accb.get_core_attrs(req, inf->varr, inf->iarr, inf->tarr);

	json_t *object = json_object();
	if(acc_time_mode == 2) {
		double dtime = (double)inf->env->tv.tv_usec;
		dtime = (dtime / 1000000) + (double)inf->env->tv.tv_sec;
		json_object_set_new(object, "time", json_real(dtime));
	} else if(acc_time_mode == 3 || acc_time_mode == 4) {
		struct tm *t;
		if(acc_time_mode == 3) {
			t = localtime(&inf->env->ts);
		} else {
			t = gmtime(&inf->env->ts);
		}
		if(strftime(acc_time_format_buf, ACC_TIME_FORMAT_SIZE, acc_time_format,
				   t)
				<= 0) {
			acc_time_format_buf[0] = '\0';
		}
		json_object_set_new(object, "time", json_string(acc_time_format_buf));
	} else { // default acc_time_mode==1
		json_object_set_new(object, "time", json_integer(inf->env->ts));
	}

	LM_DBG("text[%.*s]\n", inf->env->text.len, inf->env->text.s);
	for(i = 0; i < attr_cnt; i++) {
		LM_DBG("[%d][%.*s]\n", i, inf->varr[i].len, inf->varr[i].s);
		char *tmp = strndup(inf->varr[i].s, inf->varr[i].len);
		json_t *value = json_string(tmp);
		if(!value)
			value = json_string("NON-UTF8");
		if(i == 0) {
			json_object_set_new(object, acc_method_key.s, value);
		} else if(i == 1) {
			json_object_set_new(object, acc_fromtag_key.s, value);
		} else if(i == 2) {
			json_object_set_new(object, acc_totag_key.s, value);
		} else if(i == 3) {
			json_object_set_new(object, acc_callid_key.s, value);
		} else if(i == 4) {
			json_object_set_new(object, acc_sipcode_key.s, value);
		} else if(i == 5) {
			json_object_set_new(object, acc_sipreason_key.s, value);
		}
		free(tmp);
	}

	/* add extra fields */
	o = accb.get_extra_attrs(acc_extra, req, inf->varr + attr_cnt,
			inf->iarr + attr_cnt, inf->tarr + attr_cnt);
	attr_cnt += o;
	m = attr_cnt;

	struct acc_extra *extra = acc_extra;
	for(; i < m; i++) {
		LM_DBG("[%d][%s][%.*s]\n", i, extra->name.s, inf->varr[i].len,
				inf->varr[i].s);
		char *tmp = strndup(inf->varr[i].s, inf->varr[i].len);
		json_t *value = json_string(tmp);
		if(!value)
			value = json_string("NON-UTF8");
		json_object_set_new(object, extra->name.s, value);
		free(tmp);
		extra = extra->next;
	}

	/* add leginfo fields */
	if(inf->leg_info) {
		o = accb.get_leg_attrs(inf->leg_info, req, inf->varr + attr_cnt,
			inf->iarr + attr_cnt, inf->tarr + attr_cnt, 1);
		attr_cnt += o;
		m = attr_cnt;

		struct acc_extra *leg_info = inf->leg_info;
		for(; i < m; i++) {
			LM_DBG("[%d][%s][%.*s]\n", i, leg_info->name.s, inf->varr[i].len,
					inf->varr[i].s);
			char *tmp = strndup(inf->varr[i].s, inf->varr[i].len);
			json_t *value = json_string(tmp);
			if(!value)
				value = json_string("NON-UTF8");
			json_object_set_new(object, leg_info->name.s, value);
			free(tmp);
			leg_info = leg_info->next;
		}
	}

	if(object) {
		if(json_object_size(object) == 0) {
			LM_ERR("json object empty\n");
			json_decref(object);
			return 0;
		}
		char *json_string = json_dumps(object, JSON_ENSURE_ASCII);
		str acc_str = {json_string, strlen(json_string)};

		// json acc output to mqueue
		if(output_mqueue_str) {
			str key = str_init("acc");
			if(mq_api.add(&q_name, &key, &acc_str)) {
				LM_DBG("ACC queued [%d][%s]\n", acc_str.len, acc_str.s);
			} else {
				LM_DBG("ACC mqueue add error [%d][%s]\n", acc_str.len,
						acc_str.s);
			}
		}
		// json acc output to syslog
		if(output_syslog)
			syslog_write(json_string);
		free(json_string);
		json_object_clear(object);
		json_decref(object);
	}
	/* free memory allocated by extra2strar */
	free_strar_mem(&(inf->tarr[m - o]), &(inf->varr[m - o]), o, m);
	return 1;
}
