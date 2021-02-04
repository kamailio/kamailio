/*
 * debug print
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */
/*!
 * \file
 * \brief Kamailio core :: Debug print
 * \ingroup core
 * Module: \ref core
 */


#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

#include "globals.h"
#include "dprint.h"
#include "pvar.h"

static void log_callid_set(sip_msg_t *msg);

char *_km_log_engine_type = NULL;
char *_km_log_engine_data = NULL;

km_log_f _km_log_func = &syslog;

/**
 *
 */
void km_log_func_set(km_log_f f)
{
	_km_log_func = f;
}

#ifndef NO_SIG_DEBUG
/* signal protection: !=0 when LOG/DBG/... are printing */
volatile int dprint_crit = 0;
#endif

static char* str_fac[]={"LOG_AUTH","LOG_CRON","LOG_DAEMON",
			"LOG_KERN","LOG_LOCAL0","LOG_LOCAL1",
			"LOG_LOCAL2","LOG_LOCAL3","LOG_LOCAL4","LOG_LOCAL5",
			"LOG_LOCAL6","LOG_LOCAL7","LOG_LPR","LOG_MAIL",
			"LOG_NEWS","LOG_USER","LOG_UUCP",
#ifndef __OS_solaris
			"LOG_AUTHPRIV","LOG_FTP","LOG_SYSLOG",
#endif
			0};

static int int_fac[]={LOG_AUTH ,  LOG_CRON , LOG_DAEMON ,
		      LOG_KERN , LOG_LOCAL0 , LOG_LOCAL1 ,
		      LOG_LOCAL2 , LOG_LOCAL3 , LOG_LOCAL4 , LOG_LOCAL5 ,
		      LOG_LOCAL6 , LOG_LOCAL7 , LOG_LPR , LOG_MAIL ,
		      LOG_NEWS , LOG_USER , LOG_UUCP,
#ifndef __OS_solaris
		      LOG_AUTHPRIV,LOG_FTP,LOG_SYSLOG,
#endif
		      0};

struct log_level_info log_level_info[] = {
	{"ALERT", LOG_ALERT},	  /* L_ALERT */
	{"BUG", LOG_CRIT},        /* L_BUG */
	{"CRITICAL", LOG_CRIT},   /* L_CRIT2 */
	{"",    LOG_CRIT},         /* L_CRIT */
	{"ERROR", LOG_ERR},       /* L_ERR */
	{"WARNING", LOG_WARNING}, /* L_WARN */
	{"NOTICE", LOG_NOTICE},   /* L_NOTICE */
	{"INFO", LOG_INFO},       /* L_INFO */
	{"DEBUG", LOG_DEBUG}	  /* L_DBG */
};

int str2facility(char *s)
{
	int i;

	for (i=0; str_fac[i]; i++) {
		if (!strcasecmp(s,str_fac[i]))
			return int_fac[i];
	}
	return -1;
}

char* facility2str(int fl, int *len)
{
	int i;

	for (i=0; str_fac[i]; i++) {
		if (fl == int_fac[i]) {
			*len = strlen(str_fac[i]);
			return str_fac[i];
		}
	}

	return NULL;
}

/* fixup function for log_facility cfg parameter */
int log_facility_fixup(void *handle, str *gname, str *name, void **val)
{
	int	i;

	if ((i = str2facility((char *)*val)) == -1) {
		LM_ERR("invalid log facility: %s\n", (char *)*val);
		return -1;
	}
	*val = (void *)(long)i;
	return 0;
}


/**
 * per process debug log level (local)
 */

/* value for unset local log level  */
#define UNSET_LOCAL_DEBUG_LEVEL	    -255
#define UNSET_LOCAL_DEBUG_FACILITY  -255

/* the local debug log level */
static int _local_debug_level = UNSET_LOCAL_DEBUG_LEVEL;
static int _local_debug_facility = UNSET_LOCAL_DEBUG_FACILITY;
/* callback to get per module debug level */
static get_module_debug_level_f _module_debug_level = NULL;
static get_module_debug_facility_f _module_debug_facility = NULL;

/**
 * @brief set callback function for per module debug level
 */
