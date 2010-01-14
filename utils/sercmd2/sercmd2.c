/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * send commands using binrpc
 *
 * History:
 * --------
 *  2006-02-14  created by andrei
 *  2007        ported to libbinrpc (bpintea)
 */


#include <stdlib.h> /* exit, abort */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h> /* isprint */
#include <sys/socket.h>
#include <sys/un.h> /* unix sock*/
#include <netinet/in.h> /* udp sock */
#include <sys/uio.h> /* writev */
#include <netdb.h> /* gethostbyname */
#include <time.h> /* time */
#include <stdarg.h>
#include <signal.h>

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "license.h"

#include "../../modules/binrpc/modbrpc_defaults.h" /* default socket */
#include <binrpc.h>

#ifdef WITH_LOCDGRAM
#define TMP_TEMPLATE	"/tmp/sercmd.usock.XXXXXX"
#endif

#define RX_TIMEOUT	60000000LU
#define TX_TIMEOUT	1000000LU
#define INDENT_STEP		2

#ifndef NAME
#define NAME    "sercmd2"
#endif
#ifndef VERSION
#define VERSION "0.2"
#endif

#define MAX_LINE_SIZE 1024 /* for non readline mode */


#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

static char id[]="$Id$";
static char version[]= NAME " " VERSION;
static char compiled[]= __TIME__ " " __DATE__;
static char help_msg[]="\
Usage: " NAME " [options] [cmd]\n\
\n\
Options:\n\
    -u URI      BINRPC URI where SER is listening.\n\
    -f format   print the result using a formater.\n\
    -i          quit after receiving SIGINT\n\
    -v          Verbose       \n\
    -V          Version number\n\
    -h          This help message\n\
\n\
Format:\n\
    A string containing `%v' markers that will be substituited with values \n\
    read from the reply.\n\
\n\
URI:\n\
    scheme://<name|path>[:port]   where scheme is brpc<n46l><sd>, with:\n\
                                  'n' standing for network address,\n\
                                  '4' standing for INET IPv4 address,\n\
                                  '6' standing for INET IPv6 address,\n\
                                  'l' for unix domain address,\n\
                                  's' for stream connection and\n\
                                  'd' for datagram communication.\n\
                                  E.g.: brpcns://localhost:2048\n\
                                        brpcld://tmp/ser.usock\n\
\n\
cmd:\n\
    method  [arg1 [arg2...]]\n\
arg:\n\
     string or number; to force a certain interpretation, quote the strings\n\
     and prefix numbers by a hash sign (hexa is expected in this case); \n\
     e.g. \"1234\" or #deadbeef\n\
\n\
Examples:\n\
        " NAME " -u brpcls:/tmp/ser_unix system.listMethods\n\
        " NAME " -f \"pid: %v  desc: %v\\n\" -u brpcnd://localhost core.ps \n\
        " NAME " ps  # uses default ctl socket \n\
        " NAME "     # enters interactive mode on the default socket \n\
        " NAME " -u brpcns://localhost # interactive mode, default port \n\
";


static int (*gen_cookie)() = rand;
int verbose=0;
struct sockaddr_un mysun;
int quit; /* used only in interactive mode */
int quit_on_sigint = 0; /* if true, quit on SIGINT */
#ifdef WITH_LOCDGRAM
char *tmppath = NULL;
#endif


brpc_str_t *commands = NULL;
size_t cmds_nr = 0;


enum TOK_TYPE {
	TOK_NONE,
	TOK_STR,
	TOK_INT,
	TOK_FLT,
};

typedef struct {
	enum TOK_TYPE type;
	union {
		int i;
		double d;
		struct {
			char *val;
			size_t len;
		} s;
	};
} tok_t;


#define INT2STR_MAX_LEN  (19+1+1) /* 2^64~= 16*10^18 => 19+1 digits + \0 */

/* returns a pointer to a static buffer containing l in asciiz & sets len */
static inline char* int2str(unsigned int l, int* len)
{
	static char r[INT2STR_MAX_LEN];
	int i;
	
	i=INT2STR_MAX_LEN-2;
	r[INT2STR_MAX_LEN-1]=0; /* null terminate */
	do{
		r[i]=l%10+'0';
		i--;
		l/=10;
	}while(l && (i>=0));
	if (l && (i<0)){
		fprintf(stderr, "BUG: int2str: overflow\n");
	}
	if (len) *len=(INT2STR_MAX_LEN-2)-i;
	return &r[i+1];
}



static char* trim_ws(char* l)
{
	char* ret;
	
	for(;*l && ((*l==' ')||(*l=='\t')||(*l=='\n')||(*l=='\r')); l++);
	ret=l;
	if (*ret==0) return ret;
	for(l=l+strlen(l)-1; (l>ret) && 
			((*l==' ')||(*l=='\t')||(*l=='\n')||(*l=='\r')); l--);
	*(l+1)=0;
	return ret;
}



struct cmd_alias{
	char* name;
	char* method;
	char* format; /* reply print format */
};


