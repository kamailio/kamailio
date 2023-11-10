/**
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-journal.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "journal_send.h"

MODULE_VERSION

static int _lc_log_systemd = 0;
void _lc_core_log_systemd(int lpriority, const char *format, ...);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_sd_journal_print(struct sip_msg *msg, char *lev, char *txt);
static int w_sd_journal_send_xavp(struct sip_msg *msg, char *xname, char *);


static cmd_export_t cmds[] = {
		{"sd_journal_print", (cmd_function)w_sd_journal_print, 2,
				fixup_spve_spve, 0, ANY_ROUTE},
		{"sd_journal_send_xavp", (cmd_function)w_sd_journal_send_xavp, 1,
				fixup_spve_spve, 0, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {{0, 0, 0}};

struct module_exports exports = {
		"log_systemd",	 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* cmd (cfg function) exports */
		params,			 /* param exports */
		0,				 /* RPC method exports */
		0,				 /* pseudo-variables exports */
		0,				 /* response handling function */
		mod_init,		 /* module init function */
		child_init,		 /* per-child init function */
		mod_destroy		 /* module destroy function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 *
 */
static int ki_sd_journal_print(sip_msg_t *msg, str *slev, str *stxt)
{
	int ilev;

	/* one of LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR, LOG_WARNING,
	 * LOG_NOTICE, LOG_INFO, LOG_DEBUG, as defined in syslog.h, see syslog(3) */
	ilev = LOG_DEBUG;
	if(slev->len == 9 && strncasecmp(slev->s, "LOG_EMERG", slev->len) == 0) {
		ilev = LOG_EMERG;
	} else if(slev->len == 9
			  && strncasecmp(slev->s, "LOG_ALERT", slev->len) == 0) {
		ilev = LOG_ALERT;
	} else if(slev->len == 8
			  && strncasecmp(slev->s, "LOG_CRIT", slev->len) == 0) {
		ilev = LOG_CRIT;
	} else if(slev->len == 7
			  && strncasecmp(slev->s, "LOG_ERR", slev->len) == 0) {
		ilev = LOG_ERR;
	} else if(slev->len == 11
			  && strncasecmp(slev->s, "LOG_WARNING", slev->len) == 0) {
		ilev = LOG_WARNING;
	} else if(slev->len == 10
			  && strncasecmp(slev->s, "LOG_NOTICE", slev->len) == 0) {
		ilev = LOG_NOTICE;
	} else if(slev->len == 8
			  && strncasecmp(slev->s, "LOG_INFO", slev->len) == 0) {
		ilev = LOG_INFO;
	}

	sd_journal_print(ilev, "%.*s", stxt->len, stxt->s);

	return 1;
}

static int w_sd_journal_print(struct sip_msg *msg, char *lev, char *txt)
{
	str slev;
	str stxt;

	if(fixup_get_svalue(msg, (gparam_t *)lev, &slev) != 0) {
		LM_ERR("unable to get level parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)txt, &stxt) != 0) {
		LM_ERR("unable to get text parameter\n");
		return -1;
	}

	return ki_sd_journal_print(msg, &slev, &stxt);
}

#define LC_LOG_MSG_MAX_SIZE 16384
void _lc_core_log_systemd(int lpriority, const char *format, ...)
{
	va_list arglist;
	char obuf[LC_LOG_MSG_MAX_SIZE];
	int n;
	int priority;

	/* Return on MASKed log priorities */
	priority = LOG_PRI(lpriority);

	va_start(arglist, format);
	n = 0;
	n += vsnprintf(obuf + n, LC_LOG_MSG_MAX_SIZE - n, format, arglist);
	va_end(arglist);
	sd_journal_print(priority, "%.*s", n, obuf);
}

static int w_sd_journal_send_xavp(struct sip_msg *msg, char *xname, char *foo)
{
	str sxname;

	if(fixup_get_svalue(msg, (gparam_t *)xname, &sxname) != 0) {
		LM_ERR("unable to get xname parameter\n");
		return -1;
	}

	return k_sd_journal_send_xavp(&sxname);
}

static int ki_sd_journal_send_xavp(sip_msg_t *msg, str *xname)
{
	return k_sd_journal_send_xavp(xname);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_log_systemd_exports[] = {
	{ str_init("log_systemd"), str_init("sd_journal_print"),
		SR_KEMIP_INT, ki_sd_journal_print,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("log_systemd"), str_init("sd_journal_send_xvap"),
		SR_KEMIP_INT, ki_sd_journal_send_xavp,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(_km_log_engine_type == 0)
		return 0;

	if(strcasecmp(_km_log_engine_type, "systemd") != 0)
		return 0;

	km_log_func_set(&_lc_core_log_systemd);
	_lc_log_systemd = 1;
	sr_kemi_modules_add(sr_kemi_log_systemd_exports);
	return 0;
}