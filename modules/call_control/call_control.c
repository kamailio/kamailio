/* $Id$
 *
 * Copyright (C) 2005-2008 Dan Pascu
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../pvar.h"
#include "../../ut.h"
#include "../../script_cb.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_from.h"
#include "../dialog/dlg_load.h"
#include "../dialog/dlg_hash.h"


MODULE_VERSION

#define FL_USE_CALL_CONTROL       (1<<28) // use call control for a dialog

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
# define INLINE inline
#else
# define INLINE
#endif

#define CANONICAL_URI_AVP_SPEC "$avp(s:can_uri)"
#define SIGNALING_IP_AVP_SPEC  "$avp(s:signaling_ip)"
#define SIP_APPLICATION_AVP_SPEC "$avp(s:sip_application)"

// Although `AF_LOCAL' is mandated by POSIX.1g, `AF_UNIX' is portable to
// more systems.  `AF_UNIX' was the traditional name stemming from BSD, so
// even most POSIX systems support it.  It is also the name of choice in
// the Unix98 specification. So if there's no AF_LOCAL fallback to AF_UNIX
#ifndef AF_LOCAL
# define AF_LOCAL AF_UNIX
#endif

// Solaris does not have the MSG_NOSIGNAL flag for the send(2) syscall
#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif



typedef int Bool;
#define True  1
#define False 0


typedef struct AVP_Param {
    str spec;
    int_str name;
    unsigned short type;
} AVP_Param;

typedef struct AVP_List {
    pv_spec_p pv;
    str name;
    struct AVP_List *next;
} AVP_List;

#define RETRY_INTERVAL 10
#define BUFFER_SIZE    8192

typedef struct CallControlSocket {
    char *name;             // name
    int  sock;              // socket
    int  timeout;           // how many miliseconds to wait for an answer
    time_t last_failure;    // time of the last failure
    char data[BUFFER_SIZE]; // buffer for the answer data
} CallControlSocket;


/* Function prototypes */
static int CallControl(struct sip_msg *msg, char *str1, char *str2);

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);
static int postprocess_request(struct sip_msg *, unsigned int, void *);

int parse_param_init(unsigned int type, void *val);
int parse_param_start(unsigned int type, void *val);
int parse_param_stop(unsigned int type, void *val);

/* Local global variables */
static CallControlSocket callcontrol_socket = {
    "/var/run/callcontrol/socket", // name
    -1,                            // sock
    500,                           // timeout in 500 miliseconds if there is no answer
    0,                             // time of the last failure
    ""                             // data
};

static int disable = False;
static int diverter_avp_id = 805;

/* The AVP where the canonical URI is stored (if defined) */
static AVP_Param canonical_uri_avp = {str_init(CANONICAL_URI_AVP_SPEC), {0}, 0};

/* The AVP where the caller signaling IP is stored (if defined) */
static AVP_Param signaling_ip_avp = {str_init(SIGNALING_IP_AVP_SPEC), {0}, 0};

/* The AVP where the SIP application type is stored (if defined) */
static AVP_Param sip_application_avp = {str_init(SIP_APPLICATION_AVP_SPEC), {0}, 0};


struct dlg_binds dlg_api;
static int dialog_flag = -1;

AVP_List *cc_init_avps = NULL, *cc_start_avps = NULL, *cc_stop_avps = NULL;

pv_elem_t *model;

static cmd_export_t commands[] = {
    {"call_control",  (cmd_function)CallControl, 0, 0, 0, REQUEST_ROUTE },
    {0, 0, 0, 0, 0, 0}
};

static param_export_t parameters[] = {
    {"init",                PARAM_STRING | USE_FUNC_PARAM, parse_param_init},
    {"start",               PARAM_STRING | USE_FUNC_PARAM, parse_param_start},
    {"stop",                PARAM_STRING | USE_FUNC_PARAM, parse_param_stop},
    {"disable",             INT_PARAM, &disable},
    {"socket_name",         PARAM_STRING, &(callcontrol_socket.name)},
    {"socket_timeout",      INT_PARAM, &(callcontrol_socket.timeout)},
    {"diverter_avp_id",     INT_PARAM, &diverter_avp_id},
    {"canonical_uri_avp",   PARAM_STR, &(canonical_uri_avp.spec)},
    {"signaling_ip_avp",    PARAM_STR, &(signaling_ip_avp.spec)},
    {"sip_application_avp", PARAM_STR, &(sip_application_avp.spec)},
    {0, 0, 0}
};