struct sercmd_builtin{
	char* name;
	int (*f)(int, brpc_t*);
	char* doc;
};


static int sercmd_help(int s, brpc_t *);
static int sercmd_ver(int s, brpc_t *);
static int sercmd_quit(int s, brpc_t *);
static int sercmd_warranty(int s, brpc_t *);

/* TODO: reme (core.)prints to (core.)echo */
static struct cmd_alias cmd_aliases[]={
	{	"ps",			"core.ps",				"%v\t%v\n"	},
	{	"list",			"system.listMethods",	0			},
	{	"ls",			"system.listMethods",	0			},
	{	"server",		"core.version",			0			},
	{	"serversion",	"core.version",			0			},
	{	"who",			"binrpc.who",				
			NULL},//"[%v] %v: %v %v -> %v %v\n"}, //TODO: fix command on module
	{	"listen",		"binrpc.listen",			"[%v] %v: %v %v\n"},
	{	"dns_mem_info",	"dns.mem_info",			"%v / %v\n"},
	{	"dns_debug",	"dns.debug",			
			"%v (%v): size=%v ref=%v expire=%vs last=%vs ago f=%v\n"},
	{	"dns_debug_all",	"dns.debug_all",			
			"%v (%v) [%v]: size=%v ref=%v expire=%vs last=%vs ago f=%v\n"
			"\t\t%v:%v expire=%vs f=%v\n"},
	{	"dst_blacklist_mem_info",	"dst_blacklist.mem_info",	"%v / %v\n"},
	{	"dst_blacklist_debug",		"dst_blacklist.debug",	
		"%v:%v:%v expire:%v flags: %v\n"},
	{0,0,0}
};


static struct sercmd_builtin builtins[]={
	{	"?",		sercmd_help, "help"},
	{	"help",		sercmd_help, "displays help for a command"},
	{	"version",	sercmd_ver,  "displays " NAME "version"},
	{	"quit",		sercmd_quit, "exits " NAME },
	{	"exit",		sercmd_quit, "exits " NAME },
	{	"warranty",		sercmd_warranty, "displays " NAME "'s warranty info"},
	{	"license",		sercmd_warranty, "displays " NAME "'s license"},
	{0,0}
};



#ifdef USE_READLINE

/* instead of rl_attempted_completion_over which is not present in
   some readline emulations */
static int attempted_completion_over=0; 

/* commands for which we complete the params to other command names */
char* complete_params[]={
	"?",
	"h",
	"help",
	"system.methodSignature",
	"system.methodHelp",
	0
};
#endif


static char *format4method(char *method)
{
	int r;
	
	for (r = 0; cmd_aliases[r].name; r ++) {
		if (strcmp(cmd_aliases[r].method, method) == 0)
			return cmd_aliases[r].format;
	}
	return NULL;
}

/* resolves builtin aliases */
static char *method4name(char *name, size_t nlen)
{
	int r;
	
	for (r = 0; cmd_aliases[r].name; r ++) {
		if (nlen != strlen(cmd_aliases[r].name))
			continue;
		if (strncmp(cmd_aliases[r].name, name, nlen) != 0)
			continue;
		return cmd_aliases[r].method;
	}
	return NULL;
}


#if 0
#define DEBUG_NEW_TOK
#endif
static tok_t *new_tok(char *str, size_t len, int force_str)
{
	tok_t *tok;
	int i;
	char save, *after, *curs, *end, code;

	if (! (tok = (tok_t *)malloc(sizeof(tok_t)))) {
		fprintf(stderr, "ERROR: out of memory.\n");
		exit(1);
	}
	tok->type = TOK_NONE;

#ifdef DEBUG_NEW_TOK
	printf("RAW: [%d] `%.*s'\n", len, len, str);
#endif

	if (! force_str) {
		/* check if could be a number */
		for (i = 0; i < len && str[i] == '#'; i ++)
			;
		/* only one hash or no hash can identify a number (<>more #'es) */
		if (i <= 1) {
			save = str[len]; /* so that strXXX() can be used */
			str[len] = 0;
			after = NULL;
			tok->i = (int)strtol(str + /*pass `#'*/i, &after, 
					10 + (i ? 6 : 0));
			if (after == str + len) { /* (full) conversion succeeded */
				tok->type = TOK_INT;
			} else {
				tok->d = strtod(str + /*pass `#'*/i, &after);
				if (after == str + len) {
					tok->type = TOK_FLT;
				}
			}
			str[len] = save;
		}
	}
	if (tok->type == TOK_NONE) {
		tok->type = TOK_STR;
		/* This is nasty: parse the string again, to remove escaping slahes.
		 * It could have been done in first pass, but need the line unmodified
		 * (so that 2nd tries during reconnects are possible) */
		if (! (tok->s.val = (char *)malloc(len * sizeof(char)))) {
			fprintf(stderr, "ERROR: out of memory.\n");
			exit(1);
		}
		memcpy(tok->s.val, str, len);

		for (curs = tok->s.val, end = tok->s.val + len; curs < end; 
				curs ++) {
			switch (*curs) {
				case '\\':
					switch (curs[1]) { /* always safe: orig. line has 0-term */
						case ' ':
						case '\t':
							if (force_str)
								break;
						case '\\':
						case '"':
							/* remove slash and copy rest */
							memcpy(curs, curs + 1, end - curs - 1);
							end --;
							break;

						do {
						case 'r': code = 0x0D; break;
						case 'n': code = 0x0A; break;
						} while (0);
							/* remove slash and replace by 0xa, 0xd */
							memcpy(curs, curs + 1, end - curs - 1);
							end --;
							*curs = code;
							break;

						case '%':
							/* remove slash, copy rest and go past `%' */
							memcpy(curs, curs + 1, end - curs - 1);
							end --;
							curs ++; /* so that it won't be reanalized */
							break;
					}
					break;

				case '%':
					if (end - curs < 2)
						/* bogus % */
						break;
					save = curs[/*%*/1 + /*xx*/2];
					curs[3] = 0;
					code = strtol(curs + 1, &after, 16);
					curs[3] = save;
					if (after != curs + 3)
						break; /* xy in %xy can not be a hexa */
					/* all fine */
					*curs = code;
					memcpy(curs + 1, curs + 3, end - curs - 3);
					end -= 2;
					break; /* formal */
			}
		}
		tok->s.len = end - tok->s.val;
#ifdef DEBUG_NEW_TOK
		printf("DISTILED: [%d] `%.*s'\n", tok->s.len, tok->s.len, tok->s.val);
#endif

	}

	return tok;
}

