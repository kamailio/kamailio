/* $Id$
 *
 * Copyright (C) 2004 Dan Pascu
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
 *
 */

// TODO
//
// - make the asymmetric files use CFG_DIR
// - find a way to install the config files with make install

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../error.h"
#include "../../data_lump.h"
#include "../../forward.h"
#include "../../mem/mem.h"
#include "../../resolve.h"
#include "../../timer.h"
#include "../../ut.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../msg_translator.h"
#include "../registrar/sip_msg.h"
#include "../usrloc/usrloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>


MODULE_VERSION


// Although `AF_LOCAL' is mandated by POSIX.1g, `AF_UNIX' is portable to
// more systems.  `AF_UNIX' was the traditional name stemming from BSD, so
// even most POSIX systems support it.  It is also the name of choice in
// the Unix98 specification. So if there's no AF_LOCAL fallback to AF_UNIX
#ifndef AF_LOCAL
# define AF_LOCAL AF_UNIX
#endif


#define min(x, y)         (((x) < (y)) ? (x) : (y))

#define isAnyAddress(adr) ((adr).len==7 && memcmp("0.0.0.0", (adr).s, 7)==0)


typedef int Bool;
#define True  1
#define False 0


typedef int  (*CheckLocalPartyProc)(struct sip_msg* msg, char* s1, char* s2);

typedef Bool (*NatTestProc)(struct sip_msg* msg);


typedef enum {
    NTNone=0,
    NTPrivateContact=1,
    NTSourceAddress=2,
    NTPrivateVia=4
} NatTestType;

typedef enum {
    STUnknown=0,
    STAudio,
    STVideo,
    STAudioVideo
} StreamType;

typedef struct {
    const char *name;
    uint32_t address;
    uint32_t mask;
} NetInfo;

typedef struct {
    str ip;
    str port;
    str type;    // stream type (`audio', `video', ...)
    int localIP; // true if the IP is locally defined inside this media stream
} StreamInfo;

typedef struct {
    NatTestType test;
    NatTestProc proc;
} NatTest;

typedef struct {
    char *file;        // the file which lists the asymmetric clients
    long timestamp;    // for checking if it was modified

    regex_t **clients; // the asymmetric clients regular expressions
    int size;          // size of array above
    int count;         // how many clients are in array above
} AsymmetricClients;


/* Function prototypes */
static int ClientNatTest(struct sip_msg *msg, char *str1, char *str2);
static int FixContact(struct sip_msg *msg, char *str1, char *str2);
static int UseMediaProxy(struct sip_msg *msg, char *str1, char *str2);
static int EndMediaSession(struct sip_msg *msg, char *str1, char *str2);

static int mod_init(void);
static int fixstring2int(void **param, int param_count);

static Bool testPrivateContact(struct sip_msg* msg);
static Bool testSourceAddress(struct sip_msg* msg);
static Bool testPrivateVia(struct sip_msg* msg);


/* Local global variables */
static char *mediaproxySocket = "/var/run/proxydispatcher.sock";

static int natpingInterval = 20; // 20 seconds

static usrloc_api_t userLocation;

static AsymmetricClients sipAsymmetrics = {
    "/etc/ser/sip-asymmetric-clients",
    //CFG_DIR"/sip-asymmetric-clients",
    0,
    NULL,
    0,
    0
};

static AsymmetricClients rtpAsymmetrics = {
    "/etc/ser/rtp-asymmetric-clients",
    0,
    NULL,
    0,
    0
};

static CheckLocalPartyProc isFromLocal;
//static CheckLocalPartyProc isToLocal;
static CheckLocalPartyProc isDestinationLocal;

NetInfo rfc1918nets[] = {
    {"10.0.0.0",    0x0a000000UL, 0xff000000UL},
    {"172.16.0.0",  0xac100000UL, 0xfff00000UL},
    {"192.168.0.0", 0xc0a80000UL, 0xffff0000UL},
    {NULL,          0UL,          0UL}
};

NatTest natTests[] = {
    {NTPrivateContact, testPrivateContact},
    {NTSourceAddress,  testSourceAddress},
    {NTPrivateVia,     testPrivateVia},
    {NTNone,           NULL}
};