void set_module_debug_level_cb(get_module_debug_level_f f)
{
	_module_debug_level = f;
}

void set_module_debug_facility_cb(get_module_debug_facility_f f)
{
	_module_debug_facility = f;
}

/**
 * @brief return the log level - the local one if it set,
 *   otherwise the global value
 */
int get_debug_level(char *mname, int mnlen) {
	int mlevel;
	/*important -- no LOGs inside, because it will loop */
	if(unlikely(_module_debug_level!=NULL && mnlen>0)) {
		if(_module_debug_level(mname, mnlen, &mlevel)==0) {
			return mlevel;
		}
	}
	return (_local_debug_level != UNSET_LOCAL_DEBUG_LEVEL) ?
				_local_debug_level : cfg_get(core, core_cfg, debug);
}

/**
 * @brief return the log level - the local one if it set,
 *   otherwise the global value
 */
int get_cfg_debug_level(void) {
	/*important -- no LOGs inside, because it will loop */
	return (_local_debug_level != UNSET_LOCAL_DEBUG_LEVEL) ?
				_local_debug_level : cfg_get(core, core_cfg, debug);
}

/**
 * @brief return the log facility - the local one if it set,
 *   otherwise the global value
 */
int get_debug_facility(char *mname, int mnlen) {
	int mfacility;
	/*important -- no LOGs inside, because it will loop */
	if(unlikely(_module_debug_facility!=NULL && mnlen>0)) {
		if(_module_debug_facility(mname, mnlen, &mfacility)==0) {
			return mfacility;
		}
	}
	return (_local_debug_facility != UNSET_LOCAL_DEBUG_FACILITY) ?
				_local_debug_facility : cfg_get(core, core_cfg, log_facility);
}


/**
 * @brief set the local debug log level
 */
void set_local_debug_level(int level)
{
	_local_debug_level = level;
}

/**
 * @brief reset the local debug log level
 */
void reset_local_debug_level(void)
{
	_local_debug_level = UNSET_LOCAL_DEBUG_LEVEL;
}

/**
 * @brief set the local debug log facility
 */
void set_local_debug_facility(int facility)
{
	_local_debug_facility = facility;
}

/**
 * @brief reset the local debug log facility
 */
void reset_local_debug_facility(void)
{
	_local_debug_facility = UNSET_LOCAL_DEBUG_FACILITY;
}

typedef struct log_level_color {
	char f;
	char b;
} log_level_color_t;

log_level_color_t _log_level_colors[L_MAX - L_MIN + 1];

void dprint_init_colors(void)
{
	int i;

	i = 0;

	memset(_log_level_colors, 0,
			(L_MAX - L_MIN + 1)*sizeof(log_level_color_t));

	/* L_ALERT */
	_log_level_colors[i].f = 'R'; /* default */
	_log_level_colors[i].b = 'x'; /* default */
	i++;

	/* L_BUG */
	_log_level_colors[i].f = 'P'; /* default */
	_log_level_colors[i].b = 'x'; /* default */
	i++;

	/* L_CRIT2 */
	_log_level_colors[i].f = 'y'; /* default */
	_log_level_colors[i].b = 'x'; /* default */
	i++;

	/* L_CRIT */
	_log_level_colors[i].f = 'b'; /* default */
	_log_level_colors[i].b = 'x'; /* default */
	i++;

	/* L_ERR */
	_log_level_colors[i].f = 'r'; /* default */
	_log_level_colors[i].b = 'x'; /* default */
	i++;

	/* L_WARN */
	_log_level_colors[i].f = 'p'; /* default */
	_log_level_colors[i].b = 'x'; /* default */
	i++;

	/* L_NOTICE */
	_log_level_colors[i].f = 'g'; /* default */
	_log_level_colors[i].b = 'x'; /* default */
	i++;

	/* L_INFO */
	_log_level_colors[i].f = 'c'; /* default */
	_log_level_colors[i].b = 'x'; /* default */
	i++;

	/* L_DBG */
	_log_level_colors[i].f = 'x'; /* default */
	_log_level_colors[i].b = 'x'; /* default */
	i++;
}

#define TERM_COLOR_SIZE 16