struct module_exports exports = {
    "call_control",  // module name
    DEFAULT_DLFLAGS, // dlopen flags
    commands,        // exported functions
    parameters,      // exported parameters
    NULL,            // exported statistics
    NULL,            // exported MI functions
    NULL,            // exported pseudo-variables
    NULL,            // extra processes
    mod_init,        // module init function (before fork. kids will inherit)
    NULL,            // reply processing function
    destroy,         // destroy function
    child_init       // child init function
};



typedef enum CallControlAction {
    CAInitialize = 1,
    CAStart,
    CAStop
} CallControlAction;


typedef struct Contact {
    str username;
    str ip;
    str port;
} Contact;

typedef struct DialogID {
    unsigned int h_entry;
    unsigned int h_id;
} DialogID;

typedef struct CallInfo {
    CallControlAction action;
    DialogID dialog_id;
    str ruri;
    str diverter;
    str source_ip;
    str callid;
    str from;
    str from_tag;
    str sip_application;
} CallInfo;



#define CHECK_COND(cond) \
    if ((cond) == 0) { \
        LM_ERR("malformed modparam\n"); \
        return -1;                            \
    }

#define CHECK_ALLOC(p) \
    if (!(p)) {    \
        LM_ERR("no memory left\n"); \
        return -1;    \
    }


void
destroy_list(AVP_List *list) {
	AVP_List *cur, *next;

	cur = list;
	while (cur) {
		next = cur->next;
		pkg_free(cur);
		cur = next;
	}
}


int
parse_param(void *val, AVP_List** avps) {

    char *p;
    str s, content;
    AVP_List *mp = NULL;

    //LM_DBG("%.*s\n", content.len, content.s);

    content.s = (char*) val;
    content.len = strlen(content.s);


    p = (char*) pkg_malloc (content.len + 1);
    CHECK_ALLOC(p);

    p[content.len] = '\0';
    memcpy(p, content.s, content.len);


    for (;*p != '\0';) {

        mp = (AVP_List*) pkg_malloc (sizeof(AVP_List));
        CHECK_ALLOC(mp);
        mp->next = *avps;
        mp->pv = (pv_spec_p) pkg_malloc (sizeof(pv_spec_t));
        CHECK_ALLOC(mp->pv);

        for (; isspace(*p); p++);
        CHECK_COND(*p != '\0');

        mp->name.s = p;

        for(; isgraph(*p) && *p != '='; p++)
            CHECK_COND(*p != '\0');

        mp->name.len = p - mp->name.s;

        for (; isspace(*p); p++);
        CHECK_COND(*p != '\0' && *p == '=');
        p++;

        //LM_DBG("%.*s\n", mp->name.len, mp->name.s);

        for (; isspace(*p); p++);
        CHECK_COND(*p != '\0' && *p == '$');

        s.s = p;
        s.len = strlen(p);

        p = pv_parse_spec(&s, mp->pv);

        for (; isspace(*p); p++);
        *avps = mp;
    }

    return 0;
}


int
parse_param_init(unsigned int type, void *val) {
    if (parse_param(val, &cc_init_avps) == -1)
        return E_CFG;
    return 0;
}

int
parse_param_start(unsigned int type, void *val) {
    if (parse_param(val, &cc_start_avps) == -1)
        return E_CFG;
    return 0;
}

int
parse_param_stop(unsigned int type, void *val) {
    if (parse_param(val, &cc_stop_avps) == -1)
        return E_CFG;
    return 0;
}


// Functions dealing with strings
//

// returns string with whitespace trimmed from left end
static inline void
ltrim(str *string)
{
    while (string->len>0 && isspace((int)*(string->s))) {
        string->len--;
        string->s++;
    }
}