static cmd_export_t commands[] = {
    {"fix_contact",       FixContact,      0, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
    {"use_media_proxy",   UseMediaProxy,   0, 0,             REQUEST_ROUTE | ONREPLY_ROUTE },
    {"end_media_session", EndMediaSession, 0, 0,             REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
    {"client_nat_test",   ClientNatTest,   1, fixstring2int, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
    {0, 0, 0, 0, 0}
};

static param_export_t parameters[] = {
    {"mediaproxy_socket", STR_PARAM, &mediaproxySocket},
    {"sip_asymmetrics",   STR_PARAM, &(sipAsymmetrics.file)},
    {"rtp_asymmetrics",   STR_PARAM, &(rtpAsymmetrics.file)},
    {"natping_interval",  INT_PARAM, &natpingInterval},
    {0, 0, 0}
};

struct module_exports exports = {
    "mediaproxy", // module name
    commands,     // module exported functions
    parameters,   // module exported parameters
    mod_init,     // module init (before any kid is created. kids will inherit)
    NULL,         // reply processing
    NULL,         // destroy function
    NULL,         // on_break
    NULL          // child_init
};



/* Helper functions */

// Functions dealing with strings

/*
 * strfind() finds the start of the first occurrence of the substring needle
 * of length nlen in the memory area haystack of length len.
 */
static void*
strfind(const void *haystack, size_t len, const void *needle, size_t nlen)
{
    char *sp;

    /* Sanity check */
    if(!(haystack && needle && nlen && len>=nlen))
        return NULL;

    for (sp = (char*)haystack; sp <= (char*)haystack + len - nlen; sp++) {
        if (*sp == *(char*)needle && memcmp(sp, needle, nlen)==0) {
            return sp;
        }
    }

    return NULL;
}

/*
 * strcasefind() finds the start of the first occurrence of the substring
 * needle of length nlen in the memory area haystack of length len by doing
 * a case insensitive search
 */
static void*
strcasefind(const char *haystack, size_t len, const char *needle, size_t nlen)
{
    char *sp;

    /* Sanity check */
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

/* returns string with whitespace trimmed from left end */
static inline void
ltrim(str *string)
{
    while (string->len>0 && isspace(*(string->s))) {
        string->len--;
        string->s++;
    }
}

/* returns string with whitespace trimmed from right end */
static inline void
rtrim(str *string)
{
    char *ptr;

    ptr = string->s + string->len - 1;
    while (string->len>0 && (*ptr==0 || isspace(*ptr))) {
        string->len--;
        ptr--;
    }
}

/* returns string with whitespace trimmed from both ends */
static inline void
trim(str *string)
{
    ltrim(string);
    rtrim(string);
}

/* returns a pointer to first CR or LF char found or the end of string */
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


/* Free the returned string with pkg_free() after you've done with it */
static char*
encodeQuopri(str buf)
{
    char *result = pkg_malloc(buf.len*3+1);
    int i, j;
    char c;

    result = pkg_malloc(buf.len*3+1);
    if (!result) {
        LOG(L_ERR, "error: mediaproxy/encodeQuopri(): out of memory\n");
        return NULL;
    }

    for (i=0, j=0; i<buf.len; i++) {
        c = buf.s[i];
        if ((c>0x20 && c<0x7f && c!='=') || c=='\n' || c=='\r') {
            result[j++] = c;
        } else {
            result[j++] = '=';
            sprintf(&result[j], "%02X", (c & 0xff));
            j += 2;
        }
    }
    result[j] = 0;

    return result;
}


/* Find a line in str `block' that starts with `start'. */
static char*
findLineStartingWith(str *block, char *start, int ignoreCase)
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
        if (!ptr || ptr==zone.s || ptr[-1]=='\n' || ptr[-1]=='\r')
            break;
        zone.s = ptr + tlen;
    }

    return ptr;
}


/* get up to `limit' whitespace separated tokens from `char *string' */
static int
getTokens(char *string, str *tokens, int limit)
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

/* get up to `limit' whitespace separated tokens from `str *string' */
static int
getStrTokens(str *string, str *tokens, int limit)
{
    int count;
    char c;

    if (!string || !string->s) {
        return 0;
    }

    c = string->s[string->len];
    string->s[string->len] = 0;

    count = getTokens(string->s, tokens, limit);

    string->s[string->len] = c;

    return count;
}


/* Functions to extract the info we need from the SIP/SDP message */

/* Extract Call-ID value. */
static Bool
getCallId(struct sip_msg* msg, str *cid)
{
    if (msg->callid == NULL) {
        if (parse_headers(msg, HDR_CALLID, 0) == -1) {
            return False;
        }
        if (msg->callid == NULL) {
            return False;
        }
    }

    *cid = msg->callid->body;

    trim(cid);

    return True;
}


/* Get caller domain */
static str
getFromDomain(struct sip_msg* msg)
{
    static char buf[16] = "unknown"; // buf is here for a reason. don't
    static str notfound = {buf, 7};  // use the constant string directly!
    static struct sip_uri puri;
    str uri;

    if (parse_from_header(msg) < 0) {
        LOG(L_ERR, "error: mediaproxy/getFromDomain(): error parsing `From' header\n");
        return notfound;
    }

    uri = get_from(msg)->uri;

    if (parse_uri(uri.s, uri.len, &puri) < 0) {
        LOG(L_ERR, "error: mediaproxy/getFromDomain(): error parsing `From' URI\n");
        return notfound;
    } else if (puri.host.len == 0) {
        return notfound;
    }

    return puri.host;
}

/* Get called domain */
static str
getToDomain(struct sip_msg* msg)
{
    static char buf[16] = "unknown"; // buf is here for a reason. don't
    static str notfound = {buf, 7};  // use the constant string directly!
    static struct sip_uri puri;
    str uri;

    uri = get_to(msg)->uri;

    if (parse_uri(uri.s, uri.len, &puri) < 0) {
        LOG(L_ERR, "error: mediaproxy/getToDomain(): error parsing `To' URI\n");
        return notfound;
    } else if (puri.host.len == 0) {
        return notfound;
    }

    return puri.host;

}

/* Get destination domain */
// This function only works when called for a request although it's more
// reliable than the getToDomain function.
static str
getDestinationDomain(struct sip_msg* msg)
{
    static char buf[16] = "unknown"; // buf is here for a reason. don't
    static str notfound = {buf, 7};  // use the constant string directly!

    if (parse_sip_msg_uri(msg) < 0) {
        LOG(L_ERR, "error: mediaproxy/getDestinationDomain(): error parsing destination URI\n");
        return notfound;
    } else if (msg->parsed_uri.host.len==0) {
        return notfound;
    }

    return msg->parsed_uri.host;
}


/* Get From tag */
static str
getFromAddress(struct sip_msg *msg)
{
    static char buf[16] = "unknown"; // buf is here for a reason. don't
    static str notfound = {buf, 7};  // use the constant string directly!
    str uri;
    char *ptr;

    if (parse_from_header(msg) == -1) {
        LOG(L_ERR, "error: mediaproxy/getFromAddress(): error parsing From: field\n");
        return notfound;
    }

    uri = get_from(msg)->uri;

    if (uri.len == 0)
        return notfound;

    if (strncmp(uri.s, "sip:", 4)==0) {
        uri.s += 4;
        uri.len -= 4;
    }

    if ((ptr = strfind(uri.s, uri.len, ";", 1))!=NULL) {
        uri.len = ptr - uri.s;
    }

    return uri;
}


/* Get To tag */
static str
getToAddress(struct sip_msg *msg)
{
    static char buf[16] = "unknown"; // buf is here for a reason. don't
    static str notfound = {buf, 7};  // use the constant string directly!
    str uri;
    char *ptr;

    if (!msg->to) {
        LOG(L_ERR, "error: mediaproxy/getToAddress(): missing To: field\n");
        return notfound;
    }

    uri = get_to(msg)->uri;

    if (uri.len == 0)
        return notfound;

    if (strncmp(uri.s, "sip:", 4)==0) {
        uri.s += 4;
        uri.len -= 4;
    }

    if ((ptr = strfind(uri.s, uri.len, ";", 1))!=NULL) {
        uri.len = ptr - uri.s;
    }

    return uri;
}


/* Get From tag */
static str
getFromTag(struct sip_msg *msg)
{
    static char buf[4] = "";        // buf is here for a reason. don't
    static str notfound = {buf, 0}; // use the constant string directly!
    str tag;

    if (parse_from_header(msg) == -1) {
        LOG(L_ERR, "error: mediaproxy/getFromTag(): error parsing From: field\n");
        return notfound;
    }

    tag = get_from(msg)->tag_value;

    if (tag.len == 0)
        return notfound;

    return tag;
}


/* Get To tag */
static str
getToTag(struct sip_msg *msg)
{
    static char buf[4] = "";        // buf is here for a reason. don't
    static str notfound = {buf, 0}; // use the constant string directly!
    str tag;

    if (!msg->to) {
        LOG(L_ERR, "error: mediaproxy/getToTag(): missing To: field\n");
        return notfound;
    }

    tag = get_to(msg)->tag_value;

    if (tag.len == 0)
        return notfound;

    return tag;
}


/* Extract User-Agent */
static str
getUserAgent(struct sip_msg* msg)
{
    static char buf[16] = "unknown-agent"; // buf is here for a reason. don't
    static str notfound = {buf, 13};       // use the constant string directly!
    str block, server;
    char *ptr;

    if ((parse_headers(msg, HDR_USERAGENT, 0)!=-1) && msg->user_agent &&
        msg->user_agent->body.len>0) {
        return msg->user_agent->body;
    }

    // If we can't find user-agent, look after the Server: field

    // This is a temporary hack. Normally it should be extracted by ser
    // (either as the Server field, or if User-Agent is missing in place
    // of the User-Agent field)

    block.s   = msg->buf;
    block.len = msg->len;

    ptr = findLineStartingWith(&block, "Server:", True);
    if (!ptr)
        return notfound;

    server.s   = ptr + 7;
    server.len = findendline(server.s, block.s+block.len-server.s) - server.s;

    trim(&server);
    if (server.len == 0)
        return notfound;

    return server;
}

// Get URI from the Contact: field.
static Bool
getContactURI(struct sip_msg* msg, struct sip_uri *uri, contact_t** _c)
{

    if ((parse_headers(msg, HDR_CONTACT, 0) == -1) || !msg->contact)
        return False;

    if (!msg->contact->parsed && parse_contact(msg->contact) < 0) {
        LOG(L_ERR, "error: mediaproxy/getContactURI(): error parsing Contact body\n");
        return False;
    }

    *_c = ((contact_body_t*)msg->contact->parsed)->contacts;

    if (*_c == NULL) {
        LOG(L_ERR, "error: mediaproxy/getContactURI(): error parsing Contact body\n");
        return False;
    }

    if (parse_uri((*_c)->uri.s, (*_c)->uri.len, uri) < 0 || uri->host.len <= 0) {
        LOG(L_ERR, "error: mediaproxy/getContactURI(): error parsing Contact URI\n");
        return False;
    }

    return True;
}


// Functions to manipulate the SDP message body

static Bool
checkContentType(struct sip_msg *msg)
{
    str type;

    if (!msg->content_type) {
        LOG(L_WARN, "warning: mediaproxy/checkContentType(): Content-Type "
            "header missing! Let's assume the content is text/plain ;-)\n");
        return True;
    }

    type = msg->content_type->body;
    trim(&type);

    if (strncasecmp(type.s, "application/sdp", 15) != 0) {
        LOG(L_ERR, "error: mediaproxy/checkContentType(): invalid Content-Type "
            "for SDP message\n");
        return False;
    }

    if (!(isspace(type.s[15]) || type.s[15] == ';' || type.s[15] == 0)) {
        LOG(L_ERR,"error: mediaproxy/checkContentType(): invalid character "
            "after Content-Type!\n");
        return False;
    }

    return True;
}


// Get the SDP message from SIP message and check it's Content-Type
// return -1 on error, 0 if empty message, 1 if message present and not empty
static int
getSDPMessage(struct sip_msg *msg, str *sdp)
{
    sdp->s = get_body(msg);
    if (sdp->s==NULL) {
        LOG(L_ERR, "error: mediaproxy/getSDPMessage(): cannot get the SDP body from SIP message\n");
        return -1;
    }
    sdp->len = msg->buf + msg->len - sdp->s;
    if (sdp->len==0) {
        // 0 length body is ok for ACK messages
        if (!(msg->first_line.type == SIP_REQUEST &&
              msg->first_line.u.request.method_value == METHOD_ACK)) {
            LOG(L_ERR, "error: mediaproxy/getSDPMessage(): SDP message has zero length\n");
        }
        return 0;
    }

    if (!checkContentType(msg)) {
        LOG(L_ERR, "error: mediaproxy/getSDPMessage(): content type is not `application/sdp'\n");
        return -1;
    }

    return 1;
}


// will return the ip address present in a `c=' line in the given block
// returns: -1 on error, 0 if not found, 1 if found
static int
getMediaIPFromBlock(str *block, str *mediaip)
{
    str tokens[3], zone;
    char *ptr;
    int count;

    ptr = findLineStartingWith(block, "c=", False);

    if (!ptr) {
        mediaip->s   = NULL;
        mediaip->len = 0;
        return 0;
    }

    zone.s = ptr + 2;
    zone.len = findendline(zone.s, block->s + block->len - zone.s) - zone.s;

    count = getStrTokens(&zone, tokens, 3);

    if (count != 3) {
        LOG(L_ERR, "error: mediaproxy/getMediaIPFromBlock(): invalid `c=' "
            "line in SDP body\n");
        return -1;
    }

    // can also check if tokens[1] == 'IP4'
    *mediaip = tokens[2];

    return 1;
}


// Get the session-level media IP defined by the SDP message
static Bool
getSessionLevelMediaIP(str *sdp, str *mediaip)
{
    str block;
    char *ptr;

    // session IP can be found from the beginning up to the first media block
    ptr = findLineStartingWith(sdp, "m=", False);
    if (ptr) {
        block.s   = sdp->s;
        block.len = ptr - block.s;
    } else {
        block = *sdp;
    }

    if (getMediaIPFromBlock(&block, mediaip) == -1) {
        LOG(L_ERR, "error: mediaproxy/getSessionLevelMediaIP(): parse error "
            "while getting session-level media IP from SDP body\n");
        return False;
    }

    // it's not an error to be missing. it can be locally defined
    // by each media stream. thus we return true even if not found
    return True;
}


// will get all media streams
static int
getMediaStreams(str *sdp, str *sessionIP, StreamInfo *streams, int limit)
{
    str tokens[2], block, zone;
    char *ptr, *sdpEnd;
    int i, count, streamCount, result;

    sdpEnd = sdp->s + sdp->len;

    for (i=0, block=*sdp; i<limit; i++) {
        ptr = findLineStartingWith(&block, "m=", False);

        if (!ptr)
            break;

        zone.s = ptr + 2;
        zone.len = findendline(zone.s, sdpEnd - zone.s) - zone.s;
        count = getStrTokens(&zone, tokens, 2);

        if (count != 2) {
            LOG(L_ERR, "error: mediaproxy/getMediaStreams(): invalid `m=' "
                "line in SDP body\n");
            return -1;
        }

        streams[i].type = tokens[0];
        streams[i].port = tokens[1];

        block.s   = zone.s + zone.len;
        block.len = sdpEnd - block.s;
    }

    streamCount = i;

    for (i=0; i<streamCount; i++) {
        block.s = streams[i].port.s;
        if (i < streamCount-1)
            block.len = streams[i+1].port.s - block.s;
        else
            block.len = sdpEnd - block.s;

        result = getMediaIPFromBlock(&block, &(streams[i].ip));
        if (result == -1) {
            LOG(L_ERR, "error: mediaproxy/getMediaStreams(): parse error in "
                "getting the contact IP for the media stream nr. %d\n", i+1);
            return -1;
        } else if (result == 0) {
            if (sessionIP->s == NULL) {
                LOG(L_ERR, "error: mediaproxy/getMediaStreams(): media stream "
                    "doesn't define a contact IP and the session-level IP "
                    "is missing\n");
                return -1;
            }
            streams[i].ip = *sessionIP;
            streams[i].localIP = 0;
        } else {
            streams[i].localIP = 1;
        }
    }

    return streamCount;
}


static Bool
replaceElement(struct sip_msg *msg, str *oldElem, str *newElem)
{
    struct lump* anchor;
    char *buf;

    if (newElem->len==oldElem->len &&
        memcmp(newElem->s, oldElem->s, newElem->len)==0) {
        return True;
    }

    buf = pkg_malloc(newElem->len);
    if (!buf) {
        LOG(L_ERR, "error: mediaproxy/replaceElement(): out of memory\n");
        return False;
    }

    anchor = del_lump(msg, oldElem->s - msg->buf, oldElem->len, 0);
    if (!anchor) {
        LOG(L_ERR, "error: mediaproxy/replaceElement(): failed to delete old element\n");
        pkg_free(buf);
        return False;
    }

    memcpy(buf, newElem->s, newElem->len);

    if (insert_new_lump_after(anchor, buf, newElem->len, 0)==0) {
        LOG(L_ERR, "error: mediaproxy/replaceElement(): failed to insert new element\n");
        pkg_free(buf);
        return False;
    }

    return True;
}


// Functions dealing with the external mediaproxy helper

static inline size_t
uwrite(int fd, const void *buf, size_t count)
{
    int len;

    do
        len = write(fd, buf, count);
    while (len == -1 && errno == EINTR);

    return len;
}

static inline size_t
uread(int fd, void *buf, size_t count)
{
    int len;

    do
        len = read(fd, buf, count);
    while (len == -1 && errno == EINTR);

    return len;
}

static inline size_t
readall(int fd, void *buf, size_t count)
{
    int len, total;

    for (len=0, total=0; count-total>0; total+=len) {
        len = uread(fd, buf+total, count-total);
        if (len == -1) {
            return -1;
        }
        if (len == 0) {
            break;
        }
    }

    return total;
}

static char*
sendMediaproxyCommand(char *command)
{
    struct sockaddr_un addr;
    int smpSocket, len;
    static char buf[1024];

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, mediaproxySocket, sizeof(addr.sun_path) - 1);
#ifdef HAVE_SOCKADDR_SA_LEN
    addr.sun_len = strlen(addr.sun_path);
#endif

    smpSocket = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (smpSocket < 0) {
        LOG(L_ERR, "error: mediaproxy/sendMediaproxyCommand(): can't create socket\n");
        return NULL;
    }
    if (connect(smpSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(smpSocket);
        LOG(L_ERR, "error: mediaproxy/sendMediaproxyCommand(): can't connect to MediaProxy\n");
        return NULL;
    }

    len = uwrite(smpSocket, command, strlen(command));

    if (len <= 0) {
        close(smpSocket);
        LOG(L_ERR, "error: mediaproxy/sendMediaproxyCommand(): can't send command to MediaProxy\n");
        return NULL;
    }

    len = readall(smpSocket, buf, sizeof(buf)-1);

    close(smpSocket);

    if (len < 0) {
        LOG(L_ERR, "error: mediaproxy/sendMediaproxyCommand(): can't read reply from MediaProxy\n");
        return NULL;
    }

    buf[len] = 0;

    return buf;
}


// Miscelaneous helper functions

/* Test if IP in `address' belongs to a RFC1918 network */
static inline int
rfc1918address(str *address)
{
    struct in_addr inaddr;
    uint32_t netaddr;
    int i, result, n2ok, n2;
    char c;

    c = address->s[address->len];
    address->s[address->len] = 0;

    result = inet_aton(address->s, &inaddr);

    address->s[address->len] = c;

    if (result==0)
        return -1; /* invalid address to test */

    netaddr = ntohl(inaddr.s_addr);

    for (i=0; rfc1918nets[i].name!=NULL; i++) {
        if (rfc1918nets[i].address == 0xac100000UL) {
            /* for '172.16.0.0',  2nd number should between 16 and 31 only */
            n2 = (rfc1918nets[i].address >> 16) & 0xff;
            n2ok = (n2>=16 && n2<=31 ? 1 : 0);
        } else {
            n2ok = 1;
        }
        if (n2ok && (netaddr & rfc1918nets[i].mask)==rfc1918nets[i].address) {
            return 1;
        }
    }

    return 0;
}

#define isPrivateAddress(x) (rfc1918address(x)==1 ? 1 : 0)
#define isPublicAddress(x)  (rfc1918address(x)==0 ? 1 : 0)


// Check if the requested asymmetrics file has changed and reload it if needed
static void
checkAsymmetricFile(AsymmetricClients *aptr)
{
    char buf[512], errbuf[256], *which;
    regex_t *re, **regs;
    int i, size, code;
    Bool firstTime = False;
    struct stat statbuf;
    FILE *file;
    str line;

    if (stat(aptr->file, &statbuf) < 0)
        return; // ignore missing file

    if (statbuf.st_mtime <= aptr->timestamp)
        return; // not changed

    // now we have work to do

    which = (aptr == &sipAsymmetrics ? "SIP" : "RTP");

    if (!aptr->clients) {
        // if we are here the first time allocate memory to hold the regexps

        // initial size of array
        // (hopefully not reached so we won't need to realloc)
        size = 32;
        aptr->clients = (regex_t**)pkg_malloc(size*sizeof(regex_t**));
        if (!aptr->clients) {
            LOG(L_WARN, "warning: mediaproxy/checkAsymmetricFile() cannot "
                "allocate memory for the %s asymmetric client list. "
                "%s asymmetric clients will not be handled properly.\n",
                which, which);
            return; // ignore as it is not fatal
        }
        aptr->size  = size;
        aptr->count = 0;
        firstTime = True;
    } else {
        // else clean old stuff
        for (i=0; i<aptr->count; i++) {
            regfree(aptr->clients[i]);
            pkg_free(aptr->clients[i]);
            aptr->clients[i] = NULL;
        }
        aptr->count = 0;
    }

    // read new
    file = fopen(aptr->file, "r");
    i = 0; // this will count on which line are we in the file
    while (!feof(file)) {
        if (!fgets(buf, 512, file))
            break;
        i++;

        line.s = buf;
        line.len = strlen(buf);
        trim(&line);
        if (line.len == 0 || line.s[0] == '#')
            continue; // comment or empty line. ignore
        line.s[line.len] = 0;

        re = (regex_t*)pkg_malloc(sizeof(regex_t));
        code = regcomp(re, line.s, REG_EXTENDED|REG_ICASE|REG_NOSUB);
        if (code == 0) {
            if (aptr->count+1 > aptr->size) {
                size = aptr->size * 2;
                regs = aptr->clients;
                regs = (regex_t**)pkg_realloc(regs, size*sizeof(regex_t**));
                if (!regs) {
                    LOG(L_WARN, "warning: mediaproxy/checkAsymmetricFile(): "
                        "cannot allocate memory for all the %s asymmetric "
                        "clients listed in file. Some of them will not be "
                        "handled properly.\n", which);
                    break;
                }
                aptr->clients = regs;
                aptr->size = size;
            }
            aptr->clients[aptr->count] = re;
            aptr->count++;
        } else {
            regerror(code, re, errbuf, 256);
            LOG(L_WARN, "warning: mediaproxy/checkAsymmetricFile(): cannot "
                "compile line %d of the %s asymmetric clients file into a "
                "regular expression (will be ignored): %s", i, which, errbuf);
            pkg_free(re);
        }
    }

    aptr->timestamp = statbuf.st_mtime;

    LOG(L_INFO, "info: mediaproxy: %sloaded %s asymmetric clients file "
        "containing %d entr%s.\n", firstTime ? "" : "re",
        which, aptr->count, aptr->count==1 ? "y" : "ies");

}


//
// This is a hack. Until a I find a better way to allow all childs to update
// the asymmetric client list when the files change on disk, it stays as it is
// A timer registered from mod_init() only runs in one of the ser processes
// and the others will always see the file that was read at startup.
//
#include <time.h>
#define CHECK_INTERVAL 5
static void
periodicAsymmetricsCheck(void)
{
    static time_t last = 0;
    time_t now;

    // this is not guaranteed to run at every CHECK_INTERVAL.
    // it is only guaranteed that the files won't be checked more often.
    now = time(NULL);
    if (now > last + CHECK_INTERVAL) {
        checkAsymmetricFile(&sipAsymmetrics);
        checkAsymmetricFile(&rtpAsymmetrics);
        last = now;
    }
}

static Bool
isSIPAsymmetric(str userAgent)
{
    int i, code;
    char c;

    periodicAsymmetricsCheck();

    if (!sipAsymmetrics.clients || sipAsymmetrics.count==0)
        return False;

    c = userAgent.s[userAgent.len];
    userAgent.s[userAgent.len] = 0;

    for (i=0; i<sipAsymmetrics.count; i++) {
        code = regexec(sipAsymmetrics.clients[i], userAgent.s, 0, NULL, 0);
        if (code == 0) {
            userAgent.s[userAgent.len] = c;
            return True;
        } else if (code != REG_NOMATCH) {
            char errbuf[256];
            regerror(code, sipAsymmetrics.clients[i], errbuf, 256);
            LOG(L_WARN, "warning: mediaproxy/isSIPAsymmetric() failed to "
                "match regexp: %s\n", errbuf);
        }
    }

    userAgent.s[userAgent.len] = c;

    return False;
}


static Bool
isRTPAsymmetric(str userAgent)
{
    int i, code;
    char c;

    periodicAsymmetricsCheck();

    if (!rtpAsymmetrics.clients || rtpAsymmetrics.count==0)
        return False;

    c = userAgent.s[userAgent.len];
    userAgent.s[userAgent.len] = 0;

    for (i=0; i<rtpAsymmetrics.count; i++) {
        code = regexec(rtpAsymmetrics.clients[i], userAgent.s, 0, NULL, 0);
        if (code == 0) {
            userAgent.s[userAgent.len] = c;
            return True;
        } else if (code != REG_NOMATCH) {
            char errbuf[256];
            regerror(code, rtpAsymmetrics.clients[i], errbuf, 256);
            LOG(L_WARN, "warning: mediaproxy/isRTPAsymmetric() failed to "
                "match regexp: %s\n", errbuf);
        }
    }

    userAgent.s[userAgent.len] = c;

    return False;
}


// NAT tests

/* tests if address of signaling is different from address in Via field */
static Bool
testSourceAddress(struct sip_msg* msg)
{
    return (received_test(msg) ? True : False);
}

/* tests if Contact field contains a private IP address as defined in RFC1918 */
static Bool
testPrivateContact(struct sip_msg* msg)
{
    struct sip_uri uri;
    contact_t* contact;

    if (!getContactURI(msg, &uri, &contact))
        return False;

    return isPrivateAddress(&(uri.host));
}

/* tests if top Via field contains a private IP address as defined in RFC1918 */
static Bool
testPrivateVia(struct sip_msg* msg)
{
    return isPrivateAddress(&(msg->via1->host));
}


#include "functions.h"

/* The public functions that are exported by this module */

static int
ClientNatTest(struct sip_msg* msg, char* str1, char* str2)
{
    int tests, i;

    tests = (int)(long)str1;

    for (i=0; natTests[i].test!=NTNone; i++) {
        if ((tests & natTests[i].test)!=0 && natTests[i].proc(msg)) {
            return 1;
        }
    }

    return -1; // all failed
}


static int
EndMediaSession(struct sip_msg* msg, char* str1, char* str2)
{
    char *command, *result;
    str callId;

    if (!getCallId(msg, &callId)) {
        LOG(L_ERR, "error: end_media_session(): can't get Call-Id\n");
        return -1;
    }

    command = pkg_malloc(callId.len + 20);
    if (command == NULL) {
        LOG(L_ERR, "error: end_media_session(): out of memory\n");
        return -1;
    }

    sprintf(command, "delete %.*s info=\n", callId.len, callId.s);
    result = sendMediaproxyCommand(command);

    pkg_free(command);

    return result==NULL ? -1 : 1;
}


#define MSG_UNKNOWN 0
#define MSG_INVITE  1
#define MSG_ACK     2
#define MSG_REPLY   3


static int
UseMediaProxy(struct sip_msg* msg, char* str1, char* str2)
{
    str sdp, sessionIP, callId, fromDomain, toDomain, userAgent, tokens[64];
    str fromAddr, toAddr, fromTag, toTag;
    char *clientIP, *ptr, *command, *result, *agent, *fromType, *toType, *info;
    int streamCount, i, port, count, portCount, cmdlen, infolen, success, type;
    StreamInfo streams[64];
    Bool request;

    if (msg->first_line.type == SIP_REQUEST) {
        if (msg->first_line.u.request.method_value == METHOD_INVITE)
            type = MSG_INVITE;
        else if (msg->first_line.u.request.method_value == METHOD_ACK)
            type = MSG_ACK;
        else
            type = MSG_UNKNOWN;
    } else if (msg->first_line.type == SIP_REPLY) {
        type = MSG_REPLY;
    } else {
        type = MSG_UNKNOWN;
    }

    if (type==MSG_INVITE || type==MSG_ACK) {
        request = True;
    } else if (type==MSG_REPLY) {
        request = False;
    } else {
        return -1;
    }

    if (!getCallId(msg, &callId)) {
        LOG(L_ERR, "error: use_media_proxy(): can't get Call-Id\n");
        return -1;
    }

    success = getSDPMessage(msg, &sdp);
    if (success==0 && type==MSG_ACK) {
        return 1; // nothing to do. it's ok for ACK to not have a SDP body
    } else if (success <= 0) {
        LOG(L_ERR, "error: use_media_proxy(): failed to get the SDP message\n");
        return -1;
    }

    if (!getSessionLevelMediaIP(&sdp, &sessionIP)) {
        LOG(L_ERR, "error: use_media_proxy(): error parsing the SDP message\n");
        return -1;
    }

    streamCount = getMediaStreams(&sdp, &sessionIP, streams, 64);
    if (streamCount == -1) {
        LOG(L_ERR, "error: use_media_proxy(): can't extract media streams "
            "from the SDP message\n");
        return -1;
    }

    if (streamCount == 0) {
        // there are no media streams. we have nothing to do.
        return 1;
    }

    fromDomain = getFromDomain(msg);
    fromType   = (isFromLocal(msg, NULL, NULL)>0) ? "local" : "remote";
    fromAddr   = getFromAddress(msg);
    toAddr     = getToAddress(msg);
    fromTag    = getFromTag(msg);
    toTag      = getToTag(msg);
    userAgent  = getUserAgent(msg);
    if (request) {
        toDomain = getDestinationDomain(msg); // call only for requests
        toType = (isDestinationLocal(msg, NULL, NULL)>0) ? "local" : "remote";
    } else {
        toDomain = getToDomain(msg);
        toType = "unknown"; //there's no function to check if To domain is local
    }

    clientIP = ip_addr2a(&msg->rcv.src_ip);

    infolen = fromAddr.len + toAddr.len + fromTag.len + toTag.len + 64;

    cmdlen = callId.len + strlen(clientIP) + fromDomain.len + toDomain.len +
        userAgent.len*3 + infolen + 128;

    for (i=0; i<streamCount; i++) {
        cmdlen += streams[i].port.len + streams[i].ip.len + 2;
    }

    command = pkg_malloc(cmdlen);
    if (!command) {
        LOG(L_ERR, "error: use_media_proxy(): out of memory\n");
        return -1;
    }

    if (request)
        count = sprintf(command, "request %.*s", callId.len, callId.s);
    else
        count = sprintf(command, "lookup %.*s", callId.len, callId.s);

    for (i=0, ptr=command+count; i<streamCount; i++) {
        char c = (i==0 ? ' ' : ',');
        count = sprintf(ptr, "%c%.*s:%.*s", c,
                        streams[i].ip.len, streams[i].ip.s,
                        streams[i].port.len, streams[i].port.s);
        ptr += count;
    }

    agent = encodeQuopri(userAgent);

    info = pkg_malloc(infolen);
    sprintf(info, "from:%.*s,to:%.*s,fromtag:%.*s,totag:%.*s",
            fromAddr.len, fromAddr.s, toAddr.len, toAddr.s,
            fromTag.len, fromTag.s, toTag.len, toTag.s);
    if (isRTPAsymmetric(userAgent)) {
        strcat(info, ",asymmetric");
    }

    snprintf(ptr, command + cmdlen - ptr, " %s %.*s %s %.*s %s %s info=%s\n",
             clientIP, fromDomain.len, fromDomain.s, fromType,
             toDomain.len, toDomain.s, toType, agent, info);

    pkg_free(info);
    pkg_free(agent);

    result = sendMediaproxyCommand(command);

    pkg_free(command);

    if (result == NULL)
        return -1;

    count = getTokens(result, tokens, sizeof(tokens)/sizeof(str));

    if (count == 0) {
        LOG(L_ERR, "error: use_media_proxy(): empty response from mediaproxy\n");
        return -1;
    } else if (count<streamCount+1) {
        if (request) {
            LOG(L_ERR, "error: use_media_proxy(): insufficient ports returned "
                "from mediaproxy: got %d, expected %d\n", count-1, streamCount);
            return -1;
        } else {
            LOG(L_WARN, "warning: use_media_proxy(): broken client. Called UA "
                "added extra media stream(s) in the OK reply\n");
        }
    }

    if (sessionIP.s && !isAnyAddress(sessionIP)) {
        success = replaceElement(msg, &sessionIP, &tokens[0]);
        if (!success) {
            LOG(L_ERR, "error: use_media_proxy(): failed to replace "
                "session-level media IP in SDP body\n");
            return -1;
        }
    }

    portCount = min(count-1, streamCount);

    for (i=0; i<portCount; i++) {
        // check. is this really necessary?
        port = strtoint(&tokens[i+1]);
        if (port <= 0 || port > 65535) {
            LOG(L_ERR, "error: use_media_proxy(): invalid port returned "
                "by mediaproxy: %.*s\n", tokens[i+1].len, tokens[i+1].s);
            //return -1;
            continue;
        }

        if (streams[i].port.len!=1 || streams[i].port.s[0]!='0') {
            success = replaceElement(msg, &(streams[i].port), &tokens[i+1]);
            if (!success) {
                LOG(L_ERR, "error: use_media_proxy(): failed to replace "
                    "port in media stream nr. %d\n", i+1);
                return -1;
            }
        }

        if (streams[i].localIP && !isAnyAddress(streams[i].ip)) {
            success = replaceElement(msg, &(streams[i].ip), &tokens[0]);
            if (!success) {
                LOG(L_ERR, "error: use_media_proxy(): failed to replace "
                    "IP address in media stream nr. %d\n", i+1);
                return -1;
            }
        }
    }

    return 1;
}


/* Module management: initialization/destroy/function-parameter-fixing/... */

static int
mod_init(void)
{
    bind_usrloc_t ul_bind_usrloc;

    isFromLocal = (CheckLocalPartyProc)find_export("is_from_local", 0, 0);
    isDestinationLocal = (CheckLocalPartyProc)find_export("is_uri_host_local", 0, 0);
    if (!isFromLocal || !isDestinationLocal) {
        LOG(L_ERR, "error: mediaproxy/mod_init(): can't find is_from_local "
            "and/or is_uri_host_local functions. Check if domain.so is loaded\n");
        return -1;
    }

    if (natpingInterval > 0) {
        ul_bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
        if (!ul_bind_usrloc) {
            LOG(L_ERR, "error: mediaproxy/mod_init(): can't find the usrloc "
                "module. Check if usrloc.so is loaded.\n");
            return -1;
        }

        if (ul_bind_usrloc(&userLocation) < 0) {
            LOG(L_ERR, "error: mediaproxy/mod_init(): can't access the usrloc module.\n");
            return -1;
        }

        register_timer(pingClients, NULL, natpingInterval);
    }

    checkAsymmetricFile(&sipAsymmetrics);
    checkAsymmetricFile(&rtpAsymmetrics);

    // childs won't benefit from this. figure another way
    //register_timer(checkAsymmetricFiles, NULL, 5);

    return 0;
}


// convert string parameter to integer for functions that expect an integer
static int
fixstring2int(void **param, int param_count)
{
    unsigned long number;
    int err;

    if (param_count == 1) {
        number = str2s(*param, strlen(*param), &err);
        if (err == 0) {
            pkg_free(*param);
            *param = (void*)number;
            return 0;
        } else {
            LOG(L_ERR, "error: mediaproxy/fixstring2int(): bad number `%s'\n",
                (char*)(*param));
            return E_CFG;
        }
    }

    return 0;
}