static brpc_val_t *val4tok(tok_t *tok)
{
	brpc_val_t *val = NULL; /* 4gcc */
	switch (tok->type) {
		case TOK_NONE:
			fprintf(stderr, "BUG: invalid token type (none).\n");
			abort();
			break;
		case TOK_STR:
			val = brpc_str(tok->s.val, tok->s.len);
			break;
		case TOK_INT:
			val = brpc_int(tok->i);
			break;
		case TOK_FLT: /*TODO: support float*/
			val = brpc_int(tok->d);
			break; /*formal*/
	}
	if (! val)
		fprintf(stderr, "ERROR: failed bo build BINRPC value: %s [%d].\n", 
				brpc_strerror(), brpc_errno);
	return val;
}


static brpc_val_t *parse_arg(char* arg)
{
	tok_t *tok;
	brpc_val_t *val = NULL;

	tok = new_tok(arg, strlen(arg), 0);
	val = val4tok(tok);
	free(tok);
	return val;
}

static brpc_t *parse_cmd(char** argv, int count)
{
	int r;
	brpc_t *req;
	brpc_val_t *val;
	
	req = brpc_req_c(argv[0], gen_cookie());
	if (! req) {
		fprintf(stderr, "ERROR: failed to build request frame: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		goto error;
	}
	for (r=1; r<count; r++){
		if (! (val = parse_arg(argv[r]))) {
			fprintf(stderr, "ERROR: failed to build BINRPC value for `%s':"
					" %s [%d].\n", argv[r], brpc_strerror(), brpc_errno);
			goto error;
		}
		if (! brpc_add_val(req, val)) {
			fprintf(stderr, "ERROR: failed to add value to request: %s [%d].\n"
					, brpc_strerror(), brpc_errno);
			goto error;
		}
	}
	return req;
error:
	if (req)
		brpc_finish(req);
	return NULL;
}

/* TODO: verbosity (hexdumps, print formating) */

/* returns a malloced copy of str, with all the escapes ('\') resolved */
static char* str_escape(char* str)
{
	char* n;
	char* ret;
	
	ret=n=malloc(strlen(str)+1);
	if (n==0)
		goto end;
	
	for(;*str;str++){
		*n=*str;
		if (*str=='\\'){
			switch(*(str+1)){
				case 'n':
					*n='\n';
					str++;
					break;
				case 'r':
					*n='\r';
					str++;
					break;
				case 't':
					*n='\t';
					str++;
					break;
				case '\\':
					str++;
					break;
			}
		}
		n++;
	}
	*n=*str; /* terminating 0 */
end:
	return ret;
}


static int print_fault(brpc_t *rpl)
{
	brpc_int_t *fault_code;
	brpc_str_t *fault_reason;

	if (! brpc_fault_status(rpl, &fault_code, &fault_reason)) {
		fprintf(stderr, "ERROR: failed to retrieve fault's code&reason:"
				" %s [%d].\n", brpc_strerror(), brpc_errno);
		return -1;
	}
	printf("Call faulted: ");
	
	printf("code: ");
	if (fault_code)
		printf("%d", *fault_code);
	else
		printf("[unspecified]");
	
	printf("; reason: ");
	if (fault_reason)
		printf("%s", fault_reason->val);
	else
		printf("[unspecified]");
	
	printf("\n");
	return 0;
}

/* prints all untill hits %v, and returns offset to post %v possition */
static size_t print_till_v(const char *fmt)
{
	bool running = true;
	bool hit_percent = false;
	const char *saved = fmt;

	while (running) {
		switch (*fmt) {
			case 0:
				running = false;
				continue;
			case '%':
				if (! hit_percent) {
					hit_percent = true;
					continue;
				}
				break;
			case 'v':
				if (hit_percent) {
					fmt ++; /*step forward, past %v */
					return fmt - saved;
				}
				break;
			default:
				if (hit_percent)
					/* just a weird descriptor */
					hit_percent = false;
		}
		if (! hit_percent)
			printf("%c", *fmt);
		fmt ++;
	}
	return fmt - saved;
}

static inline void print_indent(int count)
{
	int i;
	for (i = 0; i < count; i ++)
		printf(" ");
}

/* TODO: printing patterns for A/M/L */
static int print_reply(brpc_t *rpl, const char * const fmt)
{
	brpc_dissect_t *diss;
	const brpc_val_t *val;
	int res = -1;
	size_t offset = 0, limit;
	int indent = 0;
	char mark;
	bool iterate = true;

	diss = brpc_msg_dissector(rpl);
	if (! diss) {
		fprintf(stderr, "ERROR: failed to get BINRPC message dissector: "
				"%s [%d].\n", brpc_strerror(), brpc_errno);
		return -1;
	}
	limit = fmt ? strlen(fmt): 0;
	do {
		while (brpc_dissect_next(diss)) {
			val = brpc_dissect_fetch(diss);
			switch (brpc_val_type(val)) {
				case BRPC_VAL_INT:
				case BRPC_VAL_STR:
				case BRPC_VAL_BIN:
					if (fmt) {
						offset += print_till_v(fmt + offset);
						if (limit <= offset)
							/* reset the fmt pattern */
							offset = print_till_v(fmt);
					} else {
						print_indent(indent);
					}
					if (brpc_is_null(val))
						printf("<NULL>");
					switch (brpc_val_type(val)) {
						case BRPC_VAL_INT:
							printf("%d", brpc_int_val(val));
							break;
						case BRPC_VAL_STR:
							printf("%s", brpc_str_val(val).val);
							break;
						case BRPC_VAL_BIN: 
							printf("%.*s", (int)brpc_bin_val(val).len,
									brpc_bin_val(val).val); 
									break;
						default: ; /* ignore */
					}
					break;
				do {
				case BRPC_VAL_AVP: mark = '<'; break;
				case BRPC_VAL_MAP: mark = '{'; break;
				case BRPC_VAL_LIST: mark = '['; break;
				} while (0);
					if (! fmt) {
						print_indent(indent);
						printf("%c\n", mark);
						indent += INDENT_STEP;
					}
					if (! brpc_dissect_in(diss)) {
						fprintf(stderr, "ERROR: failed to dissect into "
								"sequence: %s [%d].\n", brpc_strerror(), 
								brpc_errno);
						goto err;
					}
					continue;
				default:
					fprintf(stderr, "unsupported value type %d.\n", 
							brpc_val_type(val));
			}
			if (! fmt)
				printf("\n");
		}
		switch (brpc_dissect_seqtype(diss)) {
			case BRPC_VAL_NONE:
				iterate = false;
				break;
			do {
			case BRPC_VAL_AVP: mark = '>'; break;
			case BRPC_VAL_MAP: mark = '}'; break;
			case BRPC_VAL_LIST: mark = ']'; break;
			} while (0);
				if (! brpc_dissect_out(diss)) {
					fprintf(stderr, "BUG: dissector error: %s [%d].\n", 
							brpc_strerror(), brpc_errno);
					goto err;
				}
				if (! fmt) {
					indent -= INDENT_STEP;
					print_indent(indent);
					printf("%c\n", mark);
				}
				break;
			default:
				fprintf(stderr, "BUG: invalid type of sequence: %d.\n",
						brpc_dissect_seqtype(diss));
				goto err;;
			
		}
	} while (iterate);

	if (fmt)
		/* flush any remainings */
		print_till_v(fmt + offset);
	res = 0;
err:
	brpc_dissect_free(diss);
	return res;
}

static int run_binrpc_cmd(int s, brpc_t *req, char* fmt)
{
	int ret = -1;
	const brpc_str_t *mname;
	brpc_t *rpl = NULL;

	if (! brpc_sendto(s, NULL, req, TX_TIMEOUT)) {
		fprintf(stderr, "ERROR: failed to send request: %s [%d].\n",
			brpc_strerror(), brpc_errno);
		ret = -2;
		goto finish;
	}
	rpl = brpc_recv(s, RX_TIMEOUT);
	if (! rpl) {
		fprintf(stderr, "ERROR: failed to receive reply: %s [%d].\n",
			brpc_strerror(), brpc_errno);
		ret = -2;
		goto finish;
	}
	if (brpc_type(rpl) != BRPC_CALL_REPLY) {
		fprintf(stderr, "ERROR: received non-reply answer.\n");
		goto finish;
	}
	if (brpc_id(req) != brpc_id(rpl)) {
		fprintf(stderr, "ERROR: received unrelated reply.\n");
		goto finish;
	}
	if (brpc_is_fault(rpl)) {
		ret = print_fault(rpl);
	} else {
		if (! fmt) {
			mname = brpc_method(req);
			if (! mname) {
				fprintf(stderr, "BUG: failed to retrieve method name: "
						"%s [%d].\n", brpc_strerror(), brpc_errno);
				goto finish;
			}
			fmt = format4method(mname->val);
		}
		print_reply(rpl, fmt);
		ret = 0;
	}
finish:
	if (rpl)
		brpc_finish(rpl);
	return ret;
}

/**
 * x"ab"y => x ab y
 * "ab" "cd$ => ab cd
 */
static tok_t **tokenize_line(char *line, size_t *toks_cnt)
{
	int ended;
	tok_t **toks = NULL;
	size_t cnt = 0;
	char *marker, *curs;

#define APPEND_TOK(_start_, _len, _force_str) \
	do { \
		if (! (toks = realloc(toks, (cnt + 1) * sizeof(tok_t *)))) { \
			fprintf(stderr, "ERROR: out of memory.\n"); \
			exit(1); \
		} \
		toks[cnt ++] = new_tok(_start_, _len, _force_str); \
	} while (0)

#define EAT_WS(_s) \
	do { \
		switch (*_s) { \
			case ' ': \
			case '\t': \
				_s ++; \
				continue; \
		} \
		break; \
	} while (1)

#define SET_MARKER(_s) \
	do { \
		EAT_WS(_s); \
		marker = _s; \
	} while (0)

	curs = line;
	SET_MARKER(curs);
	ended = 0;
	//for (curs = line, SET_MARKER(curs), ended = 0; ! ended; ) {
	while (! ended)
		switch (*curs) {
			case '"':
				if (marker != curs) /* `ab"CD"ef' => marker=@a */
					APPEND_TOK(marker, curs - marker, /*don't force*/0);
				else
					marker = curs + 1;
				curs ++;
				while (1) {
					while (*curs != '"')
						curs ++;
					if (curs[-1] != '\\')
						break;
				}
				APPEND_TOK(marker, curs - marker, /*force string*/1);
				curs ++;
				SET_MARKER(curs);
				break;

			case 0:
				ended = 1;
				if (! marker)
					break;
			case ' ':
			case '\t':
				APPEND_TOK(marker, curs - marker, /*don't force*/0);
				curs ++;
				SET_MARKER(curs);
				break;

			default:
				curs ++;
		}

	*toks_cnt = cnt;
	return toks;
#undef APPEND_TOK
#undef EAT_WS
#undef SET_MARKER
}

static brpc_t *parse_line(char* line)
{
	size_t cnt, i;
	tok_t **toks;
	char *fix;
	brpc_t *req = NULL;
	brpc_val_t *val;
	brpc_str_t mname;
	int ret = -1;

	if (! (toks = tokenize_line(line, &cnt)))
		/* just spaces? */
		return NULL;

	if (toks[0]->type != TOK_STR) {
		fprintf(stderr, "ERROR: request name must be a string (have %d).\n",
				toks[0]->type);
		goto end;
	}
	
	fix = method4name(toks[0]->s.val, toks[0]->s.len);
	if (fix) {
		mname.val = fix;
		mname.len = strlen(fix);
	} else {
		mname.val = toks[0]->s.val;
		mname.len = toks[0]->s.len;
	}
	if (! (req = brpc_req(mname, gen_cookie()))) {
		fprintf(stderr, "ERROR: failed to build request frame: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		return NULL;
	}

	for (i = 1; i < cnt; i ++) {
		if (! (val = val4tok(toks[i])))
			goto end;
		if (! brpc_add_val(req, val)) {
			fprintf(stderr, "ERROR: failed to add BINRPC value to "
					"request: %s [%d].\n", brpc_strerror(), brpc_errno);
			goto end;
		}
	}

	ret = 0;
end:
	for (i = 0; i < cnt; i ++) {
		if (toks[i]->type == TOK_STR)
			free(toks[i]->s.val);
		free(toks[i]);
	}
	free(toks);
	if ((ret < 0) && req) {
		brpc_finish(req);
		req = NULL;
	}
	return req;
}


/* intercept builtin commands, returns 1 if intercepted, 0 if not, <0 on error
 */
static int run_builtins(int s, brpc_t *req)
{
	int r;
	int ret;
	const brpc_str_t *mname;

	mname = brpc_method(req);
	if (! mname) {
		fprintf(stderr, "BUG: failed to retrieve method name: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		return 0; /* drop further attempts */
	}
	
	for (r=0; builtins[r].name; r++){
		if (strcmp(builtins[r].name, mname->val)==0){
			ret=builtins[r].f(s, req);
			return (ret<0)?ret:1;
		}
	}
	return 0;
}



/* runs command from cmd */
inline static int run_cmd(int s, brpc_t *req, char* format)
{
	int ret;

	if (!(ret=run_builtins(s, req))){
		ret=run_binrpc_cmd(s, req, format);
	}
	brpc_finish(req);
	return ret;
}



/* runs a command represented in line */
inline static int run_line(int s, char* l, char* format)
{
	brpc_t *req;
	return ((req = parse_line(l))) ? run_cmd(s, req, format) : -1;
}


void free_cmds()
{
	int i;
	if (cmds_nr == 0)
		return;
	for (i = 0; i < cmds_nr; i ++)
		free(commands[i].val);
	free(commands);
	commands = NULL;
	cmds_nr = 0;
}

static int get_sercmd_list(int s)
{
	BRPC_STR_STATIC_INIT(mname, "system.listMethods");
	brpc_t *req = NULL, *rpl = NULL;
	int ret = -1;
	char *repr = NULL, *pos, *dup;
	int i;
	void **vals = NULL;
	brpc_str_t *cmd;

	/*if previously invoked, discard list */
	if (commands)
		free_cmds();
	
	if (! (req = brpc_req(mname, gen_cookie()))) {
		fprintf(stderr, "ERROR: failed to build request: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		goto end;
	}
	if (! brpc_sendto(s, NULL, req, TX_TIMEOUT)) {
		fprintf(stderr, "ERROR: failed to send request: %s [%d].\n", 
				brpc_strerror(), brpc_errno);
		ret = -2;
		goto end;
	}
	if (! (rpl = brpc_recv(s, RX_TIMEOUT))) {
		fprintf(stderr, "ERROR: failed to retrieve response: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		ret = -2;
		goto end;
	}
	if (! (repr = brpc_repr(rpl, &cmds_nr))) {
		fprintf(stderr, "ERROR: SER returned empty result (BUG?).\n");
		goto end;
	}
	if (! (dup = (char *)malloc((cmds_nr + /*for leading `!'*/1 + 
			/*0-term*/1) * sizeof(char))))
		goto outofmem;
	*dup = '!';
	memcpy(dup + 1, repr, cmds_nr + /*0-term*/1);
	free(repr);
	repr = dup;
	
	if (! (vals = (void **)malloc(cmds_nr * sizeof(void *))))
		goto outofmem;
	
	if (! brpc_dsm(rpl, repr, vals)) {
		fprintf(stderr, "ERROR: failed to unpack reply: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		goto end;
	}

	if (! (commands = (brpc_str_t *)malloc(cmds_nr * sizeof(brpc_str_t))))
		goto outofmem;
	for (pos = repr + /*skip !*/1, i = 0; *pos; pos ++, i ++) {
		switch (*pos) {
			case 0: break;
			case 's':
				cmd = (brpc_str_t *)vals[i];
				if (! cmd) {
					fprintf(stderr, "ERROR: unexpected NULL value as command"
							" name (BUG?).\n");
						goto freecmds;
				}
				commands[i].len = cmd->len;
				if (! (commands[i].val = (char *)malloc(cmd->len * 
						sizeof(char))))
					goto freecmds;
				memcpy(commands[i].val, cmd->val, cmd->len);
				printf("%s\n", cmd->val);
				break;
			default:
				fprintf(stderr, "ERROR: unexpected descriptor `%c' (0x%x) in "
						"representation `%s' (BUG?).\n", *pos, *pos, repr);
				goto end;
		}
	}

	ret = 0;
	goto end;
freecmds:
	cmds_nr = i -1;
	if (commands) {
		free_cmds();
		commands = NULL;
	}
	goto end; //nasty
outofmem:
	fprintf(stderr, "ERROR: ouf of system memory.\n");
end:
	if (vals)
		free(vals);
	if (repr)
		free(repr);
	if (req)
		brpc_finish(req);
	if (rpl)
		brpc_finish(rpl);
	return ret;
}

static int sercmd_help(int s, brpc_t *helper)
{
	int r;
	brpc_t *req = NULL;
	const brpc_val_t *val;
	brpc_val_t *clone;
	brpc_dissect_t *diss = NULL;
	BRPC_STR_STATIC_INIT(get_help, "system.methodHelp");
	size_t cnt;
	int ret = -1;

	if (! (diss = brpc_msg_dissector(helper))) {
		fprintf(stderr, "ERROR: failed to dissect helper: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		goto error;
	}
	if ((cnt = brpc_dissect_cnt(diss)) == 0) {
		if ((ret = get_sercmd_list(s)) < 0)
			goto error;

		for (r = 0; r < cmds_nr; r ++)
			printf("%s\n", commands[r].val);
		for (r=0; cmd_aliases[r].name; r++){
			printf("alias: %s\n", cmd_aliases[r].name);
		}
		for(r=0; builtins[r].name; r++){
			printf("builtin: %s\n", builtins[r].name);
		}
		ret = 0;
	} else {
		if (! (req = brpc_req(get_help, gen_cookie()))) {
			fprintf(stderr, "ERROR: failed to build help request: "
					"%s [%d].\n", brpc_strerror(), brpc_errno);
			goto error;
		}
		brpc_dissect_next(diss); /*_cnt returned 1: assume OK*/
		val = brpc_dissect_fetch(diss);
		if (brpc_val_type(val) != BRPC_VAL_STR) {
			fprintf(stderr, "ERROR: invalid token for help request.\n");
			goto error;
		}
		if (1 < cnt)
			fprintf(stderr, "WARNING: multiple tokens supplied for help "
					"request; considering only first (`%s').\n", 
					brpc_str_val(val).val);
		clone = brpc_val_clone(val); /* old val is just a ref in helper */
		if (! brpc_add_val(req, clone)) {
			fprintf(stderr, "ERROR: failed to clone help token: %s [%d].\n",
					brpc_strerror(), brpc_errno);
			goto error;
		}
		ret = run_binrpc_cmd(s, req, 0);
	}

error:
	if (diss)
		brpc_dissect_free(diss);
	return ret;
}



static int sercmd_ver(int s, brpc_t *_)
{
	printf("%s\n", version);
	printf("%s\n", id);
	printf("%s compiled on %s \n", __FILE__, compiled);
#ifdef USE_READLINE
	printf("interactive mode command completion support\n");
#endif
	return 0;
}



static int sercmd_quit(int s, brpc_t *_)
{
	quit=1;
	return 0;
}



static int sercmd_warranty(int s, brpc_t *_)
{
	printf("%s %s\n", NAME, VERSION);
	printf("%s\n", COPYRIGHT);
	printf("\n%s\n", LICENSE);
	return 0;
}


#ifdef USE_READLINE

/* readline command generator */
static char* sercmd_generator(const char* text, int state)
{
	static int idx;
	static int list; /* aliases, builtins, rpc_array */
	static int len;
	char* name;
	
	if (attempted_completion_over)
		return 0;
	
	if (state==0){
		/* init */
		idx=list=0;
		len=strlen(text);
	}
	/* return next partial match */
	switch(list){
		case 0: /* aliases*/
			while((name=cmd_aliases[idx].name)){
				idx++;
				if (strncmp(name, text, len)==0)
					return strdup(name);
			}
			list++;
			idx=0;
			/* no break */
		case 1: /* builtins */
			while((name=builtins[idx].name)){
				idx++;
				if (strncmp(name, text, len)==0)
					return strdup(name);
			}
			list++;
			idx=0;
			/* no break */
		case 2: /* rpc_array */
			while(idx < cmds_nr){
				name = commands[idx ++].val;
				if (strncmp(name, text, len) == 0)
					return strdup(name);
			}
	}
	/* no matches */
	return 0;
}


char** sercmd_completion(const char* text, int start, int end)
{
	int r;
	int i;
	
	attempted_completion_over=1;
	/* complete only at beginning */
	if (start==0){
		attempted_completion_over=0;
	}else{ /* or if this is a command for which we complete the parameters */
		/* find first whitespace */
		for(r=0; (r<start) && (rl_line_buffer[r]!=' ') && 
				(rl_line_buffer[r]!='\t'); r++);
		for(i=0; complete_params[i]; i++){
			if ((r==strlen(complete_params[i])) &&
					(strncmp(rl_line_buffer, complete_params[i], r)==0)){
					attempted_completion_over=0;
					break;
			}
		}
	}
	return 0; /* let readline call sercmd_generator */
}

#endif /* USE_READLINE */



void log_libbinrpc(int level, const char *fmt, ...)
{
	va_list ap;
	if ((level & /*no facility*/0x7) <= (verbose + 3 + ((3<=verbose)?1:0))) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

int connect2ser(char *uri)
{
	brpc_addr_t *ser_addr;
	int s = -1;
#ifdef WITH_LOCDGRAM
	int tmpfd;
	brpc_addr_t loc_addr;
#endif

	ser_addr = brpc_parse_uri(uri);
	if (! ser_addr) {
		fprintf(stderr, "ERROR: failed to parse URI `%s': %s [%d].\n", uri,
				brpc_strerror(), brpc_errno);
		return -1;
	}
#ifdef WITH_LOCDGRAM
	if ((BRPC_ADDR_DOMAIN(ser_addr) == PF_LOCAL) && 
			(BRPC_ADDR_TYPE(ser_addr) == SOCK_DGRAM)) {
		if (tmppath) {
			unlink(tmppath);
			free(tmppath);
		}
		if (! (tmppath = strdup(TMP_TEMPLATE))) {
			fprintf(stderr, "ERROR: out of memory [%d].\n", errno);
			return -1;
		}
		if ((tmpfd = mkstemp(tmppath)) < 0) {
			fprintf(stderr, "ERROR: failed to create local unix socket: "
					"%s [%d].\n", strerror(errno), errno);
			return -1;
		}
		close(tmpfd);
		unlink(tmppath);
		if (sizeof(BRPC_ADDR_UN(&loc_addr)->sun_path) < 
				strlen(tmppath) + /*0-term*/1) {
			fprintf(stderr, "BUG: temporary socket template too long.\n");
			return -1;
		}
		loc_addr = *ser_addr;
		memcpy(BRPC_ADDR_UN(&loc_addr)->sun_path, tmppath, 
				strlen(tmppath) + /*0-term*/1);
		BRPC_ADDR_LEN(&loc_addr) = SUN_LEN(BRPC_ADDR_UN(&loc_addr));
		if ((s = brpc_socket(&loc_addr, /*blocking*/true, /*name*/true)) < 0) {
			fprintf(stderr, "ERROR: failed to create named socket: %s [%d].\n",
					brpc_strerror(), brpc_errno);
			return -1;
		}
	}
#endif

	if (! brpc_connect(ser_addr, &s, RX_TIMEOUT)) {
		fprintf(stderr, "ERROR: failed to connect to location `%s': %s "
				"[%d].\n", uri, brpc_strerror(), brpc_errno);
	}
	return s;
}

/* ugly piece of code (fits the rest :) */
void reconn_run(int *s, char *l, char *format, char *uri)
{
	int reconn = 0;
	do {
		if (*s < 0) {
			if ((*s = connect2ser(uri)) < 0)
				return;
			else
				reconn ++;
		}
		if (run_line(*s, l, format) == -2) {
			close(*s);
			*s = -1;
			/* server disconnected in the meanwhile */
			if (! reconn) {
				reconn ++;
				fprintf(stderr, "INFO: trying to reconnect.\n");
				continue;
			}
		}
		break;
	} while (1);
}

void sigint_handler(int _)
{
	printf("\n");
	if (quit_on_sigint)
		exit(0);
	printf(NAME "> ");
}

int main(int argc, char** argv)
{
	char* uri;
	brpc_t *req;

	int c;
	int s;
	char* format;
	int interactive;
	char* line;
	char* l;
	int ret = -1;

	quit=0;
	format=0;
	line=0;
	interactive=0;
	s=-1;
	uri = NULL;
	opterr=0;
	while((c=getopt(argc, argv, "Vhu:vf:i"))!=-1){
		switch(c){
			case 'V':
				printf("version: %s\n", version);
				printf("%s\n", id);
				printf("%s compiled on %s \n", __FILE__,
						compiled);
				exit(0);
				break;
			case 'h':
				printf("version: %s\n", version);
				printf("%s", help_msg);
				exit(0);
				break;
			case 'u':
				uri = optarg;
				break;
			case 'v':
				verbose++;
				break;
			case 'f':
				format=str_escape(optarg);
				if (format==0){
					fprintf(stderr, "ERROR: memory allocation failure\n");
					goto end;
				}
				break;
			case 'i':
				quit_on_sigint = 1;
				break;
			case '?':
				if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, 
							"Unknown option character `\\x%x'.\n",
							optopt);
				goto end;
			case ':':
				fprintf(stderr, 
						"Option `-%c' requires an argument.\n",
						optopt);
				goto end;
			default:
				abort();
		}
	}
	if (! uri){
		uri = DEFAULT_MODBRPC_SOCKET;
	}
	
	/* init the random number generator */
	srand(getpid()+time(0)); /* we don't need very strong random numbers */

	brpc_log_setup(log_libbinrpc);

	if ((s = connect2ser(uri)) < 0)
		goto end;
	
	if (optind>=argc){
			interactive=1;
			/*fprintf(stderr, "ERROR: no command specified\n");
			goto end; */
	}else{
		if (! (req = parse_cmd(&argv[optind], argc-optind)))
			goto end;
		if (run_cmd(s, req, format)<0) 
			goto end;
		goto end;
	}
	signal(SIGINT, sigint_handler);
	/* interactive mode */
	if (get_sercmd_list(s) == -2) {
		close(s);
		s = -1;
	}
	/* banners */
	printf("%s %s\n", NAME, VERSION);
	printf("%s\n", COPYRIGHT);
	printf("%s\n", DISCLAIMER);
#ifdef USE_READLINE
	
	/* initialize readline */
	/* allow conditional parsing of the ~/.inputrc file*/
	rl_readline_name=NAME; 
	rl_completion_entry_function=sercmd_generator;
	rl_attempted_completion_function=sercmd_completion;
	
	while(!quit){
		line=readline(NAME "> ");
		if (line==0) /* EOF */
			break; 
		l=trim_ws(line); /* trim whitespace */
		if (*l){
			add_history(l);
			reconn_run(&s, l, format, uri);
		}
		free(line);
		line=0;
	}
#else
	line=malloc(MAX_LINE_SIZE);
	if (line==0){
		fprintf(stderr, "memory allocation error\n");
		goto end;
	}
	printf(NAME "> "); fflush(stdout); /* prompt */
	while(!quit && fgets(line, MAX_LINE_SIZE, stdin)){
		l=trim_ws(line);
		if (*l){
			reconn_run(&s, l, format, uri);
		}
		printf(NAME "> "); fflush(stdout); /* prompt */
	}
	free(line);
	line=0;
#endif /* USE_READLINE */
	
	ret = 0;
end:
	/* normal exit */
	if (line)
		free(line);
	if (format)
		free(format);
#ifdef WITH_LOCDGRAM
	if (tmppath) {
		unlink(tmppath);
		free(tmppath);
	}
#endif
	return ret;
}