// returns string with whitespace trimmed from right end
static inline void
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
static inline void
trim(str *string)
{
    ltrim(string);
    rtrim(string);
}


// Message checking and parsing
//

static Bool
has_to_tag(struct sip_msg *msg)
{
    str tag;

    if (!msg->to) {
        if (parse_headers(msg, HDR_TO_F, 0)==-1) {
            LOG(L_ERR, "cannot parse 'To' header\n");
            return False;
        }
        if (!msg->to) {
            LOG(L_ERR, "missing 'To' header\n");
            return False;
        }
    }

    tag = get_to(msg)->tag_value;

    if (tag.s==NULL || tag.len==0) {
        return False;
    }

    return True;
}


// Get canonical request URI
static str
get_canonical_request_uri(struct sip_msg* msg)
{
    int_str value;

    if (!search_first_avp(canonical_uri_avp.type | AVP_VAL_STR,
                          canonical_uri_avp.name, &value, NULL) ||
        value.s.s==NULL || value.s.len==0) {

        return *GET_RURI(msg);
    }

    return value.s;
}


// Get caller signaling IP
static str
get_signaling_ip(struct sip_msg* msg)
{
    int_str value;

    if (!search_first_avp(signaling_ip_avp.type | AVP_VAL_STR,
                          signaling_ip_avp.name, &value, NULL) ||
        !value.s.s || value.s.len==0) {

        value.s.s = ip_addr2a(&msg->rcv.src_ip);
        value.s.len = strlen(value.s.s);
    }

    return value.s;
}

// Get SIP application type
static str
get_sip_application(struct sip_msg* msg)
{
    int_str value;

    if (!search_first_avp(sip_application_avp.type | AVP_VAL_STR,
                          sip_application_avp.name, &value, NULL) ||
        !value.s.s || value.s.len==0) {

        value.s.s = "audio";
        value.s.len = strlen(value.s.s);
    }

    return value.s;
}

static str
get_diverter(struct sip_msg *msg)
{
    struct hdr_field *header;
    dig_cred_t *credentials;
    int_str avpname, avpvalue;
    static str diverter;

    diverter.s   = "None";
    diverter.len = 4;

    avpname.n = diverter_avp_id;

    if (search_first_avp(AVP_VAL_STR, avpname, &avpvalue, NULL)) {
        // have a diverted call
        diverter = avpvalue.s;
    } else {
        get_authorized_cred(msg->proxy_auth, &header);
        if (header) {
            credentials = &((auth_body_t*)(header->parsed))->digest;
        } else {
            if (parse_headers(msg, HDR_PROXYAUTH_F, 0) == -1) {
                LOG(L_ERR, "cannot parse Proxy-Authorization header\n");
                return diverter;
            }
            if (!msg->proxy_auth)
                return diverter;
            if (parse_credentials(msg->proxy_auth) != 0) {
                LOG(L_ERR, "cannot parse credentials\n");
                return diverter;
            }
            credentials = &((auth_body_t*)(msg->proxy_auth->parsed))->digest;
        }

        if (credentials->username.user.len > 0 &&
            credentials->username.domain.len > 0 &&
            credentials->realm.len == 0 &&
            credentials->nonce.len == 0 &&
            credentials->response.len == 0) {
            // this is a call diverted from the failure route
            // and sent back to proxy with append_pa_hf()
            diverter = credentials->username.whole;
        }
    }

    return diverter;
}


