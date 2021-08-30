/*
 * Copyright (C) 2021 Arsen Semenov arsperger@gmail.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the tertc of the GNU General Public License as published by
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
 */


#ifndef slack_h
#define slack_h

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/parser/parse_content.h"
#include "../../core/pvar.h"
#include "../../core/kemi.h"

#include <string.h>
#include <curl/curl.h>

#define BODY_FMT "{\"channel\": \"%s\", \"username\": \"%s\", \"text\": \"%s\", \"icon_emoji\": \"%s\" }"
#define SLACK_URL_MAX_SIZE 128
#define SLACK_DEFAULT_CHANNEL "#webtest"
#define SLACK_DEFAULT_USERNAME "webhookbot"
#define SLACK_DEFAULT_ICON ":ghost:"

static int _slack_print_log(struct sip_msg* msg, pv_elem_p list, char *buf, int *len)
{
	return pv_printf(msg, list, buf, len);
}

static int _curl_send(const char* uri, str *post_data );
static int _slack_parse_url_param(char *val);
static int _slack_url_param(modparam_t type, void *val);

static int slack_fixup(void** param, int param_no);
static int slack_send1(struct sip_msg* msg, char* frm, char* str2);
static int slack_fixup_helper(void** param, int param_no);

typedef struct _sl_msg
{
	pv_elem_t *m;
} sl_msg_t;

#endif /* slack_h */