#define dprint_termc_add(p, end, s) \
        do{ \
                if ((p)+(sizeof(s)-1)<=(end)){ \
                        memcpy((p), s, sizeof(s)-1); \
                        (p)+=sizeof(s)-1; \
                }else{ \
                        /* overflow */ \
                        LM_ERR("dprint_termc_add overflow\n"); \
                        goto error; \
                } \
        } while(0)


void dprint_term_color(char f, char b, str *obuf)
{
	static char term_color[TERM_COLOR_SIZE];
	char* p;
	char* end;

	p = term_color;
	end = p + TERM_COLOR_SIZE;

	/* excape sequence */
	dprint_termc_add(p, end, "\033[");

	if(f!='_')
	{
		if (islower((int)f))
		{
			/* normal font */
			dprint_termc_add(p, end, "0;");
		} else {
			/* bold font */
			dprint_termc_add(p, end, "1;");
			f += 32;
		}
	}

	/* foreground */
	switch(f)
	{
		case 'x':
			dprint_termc_add(p, end, "39;");
		break;
		case 's':
			dprint_termc_add(p, end, "30;");
		break;
		case 'r':
			dprint_termc_add(p, end, "31;");
		break;
		case 'g':
			dprint_termc_add(p, end, "32;");
		break;
		case 'y':
			dprint_termc_add(p, end, "33;");
		break;
		case 'b':
			dprint_termc_add(p, end, "34;");
		break;
		case 'p':
			dprint_termc_add(p, end, "35;");
		break;
		case 'c':
			dprint_termc_add(p, end, "36;");
		break;
		case 'w':
			dprint_termc_add(p, end, "37;");
		break;
		default:
			dprint_termc_add(p, end, "39;");
	}

	/* background */
	switch(b)
	{
		case 'x':
			dprint_termc_add(p, end, "49");
		break;
		case 's':
			dprint_termc_add(p, end, "40");
		break;
		case 'r':
			dprint_termc_add(p, end, "41");
		break;
		case 'g':
			dprint_termc_add(p, end, "42");
		break;
		case 'y':
			dprint_termc_add(p, end, "43");
		break;
		case 'b':
			dprint_termc_add(p, end, "44");
		break;
		case 'p':
			dprint_termc_add(p, end, "45");
		break;
		case 'c':
			dprint_termc_add(p, end, "46");
		break;
		case 'w':
			dprint_termc_add(p, end, "47");
		break;
		default:
			dprint_termc_add(p, end, "49");
	}

	/* end */
	dprint_termc_add(p, end, "m");

	obuf->s = term_color;
	obuf->len = p - term_color;
	return;

error:
	obuf->s = term_color;
	term_color[0] = '\0';
	obuf->len = 0;
}

void dprint_color(int level)
{
	str obuf;

	if(level<L_MIN || level>L_MAX)
		return;
	dprint_term_color(_log_level_colors[level - L_MIN].f,
			_log_level_colors[level - L_MIN].b,
			&obuf);
	fprintf(stderr, "%.*s", obuf.len, obuf.s);
}

void dprint_color_reset(void)
{
	str obuf;

	dprint_term_color('x', 'x', &obuf);
	fprintf(stderr, "%.*s", obuf.len, obuf.s);
}

void dprint_color_update(int level, char f, char b)
{
	if(level<L_MIN || level>L_MAX)
		return;
	if(f && f!='0') _log_level_colors[level - L_MIN].f = f;
	if(b && b!='0') _log_level_colors[level - L_MIN].b = b;
}


/* log_prefix functionality */
str *log_prefix_val = NULL;
int log_prefix_mode = 0;
static pv_elem_t *log_prefix_pvs = NULL;

#define LOG_PREFIX_SIZE	1024
static char log_prefix_buf[LOG_PREFIX_SIZE];
static str log_prefix_str = STR_NULL;

void log_prefix_init(void)
{
	str s;
	if(log_prefix_fmt==NULL)
		return;
	s.s = log_prefix_fmt; s.len = strlen(s.s);

	if(pv_parse_format(&s, &log_prefix_pvs)<0)
	{
		LM_ERR("wrong format[%s]\n", s.s);
		return;
	}
}