static CallInfo*
get_call_info(struct sip_msg *msg, CallControlAction action)
{
    static CallInfo call_info;
    hdr_flags_t headers;

    memset(&call_info, 0, sizeof(struct CallInfo));

    switch (action) {
    case CAInitialize:
        headers = HDR_CALLID_F|HDR_FROM_F;
        break;
    case CAStart:
    case CAStop:
        headers = HDR_CALLID_F;
        break;
    default:
        // Invalid action. Should never get here.
        assert(False);
        return NULL;
    }

    if (parse_headers(msg, headers, 0) == -1) {
        LOG(L_ERR, "cannot parse required headers\n");
        return NULL;
    }

    if (headers & HDR_CALLID_F) {
        if (msg->callid == NULL) {
            LOG(L_ERR, "missing Call-ID header\n");
            return NULL;
        }

        call_info.callid = msg->callid->body;
        trim(&call_info.callid);
    }

    if (headers & HDR_FROM_F) {
        struct to_body *from; // yeah. suggestive structure name ;)

        if (msg->from == NULL) {
            LOG(L_ERR, "missing From header\n");
            return NULL;
        }
        if (!msg->from->parsed && parse_from_header(msg)==-1) {
            LOG(L_ERR, "cannot parse From header\n");
            return NULL;
        }

        from = get_from(msg);

        if (from->body.s==NULL || from->body.len==0) {
            LOG(L_ERR, "missing From\n");
            return NULL;
        }
        if (from->tag_value.s==NULL || from->tag_value.len==0) {
            LOG(L_ERR, "missing From tag\n");
            return NULL;
        }

        call_info.from = from->body;
        call_info.from_tag = from->tag_value;
    }

    if (action == CAInitialize) {
        call_info.ruri = get_canonical_request_uri(msg);
        call_info.diverter = get_diverter(msg);
        call_info.source_ip = get_signaling_ip(msg);
        call_info.sip_application = get_sip_application(msg);
    }

    call_info.action = action;

    return &call_info;
}

static char*
make_custom_request(struct sip_msg *msg, CallInfo *call)
{
    static char request[8192];
    int len = 0;
    AVP_List *al;
    pv_value_t pt;

    switch (call->action) {
    case CAInitialize:
        al = cc_init_avps;
        break;
    case CAStart:
        al = cc_start_avps;
        break;
    case CAStop:
        al = cc_stop_avps;
        break;
    default:
        // should never get here, but keep gcc from complaining
        assert(False);
        return NULL;
    }

    for (; al; al = al->next) {
        pv_get_spec_value(msg, al->pv, &pt);
        if (pt.flags & PV_VAL_INT) {
            len += snprintf(request + len, sizeof(request),
                      "%.*s = %d ", al->name.len, al->name.s,
                   pt.ri);
        } else    if (pt.flags & PV_VAL_STR) {
            len += snprintf(request + len, sizeof(request),
                      "%.*s = %.*s ", al->name.len, al->name.s,
                   pt.rs.len, pt.rs.s);
        }

          if (len >= sizeof(request)) {
               LM_ERR("callcontrol request is longer than %ld bytes\n", (unsigned long)sizeof(request));
            return NULL;
             }
    }


    return request;
}


static char*
make_default_request(CallInfo *call)
{
    static char request[8192];
    int len;

    switch (call->action) {
    case CAInitialize:
        len = snprintf(request, sizeof(request),
                       "init\r\n"
                       "ruri: %.*s\r\n"
                       "diverter: %.*s\r\n"
                       "sourceip: %.*s\r\n"
                       "callid: %.*s\r\n"
                       "from: %.*s\r\n"
                       "fromtag: %.*s\r\n"
                       "sip_application: %.*s\r\n"
                       "\r\n",
                       call->ruri.len, call->ruri.s,
                       call->diverter.len, call->diverter.s,
                       call->source_ip.len, call->source_ip.s,
                       call->callid.len, call->callid.s,
                       call->from.len, call->from.s,
                       call->from_tag.len, call->from_tag.s,
                       call->sip_application.len, call->sip_application.s);

        if (len >= sizeof(request)) {
            LOG(L_ERR, "callcontrol request is longer than %ld bytes\n", (unsigned long)sizeof(request));
            return NULL;
        }

        break;

    case CAStart:
        len = snprintf(request, sizeof(request),
                       "start\r\n"
                       "callid: %.*s\r\n"
                       "dialogid: %d:%d\r\n"
                       "\r\n",
                       call->callid.len, call->callid.s,
                       call->dialog_id.h_entry, call->dialog_id.h_id);

        if (len >= sizeof(request)) {
            LOG(L_ERR, "callcontrol request is longer than %ld bytes\n", (unsigned long)sizeof(request));
            return NULL;
        }

        break;

    case CAStop:
        len = snprintf(request, sizeof(request),
                       "stop\r\n"
                       "callid: %.*s\r\n"
                       "\r\n",
                       call->callid.len, call->callid.s);

        if (len >= sizeof(request)) {
            LOG(L_ERR, "callcontrol request is longer than %ld bytes\n", (unsigned long)sizeof(request));
            return NULL;
        }

        break;

    default:
        // should never get here, but keep gcc from complaining
        assert(False);
        return NULL;
    }

    return request;
}


