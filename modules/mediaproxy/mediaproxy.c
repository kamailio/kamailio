/* 
 * Copyright (C) 2004-2008 Dan Pascu
 * Copyright (C) 2009 Juha Heinanen (multipart hack)
 *
 * This file is part of SIP-Router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../pvar.h"
#include "../../error.h"
#include "../../data_lump.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_param.h"
#include "../../msg_translator.h"
#include "../../modules/dialog/dlg_load.h"
#include "../../modules/dialog/dlg_hash.h"


MODULE_VERSION


#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
# define INLINE inline
#else
# define INLINE
#endif

/* WARNING: Keep this aligned with parser/msg_parser.h! */
#define FL_USE_MEDIA_PROXY (1<<30)

#define SIGNALING_IP_AVP_SPEC  "$avp(s:signaling_ip)"
#define MEDIA_RELAY_AVP_SPEC   "$avp(s:media_relay)"
#define ICE_CANDIDATE_AVP_SPEC "$avp(s:ice_candidate)"

#define NO_CANDIDATE -1

// Although `AF_LOCAL' is mandated by POSIX.1g, `AF_UNIX' is portable to
// more systems.  `AF_UNIX' was the traditional name stemming from BSD, so
// even most POSIX systems support it.  It is also the name of choice in
// the Unix98 specification. So if there's no AF_LOCAL fallback to AF_UNIX
#ifndef AF_LOCAL
# define AF_LOCAL AF_UNIX
#endif

// As Solaris does not have the MSG_NOSIGNAL flag for send(2) syscall,
// it is defined as 0
#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif


#define isnulladdr(adr)  ((adr).len==7 && memcmp("0.0.0.0", (adr).s, 7)==0)
#define isnullport(port) ((port).len==1 && (port).s[0]=='0')

#define STR_MATCH(str, buf)  ((str).len==strlen(buf) && memcmp(buf, (str).s, (str).len)==0)
#define STR_IMATCH(str, buf) ((str).len==strlen(buf) && strncasecmp(buf, (str).s, (str).len)==0)

#define STR_HAS_PREFIX(str, prefix)  ((str).len>=(prefix).len && memcmp((prefix).s, (str).s, (prefix).len)==0)
#define STR_HAS_IPREFIX(str, prefix) ((str).len>=(prefix).len && strncasecmp((prefix).s, (str).s, (prefix).len)==0)


typedef int Bool;
#define True  1
#define False 0


typedef Bool (*NatTestFunction)(struct sip_msg *msg);


typedef enum {
    TNone=0,
    TSupported,
    TUnsupported
} TransportType;

#define RETRY_INTERVAL 10
#define BUFFER_SIZE    8192

typedef struct MediaproxySocket {
    char *name;             // name
    int  sock;              // socket
    int  timeout;           // how many miliseconds to wait for an answer
    time_t last_failure;    // time of the last failure
    char data[BUFFER_SIZE]; // buffer for the answer data
} MediaproxySocket;


typedef struct {
    const char *name;
    uint32_t address;
    uint32_t mask;
} NetInfo;

typedef struct {
    str type;      // stream type (`audio', `video', `image', ...)
    str ip;
    str port;
    str rtcp_ip;   // pointer to the rtcp IP if explicitly specified by stream
    str rtcp_port; // pointer to the rtcp port if explicitly specified by stream
    str direction;
    Bool local_ip; // true if the IP is locally defined inside this media stream
    Bool has_ice;
    Bool has_rtcp_ice;
    TransportType transport;
    char *start_line;
    char *next_line;
    char *first_ice_candidate;
} StreamInfo;

#define MAX_STREAMS 32
typedef struct SessionInfo {
    str ip;
    str ip_line;   // pointer to the whole session level ip line
    str direction;
    str separator;
    StreamInfo streams[MAX_STREAMS];
    unsigned int stream_count;
    unsigned int supported_count;
} SessionInfo;

typedef struct AVP_Param {
    str spec;
    int_str name;
    unsigned short type;
} AVP_Param;

typedef struct ice_candidate_data {
    unsigned int priority;
    Bool skip_next_reply;
} ice_candidate_data;

// Function prototypes
//
static int EngageMediaProxy(struct sip_msg *msg);
static int UseMediaProxy(struct sip_msg *msg);
static int EndMediaSession(struct sip_msg *msg);

static int mod_init(void);
static int child_init(int rank);


// Module global variables and state
//
static int mediaproxy_disabled = False;
static str ice_candidate = str_init("none");

static MediaproxySocket mediaproxy_socket = {
    "/var/run/mediaproxy/dispatcher.sock", // name
    -1,                                    // sock
    500,                                   // timeout in 500 miliseconds if there is no answer
    0,                                     // time of the last failure
    ""                                     // data
};


struct dlg_binds dlg_api;
Bool have_dlg_api = False;
static int dialog_flag = -1;

// The AVP where the caller signaling IP is stored (if defined)
static AVP_Param signaling_ip_avp = {str_init(SIGNALING_IP_AVP_SPEC), {0}, 0};

// The AVP where the application-defined media relay IP is stored
static AVP_Param media_relay_avp = {str_init(MEDIA_RELAY_AVP_SPEC), {0}, 0};

// The AVP where the ICE candidate priority is stored (if defined)
static AVP_Param ice_candidate_avp = {str_init(ICE_CANDIDATE_AVP_SPEC), {0}, 0};