void log_prefix_set(sip_msg_t *msg)
{
	log_callid_set(msg);
	if(log_prefix_pvs == NULL)
		return;
	if(msg==NULL || !(IS_SIP(msg) || IS_SIP_REPLY(msg))) {
		log_prefix_val = NULL;
		return;
	}
	log_prefix_str.s = log_prefix_buf;
	log_prefix_str.len = LOG_PREFIX_SIZE;
	if(pv_printf(msg, log_prefix_pvs, log_prefix_str.s, &log_prefix_str.len)<0)
		return;
	if(log_prefix_str.len<=0)
		return;
	log_prefix_val = &log_prefix_str;
}

/* structured logging */

ksr_slog_f _ksr_slog_func = NULL;

#define LOG_CALLID_SIZE	256
static char log_callid_buf[LOG_CALLID_SIZE];
static str log_callid_str = STR_NULL;

static int _ksr_slog_json_flags = 0;
#define KSR_SLOGJSON_FL_STRIPMSGNL (1<<0)
#define KSR_SLOGJSON_FL_NOLOGNL (1<<1)
#define KSR_SLOGJSON_FL_APPPREFIX (1<<2)
#define KSR_SLOGJSON_FL_NOAPPPREFIXMSG (1<<3)
#define KSR_SLOGJSON_FL_CALLID (1<<4)


#define LOGV_CALLID_STR (((_ksr_slog_json_flags & KSR_SLOGJSON_FL_CALLID) \
			&& (log_callid_str.len>0))?log_callid_str.s:"")
#define LOGV_CALLID_LEN (((_ksr_slog_json_flags & KSR_SLOGJSON_FL_CALLID) \
			&& (log_callid_str.len>0))?log_callid_str.len:0)

static void ksr_slog_json_str_escape(str *s_in, str *s_out, int *emode)
{
	char *p1, *p2;
	int len = 0;
	char token;
	int i;

	s_out->len = 0;
	if (!s_in || !s_in->s) {
		s_out->s = strdup("");
		*emode = 1;
		return;
	}
	for(i = 0; i < s_in->len; i++) {
		if (strchr("\"\\\b\f\n\r\t", s_in->s[i])) {
			len += 2;
		} else if (s_in->s[i] < 32) {
			len += 6;
		} else {
			len++;
		}
	}
	if(len == s_in->len) {
		s_out->s = s_in->s;
		s_out->len = s_in->len;
		*emode = 0;
		return;
	}

	s_out->s = (char*)malloc(len + 2);
	if (!s_out->s) {
		return;
	}
	*emode = 1;

	p2 = s_out->s;
	p1 = s_in->s;
	while (p1 < s_in->s + s_in->len) {
		if ((unsigned char) *p1 > 31 && *p1 != '\"' && *p1 != '\\') {
			*p2++ = *p1++;
		} else {
			*p2++ = '\\';
			switch (token = *p1++) {
			case '\\':
				*p2++ = '\\';
				break;
			case '\"':
				*p2++ = '\"';
				break;
			case '\b':
				*p2++ = 'b';
				break;
			case '\f':
				*p2++ = 'f';
				break;
			case '\n':
				*p2++ = 'n';
				break;
			case '\r':
				*p2++ = 'r';
				break;
			case '\t':
				*p2++ = 't';
				break;
			default:
				/* escape and print */
				snprintf(p2, 6, "u%04x", token);
				p2 += 5;
				break;
			}
		}
	}
	*p2++ = 0;
	s_out->len = len;
	return;
}

#define KSR_SLOG_SYSLOG_JSON_FMT "{ \"level\": \"%s\", \"module\": \"%s\", \"file\": \"%s\"," \
	" \"line\": %d, \"function\": \"%s\", %.*s\"logprefix\": \"%.*s\", \"%smessage\": \"%.*s\" }%s"

#define KSR_SLOG_SYSLOG_JSON_CFMT "{ \"level\": \"%s\", \"module\": \"%s\", \"file\": \"%s\"," \
	" \"line\": %d, \"function\": \"%s\", \"callid\": \"%.*s\", \"logprefix\": \"%.*s\", \"%smessage\": \"%.*s\" }%s"