// Functions dealing with the external call_control helper
//

static Bool
callcontrol_connect(void)
{
    struct sockaddr_un addr;

    if (callcontrol_socket.sock >= 0)
        return True;

    if (callcontrol_socket.last_failure + RETRY_INTERVAL > time(NULL))
        return False;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, callcontrol_socket.name, sizeof(addr.sun_path) - 1);
#ifdef HAVE_SOCKADDR_SA_LEN
    addr.sun_len = strlen(addr.sun_path);
#endif

    callcontrol_socket.sock = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (callcontrol_socket.sock < 0) {
        LOG(L_ERR, "can't create socket\n");
        callcontrol_socket.last_failure = time(NULL);
        return False;
    }
    if (connect(callcontrol_socket.sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG(L_ERR, "failed to connect to %s: %s\n", callcontrol_socket.name, strerror(errno));
        close(callcontrol_socket.sock);
        callcontrol_socket.sock = -1;
        callcontrol_socket.last_failure = time(NULL);
        return False;
    }

    return True;
}

static void
callcontrol_disconnect(void)
{
    if (callcontrol_socket.sock < 0)
        return;

    close(callcontrol_socket.sock);
    callcontrol_socket.sock = -1;
    callcontrol_socket.last_failure = time(NULL);
}

static char*
send_command(char *command)
{
    int cmd_len, bytes, tries, sent, received, count;
    struct timeval timeout;
    fd_set rset;

    if (!callcontrol_connect())
        return NULL;

    cmd_len = strlen(command);

    for (sent=0, tries=0; sent<cmd_len && tries<3; tries++, sent+=bytes) {
        do
            bytes = send(callcontrol_socket.sock, command+sent, cmd_len-sent, MSG_DONTWAIT|MSG_NOSIGNAL);
        while (bytes == -1 && errno == EINTR);
        if (bytes == -1) {
            switch (errno) {
            case ECONNRESET:
            case EPIPE:
                callcontrol_disconnect();
                callcontrol_socket.last_failure = 0; // we want to reconnect immediately
                if (callcontrol_connect()) {
                    sent = bytes = 0;
                    continue;
                } else {
                    LOG(L_ERR, "connection with callcontrol did die\n");
                }
                break;
            case EACCES:
                LOG(L_ERR, "permission denied sending to %s\n", callcontrol_socket.name);
                break;
            case EWOULDBLOCK:
                // this shouldn't happen as we read back all the answer after a request.
                // if it would block, it means there is an error.
                LOG(L_ERR, "sending command would block!\n");
                break;
            default:
                LOG(L_ERR, "%d: %s\n", errno, strerror(errno));
                break;
            }
            callcontrol_disconnect();
            return NULL;
        }
    }
    if (sent < cmd_len) {
        LOG(L_ERR, "couldn't send complete command after 3 tries\n");
        callcontrol_disconnect();
        return NULL;
    }

    callcontrol_socket.data[0] = 0;
    received = 0;
    while (True) {
        FD_ZERO(&rset);
        FD_SET(callcontrol_socket.sock, &rset);
        timeout.tv_sec = callcontrol_socket.timeout / 1000;
        timeout.tv_usec = (callcontrol_socket.timeout % 1000) * 1000;

        do
            count = select(callcontrol_socket.sock + 1, &rset, NULL, NULL, &timeout);
        while (count == -1 && errno == EINTR);

        if (count == -1) {
            LOG(L_ERR, "select failed: %d: %s\n", errno, strerror(errno));
            callcontrol_disconnect();
            return NULL;
        } else if (count == 0) {
            LOG(L_ERR, "did timeout waiting for an answer\n");
            callcontrol_disconnect();
            return NULL;
        } else {
            do
                bytes = recv(callcontrol_socket.sock, callcontrol_socket.data+received, BUFFER_SIZE-1-received, 0);
            while (bytes == -1 && errno == EINTR);
            if (bytes == -1) {
                LOG(L_ERR, "failed to read answer: %d: %s\n", errno, strerror(errno));
                callcontrol_disconnect();
                return NULL;
            } else if (bytes == 0) {
                LOG(L_ERR, "connection with callcontrol closed\n");
                callcontrol_disconnect();
                return NULL;
            } else {
                callcontrol_socket.data[received+bytes] = 0;
                if (strstr(callcontrol_socket.data+received, "\r\n")!=NULL) {
                    break;
                }
                received += bytes;
            }
        }
    }

    return callcontrol_socket.data;
}