static cmd_export_t commands[] = {
    {"engage_media_proxy", (cmd_function)EngageMediaProxy, 0, 0, 0, REQUEST_ROUTE},
    {"use_media_proxy",    (cmd_function)UseMediaProxy,    0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
    {"end_media_session",  (cmd_function)EndMediaSession,  0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

static param_export_t parameters[] = {
    {"disable",            INT_PARAM, &mediaproxy_disabled},
    {"mediaproxy_socket",  PARAM_STRING, &(mediaproxy_socket.name)},
    {"mediaproxy_timeout", INT_PARAM, &(mediaproxy_socket.timeout)},
    {"signaling_ip_avp",   PARAM_STR, &(signaling_ip_avp.spec)},
    {"media_relay_avp",    PARAM_STR, &(media_relay_avp.spec)},
    {"ice_candidate",      PARAM_STR, &(ice_candidate)},
    {"ice_candidate_avp",  PARAM_STR, &(ice_candidate_avp.spec)},
    {0, 0, 0}
};

struct module_exports exports = {
    "mediaproxy",    // module name
    DEFAULT_DLFLAGS, // dlopen flags
    commands,        // exported functions
    parameters,      // exported parameters
    NULL,            // exported statistics
    NULL,            // exported MI functions
    NULL,            // exported pseudo-variables
    NULL,            // extra processes
    mod_init,        // module init function (before fork. kids will inherit)
    NULL,            // reply processing function
    NULL,            // destroy function
    child_init       // child init function
};



// String processing functions
//

// strfind() finds the start of the first occurrence of the substring needle
// of length nlen in the memory area haystack of length len.
static void*
strfind(const void *haystack, size_t len, const void *needle, size_t nlen)
{
    char *sp;

    // Sanity check
    if(!(haystack && needle && nlen && len>=nlen))
        return NULL;

    for (sp = (char*)haystack; sp <= (char*)haystack + len - nlen; sp++) {
        if (*sp == *(char*)needle && memcmp(sp, needle, nlen)==0) {
            return sp;
        }
    }

    return NULL;
}

// strcasefind() finds the start of the first occurrence of the substring
// needle of length nlen in the memory area haystack of length len by doing
// a case insensitive search
static void*
strcasefind(const char *haystack, size_t len, const char *needle, size_t nlen)
{
    char *sp;

    // Sanity check
    if(!(haystack && needle && nlen && len>=nlen))
        return NULL;

    for (sp = (char*)haystack; sp <= (char*)haystack + len - nlen; sp++) {
        if (tolower(*sp) == tolower(*(char*)needle) &&
            strncasecmp(sp, needle, nlen)==0) {
            return sp;
        }
    }

    return NULL;
}

// returns string with whitespace trimmed from left end
static INLINE void
ltrim(str *string)
{
    while (string->len>0 && isspace((int)*(string->s))) {
        string->len--;
        string->s++;
    }
}

// returns string with whitespace trimmed from right end
static INLINE void
rtrim(str *string)
{
    char *ptr;

    ptr = string->s + string->len - 1;
    while (string->len>0 && (*ptr==0 || isspace((int)*ptr))) {
        string->len--;
        ptr--;
    }
}

// returns string with whitespace trimmed from both ends
static INLINE void
trim(str *string)
{
    ltrim(string);
    rtrim(string);
}

// returns a pointer to first CR or LF char found or the end of string
static char*
findendline(char *string, int len)
{
    char *ptr = string;

    while(ptr - string < len && *ptr != '\n' && *ptr != '\r')
        ptr++;

    return ptr;
}


static int
strtoint(str *data)
{
    long int result;
    char c;

    // hack to avoid copying the string
    c = data->s[data->len];
    data->s[data->len] = 0;
    result = strtol(data->s, NULL, 10);
    data->s[data->len] = c;

    return (int)result;
}


// find a line in str `block' that starts with `start'.
static char*
find_line_starting_with(str *block, char *start, int ignoreCase)
{
    char *ptr, *bend;
    str zone;
    int tlen;

    bend = block->s + block->len;
    tlen = strlen(start);
    ptr = NULL;

    for (zone = *block; zone.len > 0; zone.len = bend - zone.s) {
        if (ignoreCase)
            ptr = strcasefind(zone.s, zone.len, start, tlen);
        else
            ptr = strfind(zone.s, zone.len, start, tlen);
        if (!ptr || ptr==block->s || ptr[-1]=='\n' || ptr[-1]=='\r')
            break;
        zone.s = ptr + tlen;
    }

    return ptr;
}


// count all lines in str `block' that starts with `start'.
static unsigned int
count_lines_starting_with(str *block, char *start, int ignoreCase)
{
    char *ptr, *bend;
    str zone;
    int tlen;
    unsigned count;

    bend = block->s + block->len;
    tlen = strlen(start);

    count = 0;

    for (zone = *block; zone.len > 0; zone.len = bend - zone.s) {
        if (ignoreCase)
            ptr = strcasefind(zone.s, zone.len, start, tlen);
        else
            ptr = strfind(zone.s, zone.len, start, tlen);
        if (!ptr)
            break;
        if (ptr==block->s || ptr[-1]=='\n' || ptr[-1]=='\r')
            count++;
        zone.s = ptr + tlen;
    }

    return count;
}


// get up to `limit' whitespace separated tokens from `char *string'
static int
get_tokens(char *string, str *tokens, int limit)
{
    int i, len, size;
    char *ptr;

    if (!string) {
        return 0;
    }

    len  = strlen(string);

    for (ptr=string, i=0; i<limit && len>0; i++) {
        size = strspn(ptr, " \t\n\r");
        ptr += size;
        len -= size;
        if (len <= 0)
            break;
        size = strcspn(ptr, " \t\n\r");
        if (size==0)
            break;
        tokens[i].s = ptr;
        tokens[i].len = size;
        ptr += size;
        len -= size;
    }

    return i;
}

// get up to `limit' whitespace separated tokens from `str *string'
static int
get_str_tokens(str *string, str *tokens, int limit)
{
    int count;
    char c;

    if (!string || !string->s) {
        return 0;
    }

    c = string->s[string->len];
    string->s[string->len] = 0;

    count = get_tokens(string->s, tokens, limit);

    string->s[string->len] = c;

    return count;
}


// Functions to extract the info we need from the SIP/SDP message
//

static Bool
get_callid(struct sip_msg* msg, str *cid)
{
    if (msg->callid == NULL) {
        if (parse_headers(msg, HDR_CALLID_F, 0) == -1) {
            LM_ERR("cannot parse Call-ID header\n");
            return False;
        }
        if (msg->callid == NULL) {
            LM_ERR("missing Call-ID header\n");
            return False;
        }
    }

    *cid = msg->callid->body;

    trim(cid);

    return True;
}

static Bool
get_cseq_number(struct sip_msg *msg, str *cseq)
{
    if (msg->cseq == NULL) {
        if (parse_headers(msg, HDR_CSEQ_F, 0)==-1) {
            LM_ERR("cannot parse CSeq header\n");
            return False;
        }
        if (msg->cseq == NULL) {
            LM_ERR("missing CSeq header\n");
            return False;
        }
	}

	*cseq = get_cseq(msg)->number;

    if (cseq->s==NULL || cseq->len==0) {
        LM_ERR("missing CSeq number\n");
        return False;
    }

    return True;
}

static str
get_from_uri(struct sip_msg *msg)
{
    static str unknown = str_init("unknown");
    str uri;
    char *ptr;

    if (parse_from_header(msg) < 0) {
        LM_ERR("cannot parse the From header\n");
        return unknown;
    }

    uri = get_from(msg)->uri;

    if (uri.len == 0)
        return unknown;

    if (strncasecmp(uri.s, "sip:", 4)==0) {
        uri.s += 4;
        uri.len -= 4;
    }

    if ((ptr = strfind(uri.s, uri.len, ";", 1))!=NULL) {
        uri.len = ptr - uri.s;
    }

    return uri;
}


static str
get_to_uri(struct sip_msg *msg)
{
    static str unknown = str_init("unknown");
    str uri;
    char *ptr;

    if (!msg->to) {
        LM_ERR("missing To header\n");
        return unknown;
    }

    uri = get_to(msg)->uri;

    if (uri.len == 0)
        return unknown;

    if (strncasecmp(uri.s, "sip:", 4)==0) {
        uri.s += 4;
        uri.len -= 4;
    }

    if ((ptr = strfind(uri.s, uri.len, ";", 1))!=NULL) {
        uri.len = ptr - uri.s;
    }

    return uri;
}


static str
get_from_tag(struct sip_msg *msg)
{
    static str undefined = str_init("");
    str tag;

    if (parse_from_header(msg) < 0) {
        LM_ERR("cannot parse the From header\n");
        return undefined;
    }

    tag = get_from(msg)->tag_value;

    if (tag.len == 0)
        return undefined;

    return tag;
}


static str
get_to_tag(struct sip_msg *msg)
{
    static str undefined = str_init("");
    str tag;

    if (msg->first_line.type==SIP_REPLY && msg->REPLY_STATUS<200) {
        // Ignore the To tag for provisional replies
        return undefined;
    }

    if (!msg->to) {
        LM_ERR("missing To header\n");
        return undefined;
    }

    tag = get_to(msg)->tag_value;

    if (tag.len == 0)
        return undefined;

    return tag;
}


static str
get_user_agent(struct sip_msg* msg)
{
    static str unknown = str_init("unknown agent");
    str block, server;
    char *ptr;

    if (parse_headers(msg, HDR_USERAGENT_F, 0)==0 && msg->user_agent &&
        msg->user_agent->body.s && msg->user_agent->body.len>0) {
        return msg->user_agent->body;
    }

    // If we can't find user-agent, look after the `Server' header
    // This is a temporary hack. Normally it should be extracted by sip-router.

    block.s   = msg->buf;
    block.len = msg->len;

    ptr = find_line_starting_with(&block, "Server:", True);
    if (!ptr)
        return unknown;

    server.s   = ptr + 7;
    server.len = findendline(server.s, block.s+block.len-server.s) - server.s;

    trim(&server);
    if (server.len == 0)
        return unknown;

    return server;
}


// Get caller signaling IP
static str
get_signaling_ip(struct sip_msg* msg)
{
    int_str value;

    if (!search_first_avp(signaling_ip_avp.type | AVP_VAL_STR,
                          signaling_ip_avp.name, &value, NULL) ||
        value.s.s==NULL || value.s.len==0) {

        value.s.s = ip_addr2a(&msg->rcv.src_ip);
        value.s.len = strlen(value.s.s);
    }

    return value.s;
}

// Get the application-defined media_relay if defined
static str
get_media_relay(struct sip_msg* msg)
{
    static str undefined = str_init("");
    int_str value;

    if (!search_first_avp(media_relay_avp.type | AVP_VAL_STR,
                          media_relay_avp.name, &value, NULL) || value.s.s==NULL || value.s.len==0) {
        return undefined;
    }

    return value.s;
}


// Functions to manipulate the SDP message body
//


static int
find_content_type_application_sdp(struct sip_msg *msg, str *sdp)
{
    str type, params, boundary;
    char *start, *s;
    unsigned int len;
    Bool done;
    param_hooks_t hooks;
    param_t *p, *list;

    if (!msg->content_type) {
        LM_WARN("the Content-Type header is missing! Assume the content type is text/plain\n");
        return 1;
    }

    type = msg->content_type->body;
    trim(&type);

    if (strncasecmp(type.s, "application/sdp", 15) == 0) {
	done = True;
    } else if (strncasecmp(type.s, "multipart/mixed", 15) == 0) {
	done = False;
    } else {
	LM_ERR("invalid Content-Type for SDP: %.*s\n", type.len, type.s);
	return -1;
    }

    if (!(isspace((int)type.s[15]) || type.s[15] == ';' || type.s[15] == 0)) {
        LM_ERR("invalid character after Content-Type: `%c'\n", type.s[15]);
        return -1;
    }

    if (done) return 1;

    // Hack to find application/sdp bodypart
    params.s = memchr(msg->content_type->body.s, ';', 
		      msg->content_type->body.len);
    if (params.s == NULL) {
	LM_ERR("Content-Type hdr has no params\n");
	return -1;
    }
    params.len = msg->content_type->body.len - 
	(params.s - msg->content_type->body.s);
    if (parse_params(&params, CLASS_ANY, &hooks, &list) < 0) {
	LM_ERR("while parsing Content-Type params\n");
	return -1;
    }
    boundary.s = NULL;
    boundary.len = 0;
    for (p = list; p; p = p->next) {
	if ((p->name.len == 8)
	    && (strncasecmp(p->name.s, "boundary", 8) == 0)) {
	    boundary.s = pkg_malloc(p->body.len + 2 + 1);
	    if (boundary.s == NULL) {
		free_params(list);
		LM_ERR("no memory for boundary string\n");
		return -1;
	    }
	    *(boundary.s) = '-';
	    *(boundary.s + 1) = '-';
	    memcpy(boundary.s + 2, p->body.s, p->body.len);
	    boundary.len = 2 + p->body.len;
	    *(boundary.s + boundary.len) = 0;
	    LM_DBG("boundary is <%.*s>\n", boundary.len, boundary.s);
	    break;
	}
    }
    free_params(list);
    if (boundary.s == NULL) {
	LM_ERR("no mandatory param \";boundary\"\n");
	return -1;
    }

    while ((s = find_line_starting_with(sdp, "Content-Type: ", True))) {
	start = s + 14;
	len = sdp->len - (s - sdp->s) - 14;
	if (len > 15 + 2) {
	    if (strncasecmp(start, "application/sdp", 15) == 0) {
		start = start + 15;
		if ((*start != 13) || (*(start + 1) != 10)) {
		    LM_ERR("no CRLF found after content type\n");
		    goto err;
		}
		start = start + 2;
		len = len - 15 - 2;
		while ((len > 0) && ((*start == 13) || (*start == 10))) {
		    len = len - 1;
		    start = start + 1;
		}
		sdp->s = start;
		sdp->len = len;
		s = find_line_starting_with(sdp, boundary.s, False);
		if (s == NULL) {
		    LM_ERR("boundary not found after bodypart\n");
		    goto err;
		}
		sdp->len = s - start - 2;
		pkg_free(boundary.s);
		return 1;
	    }
	}
    }
    LM_ERR("no application/sdp bodypart found\n");

 err:
    pkg_free(boundary.s);
    return -1;
}


// Get the SDP message from SIP message and check it's Content-Type
// Return values:
//    1 - success
//   -1 - error in getting body or invalid content type
//   -2 - empty message
static int
get_sdp_message(struct sip_msg *msg, str *sdp)
{
    sdp->s = get_body(msg);
    if (sdp->s==NULL) {
        LM_ERR("cannot get the SDP body\n");
        return -1;
    }

    sdp->len = msg->buf + msg->len - sdp->s;
    if (sdp->len == 0)
        return -2;

    return find_content_type_application_sdp(msg, sdp);
}


// Return a str containing the line separator used in the SDP body
static str
get_sdp_line_separator(str *sdp)
{
    char *ptr, *end_ptr, *sdp_end;
    str separator;

    sdp_end = sdp->s + sdp->len;

    ptr = find_line_starting_with(sdp, "v=", False);
    end_ptr = findendline(ptr, sdp_end-ptr);
    separator.s = ptr = end_ptr;
    while ((*ptr=='\n' || *ptr=='\r') && ptr<sdp_end)
        ptr++;
    separator.len = ptr - separator.s;
    if (separator.len > 2)
        separator.len = 2; // safety check

    return separator;
}


// will return the direction attribute defined in the given block.
// if missing, default is used if provided, else `sendrecv' is used.
static str
get_direction_attribute(str *block, str *default_direction)
{
    str direction, zone, line;
    char *ptr;

    for (zone=*block;;) {
        ptr = find_line_starting_with(&zone, "a=", False);
        if (!ptr) {
            if (default_direction)
                return *default_direction;
            direction.s = "sendrecv";
            direction.len = 8;
            return direction;
        }

        line.s = ptr + 2;
        line.len = findendline(line.s, zone.s + zone.len - line.s) - line.s;

        if (line.len==8) {
            if (strncasecmp(line.s, "sendrecv", 8)==0 || strncasecmp(line.s, "sendonly", 8)==0 ||
                strncasecmp(line.s, "recvonly", 8)==0 || strncasecmp(line.s, "inactive", 8)==0) {
                return line;
            }
        }

        zone.s   = line.s + line.len;
        zone.len = block->s + block->len - zone.s;
    }
}


// will return the rtcp port of the stream in the given block
// if defined by the stream, otherwise will return {NULL, 0}.
static str
get_rtcp_port_attribute(str *block)
{
    str zone, rtcp_port, undefined = {NULL, 0};
    char *ptr;
    int count;

    ptr = find_line_starting_with(block, "a=rtcp:", False);

    if (!ptr)
        return undefined;

    zone.s = ptr + 7;
    zone.len = findendline(zone.s, block->s + block->len - zone.s) - zone.s;

    count = get_str_tokens(&zone, &rtcp_port, 1);

    if (count != 1) {
        LM_ERR("invalid `a=rtcp' line in SDP body\n");
        return undefined;
    }

    return rtcp_port;
}


// will return the rtcp IP of the stream in the given block
// if defined by the stream, otherwise will return {NULL, 0}.
static str
get_rtcp_ip_attribute(str *block)
{
    str zone, tokens[4], undefined = {NULL, 0};
    char *ptr;
    int count;

    ptr = find_line_starting_with(block, "a=rtcp:", False);

    if (!ptr)
        return undefined;

    zone.s = ptr + 7;
    zone.len = findendline(zone.s, block->s + block->len - zone.s) - zone.s;

    count = get_str_tokens(&zone, tokens, 4);

    if (count != 4) {
        return undefined;
    }

    return tokens[3];
}


// will return true if the given block has both
// a=ice-pwd and a=ice-ufrag attributes.
static Bool
has_ice_attributes(str *block)
{
    char *ptr;
    ptr = find_line_starting_with(block, "a=ice-pwd:", False);
    if (ptr) {
        ptr = find_line_starting_with(block, "a=ice-ufrag:", False);
        if (ptr) {
            return True;
        }
    }
    return False;
}


// will return true if the given SDP has both
// a=ice-pwd and a=ice-ufrag attributes at the
// session level.
static Bool
has_session_ice_attributes(str *sdp)
{
    str block;
    char *ptr;

    // session level ICE attributes can be found from the beginning up to the first media block
    ptr = find_line_starting_with(sdp, "m=", False);
    if (ptr) {
        block.s   = sdp->s;
        block.len = ptr - block.s;
    } else {
        block = *sdp;
    }

    return has_ice_attributes(&block);
}


// will return true if the given block contains
// a a=candidate attribute. This should be called
// for a stream, as a=candidate attribute is not
// allowed at the session level
static Bool
has_ice_candidates(str *block)
{
    char *ptr;
    ptr = find_line_starting_with(block, "a=candidate:", False);
    if (ptr) {
        return True;
    }
    return False;
}


// will return true if given block contains an ICE 
// candidate with the given component ID
static Bool
has_ice_candidate_component(str *block, int id)
{
    char *ptr, *block_end;
    int i, components, count;
    str chunk, zone, tokens[2];

    block_end = block->s + block->len;
    components = count_lines_starting_with(block, "a=candidate:", False);
    for (i=0, chunk=*block; i<components; i++) {
        ptr = find_line_starting_with(&chunk, "a=candidate:", False);
        if (!ptr)
            break;

        zone.s = ptr + 12;
        zone.len = findendline(zone.s, block_end - zone.s) - zone.s;
        count = get_str_tokens(&zone, tokens, 2);

        if (count == 2) {
            if (strtoint(&tokens[1]) == id) {
                return True;
            }
        }
        
        chunk.s   = zone.s + zone.len;
        chunk.len = block_end - chunk.s;
    }
    return False;
}


// will return the priority (string value) that will be used
// for the candidate(s) inserted
static str
get_ice_candidate(void)
{
    int_str value;

    if (!search_first_avp(ice_candidate_avp.type | AVP_VAL_STR,
                          ice_candidate_avp.name, &value, NULL) || value.s.s==NULL || value.s.len==0) {
        // if AVP is not set use global module parameter
        return ice_candidate;
    } else {
        return value.s;
    }
}


// will return the priority (integer value) that will be used
// for the candidate(s) inserted
static unsigned int
get_ice_candidate_priority(str priority)
{
    int type_pref;

    if (STR_IMATCH(priority, "high-priority")) {
        // Use type preference even higher than host candidates
        type_pref = 130;
    } else if (STR_IMATCH(priority, "low-priority")) {
        type_pref = 0;
    } else {
        return NO_CANDIDATE;
    }
    // This will return the priority for the RTP component, the RTCP
    // component is RTP - 1
    return ((type_pref << 24) + 16777215);
}


// will return the ip address present in a `c=' line in the given block
// returns: -1 on error, 0 if not found, 1 if found
static int
get_media_ip_from_block(str *block, str *mediaip)
{
    str tokens[3], zone;
    char *ptr;
    int count;

    ptr = find_line_starting_with(block, "c=", False);

    if (!ptr) {
        mediaip->s   = NULL;
        mediaip->len = 0;
        return 0;
    }

    zone.s = ptr + 2;
    zone.len = findendline(zone.s, block->s + block->len - zone.s) - zone.s;

    count = get_str_tokens(&zone, tokens, 3);

    if (count != 3) {
        LM_ERR("invalid `c=' line in SDP body\n");
        return -1;
    }

    // can also check if tokens[1] == 'IP4'
    *mediaip = tokens[2];

    return 1;
}


static Bool
get_sdp_session_ip(str *sdp, str *mediaip, str *ip_line)
{
    char *ptr, *end_ptr;
    str block;

    // session IP can be found from the beginning up to the first media block
    ptr = find_line_starting_with(sdp, "m=", False);
    if (ptr) {
        block.s   = sdp->s;
        block.len = ptr - block.s;
    } else {
        block = *sdp;
    }

    if (get_media_ip_from_block(&block, mediaip) == -1) {
        LM_ERR("parse error while getting session-level media IP from SDP\n");
        return False;
    }

    if (ip_line != NULL) {
        ptr = find_line_starting_with(&block, "c=", False);
        if (!ptr) {
            ip_line->s = NULL;
            ip_line->len = 0;
        } else {
            end_ptr = findendline(ptr, block.s + block.len - ptr);
            while ((*end_ptr=='\n' || *end_ptr=='\r'))
                end_ptr++;
            ip_line->s = ptr;
            ip_line->len = end_ptr - ptr;
        }
    }

    // it's not an error to be missing. it can be locally defined
    // by each media stream. thus we return true even if not found
    return True;
}


// will return the direction as defined at the session level
// in the SDP. if missing, `sendrecv' is used.
static str
get_session_direction(str *sdp)
{
    static str default_direction = str_init("sendrecv");
    str block;
    char *ptr;

    // session level direction can be found from the beginning up to the first media block
    ptr = find_line_starting_with(sdp, "m=", False);
    if (ptr) {
        block.s   = sdp->s;
        block.len = ptr - block.s;
    } else {
        block = *sdp;
    }

    return get_direction_attribute(&block, &default_direction);
}


// will return the method ID for a reply by inspecting the Cseq header
static int
get_method_from_reply(struct sip_msg *reply)
{
    struct cseq_body *cseq;

    if (reply->first_line.type != SIP_REPLY)
        return -1;

    if (!reply->cseq && parse_headers(reply, HDR_CSEQ_F, 0) < 0) {
        LM_ERR("failed to parse the CSeq header\n");
        return -1;
    }
    if (!reply->cseq) {
        LM_ERR("missing CSeq header\n");
        return -1;
    }
    cseq = reply->cseq->parsed;
    return cseq->method_id;
}

static Bool
supported_transport(str transport)
{
    // supported transports: RTP/AVP, RTP/AVPF, RTP/SAVP, RTP/SAVPF, udp, udptl
    str prefixes[] = {str_init("RTP"), str_init("udp"), {NULL, 0}};
    int i;

    for (i=0; prefixes[i].s != NULL; i++) {
        if (STR_HAS_IPREFIX(transport, prefixes[i])) {
            return True;
        }
    }

    return False;
}


static int
get_session_info(str *sdp, SessionInfo *session)
{
    str tokens[3], ip, ip_line, block, zone;
    char *ptr, *sdp_end;
    int i, count, result;

    count = count_lines_starting_with(sdp, "v=", False);
    if (count != 1) {
        LM_ERR("cannot handle more than 1 media session in SDP\n");
        return -1;
    }

    count = count_lines_starting_with(sdp, "m=", False);
    if (count > MAX_STREAMS) {
        LM_ERR("cannot handle more than %d media streams in SDP\n", MAX_STREAMS);
        return -1;
    }

    memset(session, 0, sizeof(SessionInfo));

    if (count == 0)
        return 0;

    if (!get_sdp_session_ip(sdp, &ip, &ip_line)) {
        LM_ERR("failed to parse the SDP message\n");
        return -1;
    }

    ptr = memchr(ip.s, '/', ip.len);
    if (ptr) {
        LM_ERR("unsupported multicast IP specification in SDP: %.*s\n", ip.len, ip.s);
        return -1;
    }

    session->ip = ip;
    session->ip_line = ip_line;
    session->direction = get_session_direction(sdp);
    session->separator = get_sdp_line_separator(sdp);
    session->stream_count = count;

    sdp_end = sdp->s + sdp->len;

    for (i=0, block=*sdp; i<MAX_STREAMS; i++) {
        ptr = find_line_starting_with(&block, "m=", False);

        if (!ptr)
            break;

        zone.s = ptr + 2;
        zone.len = findendline(zone.s, sdp_end - zone.s) - zone.s;

        count = get_str_tokens(&zone, tokens, 3);
        if (count != 3) {
            LM_ERR("invalid `m=' line in the SDP body\n");
            return -1;
        }

        session->streams[i].start_line = ptr;
        session->streams[i].next_line = zone.s + zone.len + session->separator.len;
        if (session->streams[i].next_line > sdp_end)
            session->streams[i].next_line = sdp_end; //safety check

        if (supported_transport(tokens[2])) {
            // handle case where port is specified like <port>/<nr_of_ports>
            // as defined by RFC2327. ex: m=audio 5012/1 RTP/AVP 18 0 8
            // TODO: also handle case where nr_of_ports > 1  -Dan
            ptr = memchr(tokens[1].s, '/', tokens[1].len);
            if (ptr != NULL) {
                str port_nr;

                port_nr.s = ptr + 1;
                port_nr.len = tokens[1].s + tokens[1].len - port_nr.s;
                if (port_nr.len==0) {
                    LM_ERR("invalid port specification in `m=' line: %.*s\n", tokens[1].len, tokens[1].s);
                    return -1;
                }
                if (!(port_nr.len==1 && port_nr.s[0]=='1')) {
                    LM_ERR("unsupported number of ports specified in `m=' line\n");
                    return -1;
                }
                tokens[1].len = ptr - tokens[1].s;
            }

            session->streams[i].type = tokens[0];
            session->streams[i].port = tokens[1];

            session->streams[i].transport = TSupported;
            session->supported_count++;
        } else {
            // mark that we have an unsupported transport so we can ignore this stream later
            LM_INFO("unsupported transport in stream nr %d's `m=' line: %.*s\n", i+1, tokens[2].len, tokens[2].s);
            session->streams[i].type = tokens[0];
            session->streams[i].port = tokens[1];
            session->streams[i].transport = TUnsupported;
        }

        block.s   = zone.s + zone.len;
        block.len = sdp_end - block.s;
    }

    for (i=0; i<session->stream_count; i++) {
        block.s = session->streams[i].port.s;
        if (i < session->stream_count-1)
            block.len = session->streams[i+1].port.s - block.s;
        else
            block.len = sdp_end - block.s;

        result = get_media_ip_from_block(&block, &ip);
        if (result == -1) {
            LM_ERR("parse error while getting the contact IP for the "
                   "media stream number %d\n", i+1);
            return -1;
        } else if (result == 0) {
            if (session->ip.s == NULL) {
                LM_ERR("media stream number %d doesn't define a contact IP "
                       "and the session-level IP is missing\n", i+1);
                return -1;
            }
            session->streams[i].ip = session->ip;
            session->streams[i].local_ip = 0;
        } else {
            if (session->streams[i].transport == TSupported) {
                ptr = memchr(ip.s, '/', ip.len);
                if (ptr) {
                    LM_ERR("unsupported multicast IP specification in stream nr %d: %.*s\n", i+1, ip.len, ip.s);
                    return -1;
                }
            }
            session->streams[i].ip = ip;
            session->streams[i].local_ip = 1;
        }

        session->streams[i].rtcp_ip = get_rtcp_ip_attribute(&block);
        session->streams[i].rtcp_port = get_rtcp_port_attribute(&block);
        session->streams[i].direction = get_direction_attribute(&block, &session->direction);
        session->streams[i].has_ice = ((has_ice_attributes(&block) || has_session_ice_attributes(sdp)) && has_ice_candidates(&block));
        session->streams[i].has_rtcp_ice = has_ice_candidate_component(&block, 2);
        session->streams[i].first_ice_candidate = find_line_starting_with(&block, "a=candidate:", False);
    }

    return session->stream_count;
}


static Bool
insert_element(struct sip_msg *msg, char *position, char *element)
{
    struct lump *anchor;
    char *buf;
    int len;

    len = strlen(element);

    buf = pkg_malloc(len);
    if (!buf) {
        LM_ERR("out of memory\n");
        return False;
    }

    anchor = anchor_lump(msg, position - msg->buf, 0, 0);
    if (!anchor) {
        LM_ERR("failed to get anchor for new element\n");
        pkg_free(buf);
        return False;
    }

    memcpy(buf, element, len);

    if (insert_new_lump_after(anchor, buf, len, 0)==0) {
        LM_ERR("failed to insert new element\n");
        pkg_free(buf);
        return False;
    }

    return True;
}


static Bool
replace_element(struct sip_msg *msg, str *old_element, str *new_element)
{
    struct lump *anchor;
    char *buf;

    if (new_element->len==old_element->len &&
        memcmp(new_element->s, old_element->s, new_element->len)==0) {
        return True;
    }

    buf = pkg_malloc(new_element->len);
    if (!buf) {
        LM_ERR("out of memory\n");
        return False;
    }

    anchor = del_lump(msg, old_element->s - msg->buf, old_element->len, 0);
    if (!anchor) {
        LM_ERR("failed to delete old element\n");
        pkg_free(buf);
        return False;
    }

    memcpy(buf, new_element->s, new_element->len);

    if (insert_new_lump_after(anchor, buf, new_element->len, 0)==0) {
        LM_ERR("failed to insert new element\n");
        pkg_free(buf);
        return False;
    }

    return True;
}


static Bool
remove_element(struct sip_msg *msg, str *element)
{
    if (!del_lump(msg, element->s - msg->buf, element->len, 0)) {
        LM_ERR("failed to delete old element\n");
        return False;
    }

    return True;
}


// Functions dealing with the external mediaproxy helper
//

static Bool
mediaproxy_connect(void)
{
    struct sockaddr_un addr;

    if (mediaproxy_socket.sock >= 0)
        return True;

    if (mediaproxy_socket.last_failure + RETRY_INTERVAL > time(NULL))
        return False;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, mediaproxy_socket.name, sizeof(addr.sun_path) - 1);
#ifdef HAVE_SOCKADDR_SA_LEN
    addr.sun_len = strlen(addr.sun_path);
#endif

    mediaproxy_socket.sock = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (mediaproxy_socket.sock < 0) {
        LM_ERR("can't create socket\n");
        mediaproxy_socket.last_failure = time(NULL);
        return False;
    }
    if (connect(mediaproxy_socket.sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LM_ERR("failed to connect to %s: %s\n", mediaproxy_socket.name, strerror(errno));
        close(mediaproxy_socket.sock);
        mediaproxy_socket.sock = -1;
        mediaproxy_socket.last_failure = time(NULL);
        return False;
    }

    return True;
}

static void
mediaproxy_disconnect(void)
{
    if (mediaproxy_socket.sock < 0)
        return;

    close(mediaproxy_socket.sock);
    mediaproxy_socket.sock = -1;
    mediaproxy_socket.last_failure = time(NULL);
}

static char*
send_command(char *command)
{
    int cmd_len, bytes, tries, sent, received, count;
    struct timeval timeout;
    fd_set rset;

    if (!mediaproxy_connect())
        return NULL;

    cmd_len = strlen(command);

    for (sent=0, tries=0; sent<cmd_len && tries<3; tries++, sent+=bytes) {
        do
            bytes = send(mediaproxy_socket.sock, command+sent, cmd_len-sent, MSG_DONTWAIT|MSG_NOSIGNAL);
        while (bytes == -1 && errno == EINTR);
        if (bytes == -1) {
            switch (errno) {
            case ECONNRESET:
            case EPIPE:
                mediaproxy_disconnect();
                mediaproxy_socket.last_failure = 0; // we want to reconnect immediately
                if (mediaproxy_connect()) {
                    sent = bytes = 0;
                    continue;
                } else {
                    LM_ERR("connection with mediaproxy did die\n");
                }
                break;
            case EACCES:
                LM_ERR("got permission denied while sending to %s\n", mediaproxy_socket.name);
                break;
            case EWOULDBLOCK:
                // this shouldn't happen as we read back all the answer after a request.
                // if it would block, it means there is an error.
                LM_ERR("sending command would block!\n");
                break;
            default:
                LM_ERR("%d: %s\n", errno, strerror(errno));
                break;
            }
            mediaproxy_disconnect();
            return NULL;
        }
    }
    if (sent < cmd_len) {
        LM_ERR("couldn't send complete command after 3 tries\n");
        mediaproxy_disconnect();
        return NULL;
    }

    mediaproxy_socket.data[0] = 0;
    received = 0;
    while (True) {
        FD_ZERO(&rset);
        FD_SET(mediaproxy_socket.sock, &rset);
        timeout.tv_sec = mediaproxy_socket.timeout / 1000;
        timeout.tv_usec = (mediaproxy_socket.timeout % 1000) * 1000;

        do
            count = select(mediaproxy_socket.sock + 1, &rset, NULL, NULL, &timeout);
        while (count == -1 && errno == EINTR);

        if (count == -1) {
            LM_ERR("select failed: %d: %s\n", errno, strerror(errno));
            mediaproxy_disconnect();
            return NULL;
        } else if (count == 0) {
            LM_ERR("did timeout waiting for an answer\n");
            mediaproxy_disconnect();
            return NULL;
        } else {
            do
                bytes = recv(mediaproxy_socket.sock, mediaproxy_socket.data+received, BUFFER_SIZE-1-received, 0);
            while (bytes == -1 && errno == EINTR);
            if (bytes == -1) {
                LM_ERR("failed to read answer: %d: %s\n", errno, strerror(errno));
                mediaproxy_disconnect();
                return NULL;
            } else if (bytes == 0) {
                LM_ERR("connection with mediaproxy closed\n");
                mediaproxy_disconnect();
                return NULL;
            } else {
                mediaproxy_socket.data[received+bytes] = 0;
                if (strstr(mediaproxy_socket.data+received, "\r\n")!=NULL) {
                    break;
                }
                received += bytes;
            }
        }
    }

    return mediaproxy_socket.data;
}


// Exported API implementation
//

// ice_candidate_data: it carries data across the dialog when using engage_media_proxy:
//   - priority: the priority that should be used for the ICE candidate
//      * -1: no candidate should be added.
//      * other: the specified type preference should be used for calculating 
//   - skip_next_reply: flag for knowing the fact that the next reply with SDP must be skipped
//     because it is a reply to a re-INVITE or UPDATE *after* the ICE negotiation
static int
use_media_proxy(struct sip_msg *msg, char *dialog_id, ice_candidate_data *ice_data)
{
    str callid, cseq, from_uri, to_uri, from_tag, to_tag, user_agent;
    str signaling_ip, media_relay, sdp, str_buf, tokens[MAX_STREAMS+1];
    str priority_str, candidate;
    char request[8192], media_str[4096], buf[128], *result, *type;
    int i, j, port, len, status;
    Bool removed_session_ip, have_sdp;
    SessionInfo session;
    StreamInfo stream;
    unsigned int priority;

    if (msg == NULL)
        return -1;

    if (msg->first_line.type == SIP_REQUEST) {
        type = "request";
    } else if (msg->first_line.type == SIP_REPLY) {
        if (ice_data != NULL && ice_data->skip_next_reply) {
            // we don't process replies to ICE negotiation end requests 
            // (those containing a=remote-candidates)
            ice_data->skip_next_reply = False;
            return -1;
        }
        type = "reply";
    } else {
        return -1;
    }

    if (!get_callid(msg, &callid)) {
        LM_ERR("failed to get Call-ID\n");
        return -1;
    }

    if (!get_cseq_number(msg, &cseq)) {
        LM_ERR("failed to get CSeq\n");
        return -1;
    }

    status = get_sdp_message(msg, &sdp);
    // status = -1 is error, -2 is missing SDP body
    if (status == -1 || (status == -2 && msg->first_line.type == SIP_REQUEST)) {
        return status;
    } else if (status == -2 && !(msg->REPLY_STATUS == 200 && get_method_from_reply(msg) == METHOD_INVITE)) {
        return -2;
    }
    have_sdp = (status == 1);

    if (have_sdp) {
        if (msg->first_line.type == SIP_REQUEST && find_line_starting_with(&sdp, "a=remote-candidates", False)) {
            // we don't process requests with a=remote-candidates, this indicates the end of an ICE
            // negotiation and we must not mangle the SDP.
            if (ice_data != NULL) {
                ice_data->skip_next_reply = True;
            }
            return -1;
        }
       
        status = get_session_info(&sdp, &session);
        if (status < 0) {
            LM_ERR("can't extract media streams from the SDP message\n");
            return -1;
        }

        if (session.supported_count == 0)
            return 1; // there are no supported media streams. we have nothing to do.

        len = sprintf(media_str, "%s", "media: ");
        for (i=0, str_buf.len=sizeof(media_str)-len-2, str_buf.s=media_str+len; i<session.stream_count; i++) {
            stream = session.streams[i];
            if (stream.transport != TSupported)
                continue; // skip streams with unsupported transports
            if (stream.type.len + stream.ip.len + stream.port.len + stream.direction.len + 4 > str_buf.len) {
                LM_ERR("media stream description is longer than %lu bytes\n", (unsigned long)sizeof(media_str));
                return -1;
            }
            len = sprintf(str_buf.s, "%.*s:%.*s:%.*s:%.*s:%s,",
                          stream.type.len, stream.type.s,
                          stream.ip.len, stream.ip.s,
                          stream.port.len, stream.port.s,
                          stream.direction.len, stream.direction.s,
                          stream.has_ice?"ice=yes":"ice=no");
            str_buf.s   += len;
            str_buf.len -= len;
        }
        *(str_buf.s-1) = 0; // remove the last comma
        sprintf(str_buf.s-1, "%s", "\r\n");
    } else {
        media_str[0] = 0;
    }

    from_uri     = get_from_uri(msg);
    to_uri       = get_to_uri(msg);
    from_tag     = get_from_tag(msg);
    to_tag       = get_to_tag(msg);
    user_agent   = get_user_agent(msg);
    signaling_ip = get_signaling_ip(msg);
    media_relay  = get_media_relay(msg);

    len = snprintf(request, sizeof(request),
                   "update\r\n"
                   "type: %s\r\n"
                   "dialog_id: %s\r\n"
                   "call_id: %.*s\r\n"
                   "cseq: %.*s\r\n"
                   "from_uri: %.*s\r\n"
                   "to_uri: %.*s\r\n"
                   "from_tag: %.*s\r\n"
                   "to_tag: %.*s\r\n"
                   "user_agent: %.*s\r\n"
                   "signaling_ip: %.*s\r\n"
                   "media_relay: %.*s\r\n"
                   "%s"
                   "\r\n",
                   type, dialog_id, callid.len, callid.s, cseq.len, cseq.s,
                   from_uri.len, from_uri.s, to_uri.len, to_uri.s,
                   from_tag.len, from_tag.s, to_tag.len, to_tag.s,
                   user_agent.len, user_agent.s,
                   signaling_ip.len, signaling_ip.s,
                   media_relay.len, media_relay.s, media_str);

    if (len >= sizeof(request)) {
        LM_ERR("mediaproxy request is longer than %lu bytes\n", (unsigned long)sizeof(request));
        return -1;
    }

    result = send_command(request);

    if (result == NULL)
        return -1;

    if (!have_sdp) {
        // we updated the dispatcher, we can't do anything else as
        // there is no SDP
        return 1;
    }

    len = get_tokens(result, tokens, sizeof(tokens)/sizeof(str));

    if (len == 0) {
        LM_ERR("empty response from mediaproxy\n");
        return -1;
    } else if (len==1 && STR_MATCH(tokens[0], "error")) {
        LM_ERR("mediaproxy returned error\n");
        return -1;
    } else if (len<session.supported_count+1) {
        if (msg->first_line.type == SIP_REQUEST) {
            LM_ERR("insufficient ports returned from mediaproxy: got %d, "
                   "expected %d\n", len-1, session.supported_count);
            return -1;
        } else {
            LM_WARN("broken client. Called UA added extra media stream(s) "
                    "in the OK reply\n");
        }
    }

    removed_session_ip = False;

    // only replace the session ip if there are no streams with unsupported
    // transports otherwise we insert an ip line in the supported streams
    // and remove the session level ip
    if (session.ip.s && !isnulladdr(session.ip)) {
        if (session.stream_count == session.supported_count) {
            if (!replace_element(msg, &session.ip, &tokens[0])) {
                LM_ERR("failed to replace session-level media IP in the SDP body\n");
                return -1;
            }
        } else {
            if (!remove_element(msg, &session.ip_line)) {
                LM_ERR("failed to remove session-level media IP in the SDP body\n");
                return -1;
            }
            removed_session_ip = True;
        }
    }

    for (i=0, j=1; i<session.stream_count; i++) {
        stream = session.streams[i];
        if (stream.transport != TSupported) {
            if (!stream.local_ip && removed_session_ip) {
                strcpy(buf, "c=IN IP4 ");
                strncat(buf, session.ip.s, session.ip.len);
                strncat(buf, session.separator.s, session.separator.len);
                if (!insert_element(msg, stream.next_line, buf)) {
                    LM_ERR("failed to insert IP address in media stream number %d\n", i+1);
                    return -1;
                }
            }
            continue;
        }

        if (j >= len) {
            break;
        }
        
        if (!isnullport(stream.port)) {
            if (!replace_element(msg, &stream.port, &tokens[j])) {
                LM_ERR("failed to replace port in media stream number %d\n", i+1);
                return -1;
            }
        }

        if (stream.rtcp_port.len>0 && !isnullport(stream.rtcp_port)) {
            str rtcp_port;

            port = strtoint(&tokens[j]);
            rtcp_port.s = int2str(port+1, &rtcp_port.len);
            if (!replace_element(msg, &stream.rtcp_port, &rtcp_port)) {
                LM_ERR("failed to replace RTCP port in media stream number %d\n", i+1);
                return -1;
            }
        }

        if (stream.rtcp_ip.len > 0) {
            if (!replace_element(msg, &stream.rtcp_ip, &tokens[0])) {
                LM_ERR("failed to replace RTCP IP in media stream number %d\n", i+1);
                return -1;
            }
        }

        if (stream.local_ip && !isnulladdr(stream.ip)) {
            if (!replace_element(msg, &stream.ip, &tokens[0])) {
                LM_ERR("failed to replace IP address in media stream number %d\n", i+1);
                return -1;
            }
        } else if (!stream.local_ip && removed_session_ip) {
            strcpy(buf, "c=IN IP4 ");
            strncat(buf, tokens[0].s, tokens[0].len);
            strncat(buf, session.separator.s, session.separator.len);
            if (!insert_element(msg, stream.next_line, buf)) {
                LM_ERR("failed to insert IP address in media stream number %d\n", i+1);
                return -1;
            }
        }

        if (ice_data == NULL) {
            priority_str = get_ice_candidate();
        } else if (ice_data->priority == NO_CANDIDATE) {
            priority_str.s = "none";
        } else {
            // we don't need the string value, we'll use the number
            priority_str.s = "";
        }
        priority_str.len = strlen(priority_str.s);

        if (stream.has_ice && stream.first_ice_candidate && !STR_IMATCH(priority_str, "none")) {
            // add some pseudo-random string to the foundation
            struct in_addr hexip;
            inet_aton(tokens[0].s, &hexip);

            priority = (ice_data == NULL)?get_ice_candidate_priority(priority_str):ice_data->priority;
            port = strtoint(&tokens[j]);
            candidate.s = buf;
            candidate.len = sprintf(candidate.s, "a=candidate:R%x 1 UDP %u %.*s %i typ relay%.*s",
                                    hexip.s_addr,
                                    priority,
                                    tokens[0].len, tokens[0].s, 
                                    port,
                                    session.separator.len, session.separator.s);

            if (!insert_element(msg, stream.first_ice_candidate, candidate.s)) {
                LM_ERR("failed to insert ICE candidate in media stream number %d\n", i+1);
                return -1;
            }

            if (stream.has_rtcp_ice) {
                candidate.s = buf;
                candidate.len = sprintf(candidate.s, "a=candidate:R%x 2 UDP %u %.*s %i typ relay%.*s",
                                        hexip.s_addr,
                                        priority-1,
                                        tokens[0].len, tokens[0].s, 
                                        port+1,
                                        session.separator.len, session.separator.s);

                if (!insert_element(msg, stream.first_ice_candidate, candidate.s)) {
                    LM_ERR("failed to insert ICE candidate in media stream number %d\n", i+1);
                    return -1;
                }
            }
        }

        j++;
    }

    return 1;
}


static int
end_media_session(str callid, str from_tag, str to_tag)
{
    char request[2048], *result;
    int len;

    len = snprintf(request, sizeof(request),
                   "remove\r\n"
                   "call_id: %.*s\r\n"
                   "from_tag: %.*s\r\n"
                   "to_tag: %.*s\r\n"
                   "\r\n",
                   callid.len, callid.s,
                   from_tag.len, from_tag.s,
                   to_tag.len, to_tag.s);

    if (len >= sizeof(request)) {
        LM_ERR("mediaproxy request is longer than %lu bytes\n", (unsigned long)sizeof(request));
        return -1;
    }

    result = send_command(request);

    return result==NULL ? -1 : 1;
}


// Dialog callbacks and helpers
//

typedef enum {
    MPInactive = 0,
    MPActive
} MediaProxyState;


static INLINE char*
get_dialog_id(struct dlg_cell *dlg)
{
    static char buffer[64];

    snprintf(buffer, sizeof(buffer), "%d:%d", dlg->h_entry, dlg->h_id);

    return buffer;
}


static void
__free_dialog_data(void *data)
{
    shm_free((ice_candidate_data*)data);
}


static void
__dialog_requests(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    use_media_proxy(_params->req, get_dialog_id(dlg), (ice_candidate_data*)*_params->param);
}


static void
__dialog_replies(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    struct sip_msg *reply = _params->rpl;

    if (reply == FAKED_REPLY)
        return;

    if (reply->REPLY_STATUS>100 && reply->REPLY_STATUS<300) {
        use_media_proxy(reply, get_dialog_id(dlg), (ice_candidate_data*)*_params->param);
    }
}


static void
__dialog_ended(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    if ((int)(long)*_params->param == MPActive) {
        end_media_session(dlg->callid, dlg->tag[DLG_CALLER_LEG], dlg->tag[DLG_CALLEE_LEG]);
        *_params->param = MPInactive;
    }
}


static void
__dialog_created(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    struct sip_msg *request = _params->req;
    ice_candidate_data *ice_data;

    if (request->REQ_METHOD != METHOD_INVITE)
        return;

    if ((request->msg_flags & FL_USE_MEDIA_PROXY) == 0)
        return;

    ice_data = (ice_candidate_data*)shm_malloc(sizeof(ice_candidate_data));
    if (!ice_data) {
        LM_ERR("failed to allocate shm memory for ice_candidate_data\n");
        return;
    }

    ice_data->priority = get_ice_candidate_priority(get_ice_candidate());
    ice_data->skip_next_reply = False;

    if (dlg_api.register_dlgcb(dlg, DLGCB_REQ_WITHIN | DLGCB_CONFIRMED, __dialog_requests, (void*)ice_data, __free_dialog_data) != 0)
        LM_ERR("cannot register callback for in-dialog requests\n");
    if (dlg_api.register_dlgcb(dlg, DLGCB_RESPONSE_FWDED | DLGCB_RESPONSE_WITHIN, __dialog_replies, (void*)ice_data, NULL) != 0)
        LM_ERR("cannot register callback for dialog and in-dialog replies\n");
    if (dlg_api.register_dlgcb(dlg, DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_EXPIRED | DLGCB_DESTROY, __dialog_ended, (void*)MPActive, NULL) != 0)
        LM_ERR("cannot register callback for dialog termination\n");

    use_media_proxy(request, get_dialog_id(dlg), ice_data);
}


//
// The public functions that are exported by this module
//


static int
EngageMediaProxy(struct sip_msg *msg)
{
    if (mediaproxy_disabled)
        return -1;

    if (!have_dlg_api) {
        LM_ERR("engage_media_proxy requires the dialog module to be loaded and configured\n");
        return -1;
    }
    msg->msg_flags |= FL_USE_MEDIA_PROXY;
    setflag(msg, dialog_flag); // have the dialog module trace this dialog
    return 1;
}


static int
UseMediaProxy(struct sip_msg *msg)
{
    if (mediaproxy_disabled)
        return -1;

    return use_media_proxy(msg, "", NULL);
}


static int
EndMediaSession(struct sip_msg *msg)
{
    str callid, from_tag, to_tag;

    if (mediaproxy_disabled)
        return -1;

    if (!get_callid(msg, &callid)) {
        LM_ERR("failed to get Call-ID\n");
        return -1;
    }

    from_tag = get_from_tag(msg);
    to_tag   = get_to_tag(msg);

    return end_media_session(callid, from_tag, to_tag);
}


//
// Module management: initialization/destroy/function-parameter-fixing/...
//


static int
mod_init(void)
{
    pv_spec_t avp_spec;
    int *param;
    modparam_t type;

    // initialize the signaling_ip_avp structure
    if (!signaling_ip_avp.spec.s || signaling_ip_avp.spec.len<=0) {
        LM_WARN("missing/empty signaling_ip_avp parameter. will use default.\n");
        signaling_ip_avp.spec.s = SIGNALING_IP_AVP_SPEC;
        signaling_ip_avp.spec.len = strlen(signaling_ip_avp.spec.s);
    }

    if (pv_parse_spec(&(signaling_ip_avp.spec), &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
        LM_CRIT("invalid AVP specification for signaling_ip_avp: `%s'\n", signaling_ip_avp.spec.s);
        return -1;
    }
    if (pv_get_avp_name(0, &(avp_spec.pvp), &(signaling_ip_avp.name), &(signaling_ip_avp.type))!=0) {
        LM_CRIT("invalid AVP specification for signaling_ip_avp: `%s'\n", signaling_ip_avp.spec.s);
        return -1;
    }

    // initialize the media_relay_avp structure
    if (!media_relay_avp.spec.s || media_relay_avp.spec.len<=0) {
        LM_WARN("missing/empty media_relay_avp parameter. will use default.\n");
        media_relay_avp.spec.s = MEDIA_RELAY_AVP_SPEC;
        media_relay_avp.spec.len = strlen(media_relay_avp.spec.s);
    }

    if (pv_parse_spec(&(media_relay_avp.spec), &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
        LM_CRIT("invalid AVP specification for media_relay_avp: `%s'\n", media_relay_avp.spec.s);
        return -1;
    }
    if (pv_get_avp_name(0, &(avp_spec.pvp), &(media_relay_avp.name), &(media_relay_avp.type))!=0) {
        LM_CRIT("invalid AVP specification for media_relay_avp: `%s'\n", media_relay_avp.spec.s);
        return -1;
    }

    // initialize the ice_candidate_avp structure
    if (!ice_candidate_avp.spec.s || ice_candidate_avp.spec.len<=0) {
        LM_WARN("missing/empty ice_candidate_avp parameter. will use default.\n");
        ice_candidate_avp.spec.s = ICE_CANDIDATE_AVP_SPEC;
        ice_candidate_avp.spec.len = strlen(ice_candidate_avp.spec.s);
    }

    if (pv_parse_spec(&(ice_candidate_avp.spec), &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
        LM_CRIT("invalid AVP specification for ice_candidate_avp: `%s'\n", ice_candidate_avp.spec.s);
        return -1;
    }
    if (pv_get_avp_name(0, &(avp_spec.pvp), &(ice_candidate_avp.name), &(ice_candidate_avp.type))!=0) {
        LM_CRIT("invalid AVP specification for ice_candidate_avp: `%s'\n", ice_candidate_avp.spec.s);
        return -1;
    }

    // initialize ice_candidate module parameter
    if (!STR_IMATCH(ice_candidate, "none") && !STR_IMATCH(ice_candidate, "low-priority") && !STR_IMATCH(ice_candidate, "high-priority")) {
        LM_CRIT("invalid value specified for ice_candidate: `%s'\n", ice_candidate.s);
        return -1;
    }

    // bind to the dialog API
    if (load_dlg_api(&dlg_api)==0) {
        have_dlg_api = True;

        // load dlg_flag and default_timeout parameters from the dialog module
        param = find_param_export(find_module_by_name("dialog"), "dlg_flag", INT_PARAM, &type);
        if (!param) {
            LM_CRIT("cannot find dlg_flag parameter in the dialog module\n");
            return -1;
        }

	if (type != INT_PARAM) {
	    LM_CRIT("dlg_flag parameter found but with wrong type: %d\n", type);
	    return -1;
	}

        dialog_flag = *param;

        // register dialog creation callback
        if (dlg_api.register_dlgcb(NULL, DLGCB_CREATED, __dialog_created, NULL, NULL) != 0) {
            LM_CRIT("cannot register callback for dialog creation\n");
            return -1;
        }
    } else {
        LM_NOTICE("engage_media_proxy() will not work because the dialog module is not loaded\n");
    }

    return 0;
}


static int
child_init(int rank)
{
    // initialize the connection to mediaproxy if needed
    if (!mediaproxy_disabled && rank > PROC_MAIN)
        mediaproxy_connect();

    return 0;
}