#define KSR_SLOG_SYSLOG_JSON_PFMT "{ \"" NAME ".level\": \"%s\", \"" NAME ".module\": \"%s\", \"" NAME ".file\": \"%s\"," \
	" \"" NAME ".line\": %d, \"" NAME ".function\": \"%s\", %.*s\"" NAME ".logprefix\": \"%.*s\", \"%smessage\": \"%.*s\" }%s"

#define KSR_SLOG_SYSLOG_JSON_CPFMT "{ \"" NAME ".level\": \"%s\", \"" NAME ".module\": \"%s\", \"" NAME ".file\": \"%s\"," \
	" \"" NAME ".line\": %d, \"" NAME ".function\": \"%s\", \"" NAME ".callid\": \"%.*s\", \"" NAME ".logprefix\": \"%.*s\"," \
	" \"%smessage\": \"%.*s\" }%s"

#define KSR_SLOG_STDERR_JSON_FMT "{ \"idx\": %d, \"pid\": %d, \"level\": \"%s\"," \
	" \"module\": \"%s\", \"file\": \"%s\"," \
	" \"line\": %d, \"function\": \"%s\", %.*s\"logprefix\": \"%.*s\", \"%smessage\": \"%.*s\" }%s"

#define KSR_SLOG_STDERR_JSON_CFMT "{ \"idx\": %d, \"pid\": %d, \"level\": \"%s\"," \
	" \"module\": \"%s\", \"file\": \"%s\"," \
	" \"line\": %d, \"function\": \"%s\", \"callid\": \"%.*s\", \"logprefix\": \"%.*s\", \"%smessage\": \"%.*s\" }%s"

#define KSR_SLOG_STDERR_JSON_PFMT "{ \"" NAME ".idx\": %d, \"" NAME ".pid\": %d, \"" NAME ".level\": \"%s\"," \
	" \"" NAME ".module\": \"%s\", \"" NAME ".file\": \"%s\"," \
	" \"" NAME ".line\": %d, \"" NAME ".function\": \"%s\", %.*s\"" NAME ".logprefix\": \"%.*s\", \"%smessage\": \"%.*s\" }%s"

#define KSR_SLOG_STDERR_JSON_CPFMT "{ \"" NAME ".idx\": %d, \"" NAME ".pid\": %d, \"" NAME ".level\": \"%s\"," \
	" \"" NAME ".module\": \"%s\", \"" NAME ".file\": \"%s\"," \
	" \"" NAME ".line\": %d, \"" NAME ".function\": \"%s\", \"" NAME ".callid\": \"%.*s\", \"" NAME ".logprefix\": \"%.*s\"," \
	" \"%smessage\": \"%.*s\" }%s"