// Call control processing
//

// Return codes:
//   2 - No limit
//   1 - Limited
//  -1 - No credit
//  -2 - Locked
//  -5 - Internal error (message parsing, communication, ...)
static int
call_control_initialize(struct sip_msg *msg)
{
    CallInfo *call;
    char *message, *result = NULL;


    call = get_call_info(msg, CAInitialize);
    if (!call) {
        LOG(L_ERR, "can't retrieve call info\n");
        return -5;
    }


    if (!cc_init_avps)
        message = make_default_request(call);
    else
        message = make_custom_request(msg, call);

    if (!message)
        return -5;

   result = send_command(message);

    if (result==NULL) {
        return -5;
    } else if (strcasecmp(result, "No limit\r\n")==0) {
        return 2;
    } else if (strcasecmp(result, "Limited\r\n")==0) {
        return 1;
    } else if (strcasecmp(result, "No credit\r\n")==0) {
        return -1;
    } else if (strcasecmp(result, "Locked\r\n")==0) {
        return -2;
    } else {
        return -5;
    }
}


// Called during a dialog for start and update requests
//
// Return codes:
//   1 - Ok
//  -1 - Session not found
//  -5 - Internal error (message parsing, communication, ...)
static int
call_control_start(struct sip_msg *msg, struct dlg_cell *dlg)
{
    CallInfo *call;
    char *message, *result;

    call = get_call_info(msg, CAStart);
    if (!call) {
        LOG(L_ERR, "can't retrieve call info\n");
        return -5;
    }

    call->dialog_id.h_entry = dlg->h_entry;
    call->dialog_id.h_id = dlg->h_id;

    if (!cc_start_avps)
        message = make_default_request(call);
    else
        message = make_custom_request(msg, call);

    if (!message)
        return -5;

    result = send_command(message);

    if (result==NULL) {
        return -5;
    } else if (strcasecmp(result, "Ok\r\n")==0) {
        return 1;
    } else if (strcasecmp(result, "Not found\r\n")==0) {
        return -1;
    } else {
        return -5;
    }
}


// Called during a dialog ending to stop callcontrol
//
// Return codes:
//   1 - Ok
//  -1 - Session not found
//  -5 - Internal error (message parsing, communication, ...)
static int
call_control_stop(struct sip_msg *msg, str callid)
{
    CallInfo call;
    char *message, *result;

    call.action = CAStop;
    call.callid = callid;

    if (!cc_stop_avps)
        message = make_default_request(&call);
    else
        message = make_custom_request(msg, &call);

    if (!message)
        return -5;

    result = send_command(message);

    if (result==NULL) {
        return -5;
    } else if (strcasecmp(result, "Ok\r\n")==0) {
        return 1;
    } else if (strcasecmp(result, "Not found\r\n")==0) {
        return -1;
    } else {
        return -5;
    }
}


// Dialog callbacks and helpers
//

typedef enum {
    CCInactive = 0,
    CCActive
} CallControlState;


static void
__dialog_replies(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    struct sip_msg *reply = _params->rpl;

    if (reply!=FAKED_REPLY && reply->REPLY_STATUS==200) {
        call_control_start(reply, dlg);
    }
}


static void
__dialog_ended(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    if ((int)(long)*_params->param == CCActive) {
        struct sip_msg* msg = _params->rpl;
        if( !msg || msg == FAKED_REPLY)
            msg = _params->req;
        call_control_stop(msg, dlg->callid);
        *_params->param = (void*)CCInactive;
    }
}


static void
__dialog_created(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    struct sip_msg *request = _params->req;

    if (request->REQ_METHOD != METHOD_INVITE)
        return;

    if ((request->msg_flags & FL_USE_CALL_CONTROL) == 0)
        return;

    if (dlg_api.register_dlgcb(dlg, DLGCB_RESPONSE_FWDED, __dialog_replies, NULL, NULL) != 0)
        LOG(L_ERR, "cannot register callback for dialog confirmation\n");
    if (dlg_api.register_dlgcb(dlg, DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_EXPIRED | DLGCB_DESTROY, __dialog_ended, (void*)CCActive, NULL) != 0)
        LOG(L_ERR, "cannot register callback for dialog termination\n");

    // reset the flag to indicate that the dialog for callcontrol was created
    request->msg_flags &= ~FL_USE_CALL_CONTROL;
}


static void
__dialog_loaded(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    if (dlg_api.register_dlgcb(dlg, DLGCB_RESPONSE_FWDED, __dialog_replies, NULL, NULL) != 0)
        LOG(L_ERR, "cannot register callback for dialog confirmation\n");
    if (dlg_api.register_dlgcb(dlg, DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_EXPIRED | DLGCB_DESTROY, __dialog_ended, (void*)CCActive, NULL) != 0)
        LOG(L_ERR, "cannot register callback for dialog termination\n");
}


// Public API
//

// Return codes:
//   2 - No limit
//   1 - Limited
//  -1 - No credit
//  -2 - Locked
//  -3 - No provider
//  -5 - Internal error (message parsing, communication, ...)
static int
CallControl(struct sip_msg *msg, char *str1, char *str2)
{
    int result;

    if (disable)
        return 2;

    if (msg->first_line.type!=SIP_REQUEST || msg->REQ_METHOD!=METHOD_INVITE || has_to_tag(msg)) {
        LOG(L_WARN, "call_control should only be called for the first INVITE\n");
        return -5;
    }

    result = call_control_initialize(msg);
    if (result == 1) {
        // A call with a time limit that will be traced by callcontrol
        msg->msg_flags |= FL_USE_CALL_CONTROL;
        setflag(msg, dialog_flag); // have the dialog module trace this dialog
    }

    return result;
}


// Module management: initialization/destroy/function-parameter-fixing/...
//