void ksr_slog_json(ksr_logdata_t *kld, const char *format, ...)
{
	va_list arglist;
#define KSR_SLOG_MAX_SIZE 32*1024
	char obuf[KSR_SLOG_MAX_SIZE];
	int n;
	str s_in = STR_NULL;
	str s_out = STR_NULL;
	int emode = 0;
	char *prefmsg;
	const char *efmt;
	const char *sfmt;

	va_start(arglist, format);
	n = vsnprintf(obuf + s_in.len, KSR_SLOG_MAX_SIZE - s_in.len, format, arglist);
	if(n<0 || n>=KSR_SLOG_MAX_SIZE - s_in.len) {
		va_end(arglist);
		goto error;
	}
	s_in.len += n;
	va_end(arglist);

	s_in.s = obuf;
	if (_ksr_slog_json_flags & KSR_SLOGJSON_FL_STRIPMSGNL) {
		if(s_in.s[s_in.len - 1] == '\n') {
			s_in.len--;
		}
	}

	ksr_slog_json_str_escape(&s_in, &s_out, &emode);
	if(s_out.s == NULL) {
		goto error;
	}

	if(_ksr_slog_json_flags & KSR_SLOGJSON_FL_APPPREFIX) {
		if(_ksr_slog_json_flags & KSR_SLOGJSON_FL_NOAPPPREFIXMSG) {
			prefmsg = "";
		} else {
			prefmsg = NAME ".";
		}
		if(_ksr_slog_json_flags & KSR_SLOGJSON_FL_CALLID) {
			efmt = KSR_SLOG_STDERR_JSON_CPFMT;
			sfmt = KSR_SLOG_SYSLOG_JSON_CPFMT;
		} else {
			efmt = KSR_SLOG_STDERR_JSON_PFMT;
			sfmt = KSR_SLOG_SYSLOG_JSON_PFMT;
		}
	} else {
		prefmsg = "";
		if(_ksr_slog_json_flags & KSR_SLOGJSON_FL_CALLID) {
			efmt = KSR_SLOG_STDERR_JSON_CFMT;
			sfmt = KSR_SLOG_SYSLOG_JSON_CFMT;
		} else {
			efmt = KSR_SLOG_STDERR_JSON_FMT;
			sfmt = KSR_SLOG_SYSLOG_JSON_FMT;
		}
	}
	if (unlikely(log_stderr)) {
		if (unlikely(log_color)) dprint_color(kld->v_level);
		fprintf(stderr,
				efmt, process_no, my_pid(),
				kld->v_lname, kld->v_mname, kld->v_fname, kld->v_fline,
				kld->v_func, LOGV_CALLID_LEN, LOGV_CALLID_STR,
				LOGV_PREFIX_LEN, LOGV_PREFIX_STR, prefmsg, s_out.len, s_out.s,
				(_ksr_slog_json_flags & KSR_SLOGJSON_FL_NOLOGNL)?"":"\n");
		if (unlikely(log_color)) dprint_color_reset();
	} else {
		_km_log_func(kld->v_facility,
				sfmt,
				kld->v_lname, kld->v_mname, kld->v_fname, kld->v_fline,
				kld->v_func, LOGV_CALLID_LEN, LOGV_CALLID_STR,
				LOGV_PREFIX_LEN, LOGV_PREFIX_STR, prefmsg, s_out.len, s_out.s,
				(_ksr_slog_json_flags & KSR_SLOGJSON_FL_NOLOGNL)?"":"\n");
	}
	if(emode && s_out.s) {
		free(s_out.s);
	}
	return;
error:
	return;
}

void ksr_slog_init(char *ename)
{
	char *p;
	int elen = 0;

	if (!ename) {
		return;
	}

	p = strchr(ename, ':');
	if(p) {
		elen = p - ename;
	} else {
		elen = strlen(ename);
	}

	if ((elen==4) && (strncasecmp(ename, "json", 4)==0)) {
		_km_log_engine_type = "json";
		_ksr_slog_func = &ksr_slog_json;
		if(p) {
			_km_log_engine_data = p + 1;
			while (*p) {
				switch (*p) {
					case 'M':
						_ksr_slog_json_flags |= KSR_SLOGJSON_FL_STRIPMSGNL;
					break;
					case 'N':
						_ksr_slog_json_flags |= KSR_SLOGJSON_FL_NOLOGNL;
					break;
					case 'a':
						_ksr_slog_json_flags |= KSR_SLOGJSON_FL_APPPREFIX;
					break;
					case 'A':
						_ksr_slog_json_flags |= KSR_SLOGJSON_FL_NOAPPPREFIXMSG;
					break;
					case 'c':
						_ksr_slog_json_flags |= KSR_SLOGJSON_FL_CALLID;
					break;
				}
				p++;
			}
		}
	}
}

static void log_callid_set(sip_msg_t *msg)
{
	if(!(_ksr_slog_json_flags & KSR_SLOGJSON_FL_CALLID)) {
		return;
	}
	if(msg==NULL) {
		log_callid_str.len = 0;
		log_callid_str.s = NULL;
		return;
	}
	if(msg->callid==NULL && ((parse_headers(msg, HDR_CALLID_F, 0)==-1)
			|| (msg->callid==NULL))) {
		log_callid_str.len = 0;
		log_callid_str.s = NULL;
		return;
	}
	if(msg->callid->body.len >= LOG_CALLID_SIZE) {
		log_callid_str.len = 0;
		log_callid_str.s = NULL;
		return;
	}
	log_callid_str.len = msg->callid->body.len;
	memcpy(log_callid_buf, msg->callid->body.s, msg->callid->body.len);
	log_callid_str.s = log_callid_buf;
	log_callid_str.s[log_callid_str.len] = '\0';
}