static int
mod_init(void)
{
    pv_spec_t avp_spec;
    int *param;
	 modparam_t type;


    // initialize the canonical_uri_avp structure
    if (canonical_uri_avp.spec.s==NULL || canonical_uri_avp.spec.len<=0) {
        LOG(L_ERR, "missing/empty canonical_uri_avp parameter. using default.\n");
        canonical_uri_avp.spec.s = CANONICAL_URI_AVP_SPEC;
    }

    if (pv_parse_spec(&(canonical_uri_avp.spec), &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
        LOG(L_CRIT, "invalid AVP specification for canonical_uri_avp: `%s'\n", canonical_uri_avp.spec.s);
        return -1;
    }
    if (pv_get_avp_name(0, &(avp_spec.pvp), &(canonical_uri_avp.name), &(canonical_uri_avp.type))!=0) {
        LOG(L_CRIT, "invalid AVP specification for canonical_uri_avp: `%s'\n", canonical_uri_avp.spec.s);
        return -1;
    }

    // initialize the signaling_ip_avp structure
    if (signaling_ip_avp.spec.s==NULL || signaling_ip_avp.spec.len<=0) {
        LOG(L_ERR, "missing/empty signaling_ip_avp parameter. using default.\n");
        signaling_ip_avp.spec.s = SIGNALING_IP_AVP_SPEC;
    }

    if (pv_parse_spec(&(signaling_ip_avp.spec), &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
        LOG(L_CRIT, "invalid AVP specification for signaling_ip_avp: `%s'\n", signaling_ip_avp.spec.s);
        return -1;
    }
    if (pv_get_avp_name(0, &(avp_spec.pvp), &(signaling_ip_avp.name), &(signaling_ip_avp.type))!=0) {
        LOG(L_CRIT, "invalid AVP specification for signaling_ip_avp: `%s'\n", signaling_ip_avp.spec.s);
        return -1;
    }

    // initialize the sip_application_avp structure
    if (sip_application_avp.spec.s==NULL || sip_application_avp.spec.len<=0) {
        LOG(L_ERR, "missing/empty sip_application_avp parameter. using default.\n");
        sip_application_avp.spec.s = SIP_APPLICATION_AVP_SPEC;
    }

    if (pv_parse_spec(&(sip_application_avp.spec), &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
        LOG(L_CRIT, "invalid AVP specification for sip_application_avp: `%s'\n", sip_application_avp.spec.s);
        return -1;
    }
    if (pv_get_avp_name(0, &(avp_spec.pvp), &(sip_application_avp.name), &(sip_application_avp.type))!=0) {
        LOG(L_CRIT, "invalid AVP specification for sip_application_avp: `%s'\n", sip_application_avp.spec.s);
        return -1;
    }

    // bind to the dialog API
    if (load_dlg_api(&dlg_api)!=0) {
        LOG(L_CRIT, "cannot load the dialog module API\n");
        return -1;
    }

    // load dlg_flag and default_timeout parameters from the dialog module
    param = find_param_export(find_module_by_name("dialog"), "dlg_flag", INT_PARAM, &type);
    if (!param) {
        LOG(L_CRIT, "cannot find dlg_flag parameter in the dialog module\n");
        return -1;
    }
    if (type != INT_PARAM) {
        LOG(L_CRIT, "dlg_flag parameter found but with wrong type: %d\n", type);
        return -1;
    }

    dialog_flag = *param;

    // register dialog creation callback
    if (dlg_api.register_dlgcb(NULL, DLGCB_CREATED, __dialog_created, NULL, NULL) != 0) {
        LOG(L_CRIT, "cannot register callback for dialog creation\n");
        return -1;
    }

    // register dialog loading callback
    if (dlg_api.register_dlgcb(NULL, DLGCB_LOADED, __dialog_loaded, NULL, NULL) != 0) {
        LOG(L_ERR, "cannot register callback for dialogs loaded from the database\n");
    }

    // register a pre-script callback to automatically enable dialog tracing
    if (register_script_cb(postprocess_request, POST_SCRIPT_CB|REQUEST_CB, 0) != 0) {
        LOG(L_CRIT, "ERROR:call_control:mod_init: could not register request postprocessing callback\n");
        return -1;
    }

    return 0;
}


static int
child_init(int rank)
{
    // initialize the connection to callcontrol if needed
    if (!disable)
        callcontrol_connect();

    return 0;
}


static void
destroy(void) {
	if (cc_init_avps)
		destroy_list(cc_init_avps);

	if (cc_start_avps)
		destroy_list(cc_start_avps);

	if (cc_stop_avps)
		destroy_list(cc_stop_avps);
}


// Postprocess a request after the main script route is done.
//
// After all script processing is done, check if the dialog was actually
// created to take care of call control. If the FL_USE_CALL_CONTROL flag
// is still set, then the dialog creation callback was not called which
// means that there was a failure relaying the message and we have to
// tell the call control application to discard the call, otherwise it
// would remain dangling until it expires.
//
static int
postprocess_request(struct sip_msg *msg, unsigned int flags, void *_param)
{
    CallInfo *call;

    if ((msg->msg_flags & FL_USE_CALL_CONTROL) == 0)
        return 1;

    // the FL_USE_CALL_CONTROL flag is still set => the dialog was not created

    LOG(L_WARN, "dialog to trace controlled call was not created. discarding callcontrol.");

    call = get_call_info(msg, CAStop);
    if (!call) {
        LOG(L_ERR, "can't retrieve call info\n");
        return -1;
    }
    call_control_stop(msg, call->callid);

    return 1;
}


