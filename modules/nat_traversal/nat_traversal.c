/*
 * Copyright (C) 2007-2009 Dan Pascu
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

/*!
 * \file
 * \brief Module interface and functions
 * \ingroup nat_traversal
 * Module: \ref nat_traversal
 */

/**
 * @defgroup nat_traversal Nat
 * @brief Kamailio nat_traversal module

   The nat_traversal module provides support for handling far-end NAT
   traversal for SIP signaling. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "../../sr_module.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../lock_ops.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../pvar.h"
#include "../../error.h"
#include "../../timer.h"
#include "../../resolve.h"
#include "../../data_lump.h"
#include "../../mod_fix.h"
#include "../../script_cb.h"
#include "../../timer_proc.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_expires.h"
#include "../../parser/contact/parse_contact.h"
#include "../../lib/kcore/statistics.h"
#include "../dialog/dlg_load.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/sl/sl.h"


MODULE_VERSION


#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
# define INLINE inline
#else
# define INLINE
#endif


/* WARNING: Keep this aligned with parser/msg_parser.h! */
#define FL_DO_KEEPALIVE (1<<31)

#define HASH_SIZE    512


#define max(a, b)    ((a)>(b) ? (a) : (b))
#define min(a, b)    ((a)<(b) ? (a) : (b))

#define STR_MATCH(str, buf)  ((str).len==strlen(buf) && memcmp(buf, (str).s, (str).len)==0)
#define STR_IMATCH(str, buf) ((str).len==strlen(buf) && strncasecmp(buf, (str).s, (str).len)==0)

#define STR_MATCH_STR(str, str2)  ((str).len==(str2).len && memcmp((str).s, (str2).s, (str).len)==0)
#define STR_IMATCH_STR(str, str2) ((str).len==(str2).len && strncasecmp((str).s, (str2).s, (str).len)==0)

#define STR_HAS_PREFIX(str, prefix)  ((str).len>(prefix).len && memcmp((prefix).s, (str).s, (prefix).len)==0)
#define STR_HAS_IPREFIX(str, prefix) ((str).len>(prefix).len && strncasecmp((prefix).s, (str).s, (prefix).len)==0)


typedef int Bool;
#define True  1
#define False 0


typedef Bool (*NatTestFunction)(struct sip_msg *msg);

typedef enum {
    NTNone=0,
    NTPrivateContact=1,
    NTSourceAddress=2,
    NTPrivateVia=4
} NatTestType;

typedef struct {
    NatTestType test;
    NatTestFunction proc;
} NatTest;

typedef struct {
    const char *name;
    uint32_t address;
    uint32_t mask;
} NetInfo;


typedef struct SIP_Dialog {
    struct dlg_cell *dlg;
    time_t expire;
    struct SIP_Dialog *next;
} SIP_Dialog;


typedef struct NAT_Contact {
    char *uri;
    struct socket_info *socket;

    time_t registration_expire;
    time_t subscription_expire;
    SIP_Dialog *dialogs;

    struct NAT_Contact *next;
} NAT_Contact;


typedef struct HashSlot {
    NAT_Contact *head;  // pointer to the head of the linked list stored in this slot
    gen_lock_t lock;
} HashSlot;


typedef struct HashTable {
    HashSlot *slots;
    unsigned size;      // table size (number of slots)
} HashTable;


#define URI_LIST_INITIAL_SIZE 8
#define URI_LIST_RESIZE_INCREMENT 8

typedef struct Dialog_Param {
    char *caller_uri;
    char *callee_uri;
    time_t expire;
    Bool confirmed;
    gen_lock_t lock;
    struct {
        char **uri;
        int count;
        int size;
    } callee_candidates;
} Dialog_Param;


// Module parameters
//
typedef struct Keepalive_Params {
    // user specified
    char *method;
    char *from;
    char *extra_headers;

    // internally generated
    char callid_prefix[20];
    unsigned callid_counter;
    unsigned from_tag;
    char *event_header; // this will be set if method is NOTIFY
} Keepalive_Params;


// Function prototypes
//
static int NAT_Keepalive(struct sip_msg *msg);
static int FixContact(struct sip_msg *msg);
static int ClientNatTest(struct sip_msg *msg, unsigned int tests);

static Bool test_private_contact(struct sip_msg *msg);
static Bool test_source_address(struct sip_msg *msg);
static Bool test_private_via(struct sip_msg *msg);

static INLINE char* shm_strdup(char *source);

static int  mod_init(void);
static int  child_init(int rank);
static void mod_destroy(void);
static int  preprocess_request(struct sip_msg *msg, unsigned int flags, void *param);
static int  reply_filter(struct sip_msg *reply);

static int pv_parse_nat_contact_name(pv_spec_p sp, str *in);
static int pv_get_keepalive_socket(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int pv_get_source_uri(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);


// Module global variables and state
//
static HashTable *nat_table = NULL;

static Bool keepalive_disabled = False;

static unsigned int keepalive_interval = 60;

static char *keepalive_state_file = "keepalive_state";

static Keepalive_Params keepalive_params = {"NOTIFY", NULL, "", "", 0, 0, ""};

struct tm_binds  tm_api;
struct dlg_binds dlg_api;
Bool have_dlg_api = False;

static int dialog_flag = -1;
static unsigned dialog_default_timeout = 12*3600;  // 12 hours

stat_var *keepalive_endpoints = 0;
stat_var *registered_endpoints = 0;
stat_var *subscribed_endpoints = 0;
stat_var *dialog_endpoints = 0;

static NetInfo rfc1918nets[] = {
    {"10.0.0.0",    0x0a000000UL, 0xff000000UL},
    {"172.16.0.0",  0xac100000UL, 0xfff00000UL},
    {"192.168.0.0", 0xc0a80000UL, 0xffff0000UL},
    {"100.64.0.0",  0x64400000UL, 0xffc00000UL}, // include rfc6598 shared address space as technically the same for our purpose
    {NULL,          0UL,          0UL}
};

static NatTest NAT_Tests[] = {
    {NTPrivateContact, test_private_contact},
    {NTSourceAddress,  test_source_address},
    {NTPrivateVia,     test_private_via},
    {NTNone,           NULL}
};

/** SL API structure */
sl_api_t slb;

static cmd_export_t commands[] = {
    {"nat_keepalive",   (cmd_function)NAT_Keepalive, 0, NULL, 0, REQUEST_ROUTE},
    {"fix_contact",     (cmd_function)FixContact,    0, NULL, 0, REQUEST_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE |LOCAL_ROUTE},
    {"client_nat_test", (cmd_function)ClientNatTest, 1, fixup_uint_null, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE|LOCAL_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

static param_export_t parameters[] = {
    {"keepalive_interval",       INT_PARAM, &keepalive_interval},
    {"keepalive_method",         PARAM_STRING, &keepalive_params.method},
    {"keepalive_from",           PARAM_STRING, &keepalive_params.from},
    {"keepalive_extra_headers",  PARAM_STRING, &keepalive_params.extra_headers},
    {"keepalive_state_file",     PARAM_STRING, &keepalive_state_file},
    {0, 0, 0}
};

static pv_export_t pvars[] = {
    {str_init("keepalive.socket"), PVT_OTHER, pv_get_keepalive_socket, NULL, pv_parse_nat_contact_name, NULL, NULL, 0},
    {str_init("source_uri"), PVT_OTHER, pv_get_source_uri, NULL, NULL, NULL, NULL, 0},
    {{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

#ifdef STATISTICS
static stat_export_t statistics[] = {
    {"keepalive_endpoints",  STAT_NO_RESET, &keepalive_endpoints},
    {"registered_endpoints", STAT_NO_RESET, &registered_endpoints},
    {"subscribed_endpoints", STAT_NO_RESET, &subscribed_endpoints},
    {"dialog_endpoints",     STAT_NO_RESET, &dialog_endpoints},
    {0, 0, 0}
};
#endif

struct module_exports exports = {
    "nat_traversal", // module name
    DEFAULT_DLFLAGS, // dlopen flags
    commands,        // exported functions
    parameters,      // exported parameters
    NULL,            // exported statistics (initialized early in mod_init)
    NULL,            // exported MI functions
    pvars,           // exported pseudo-variables
    NULL,            // extra processes
    mod_init,        // module init function (before fork. kids will inherit)
    reply_filter,    // reply processing function
    mod_destroy,     // destroy function
    child_init       // child init function
};



// SIP_Dialog structure handling functions
//

static SIP_Dialog*
SIP_Dialog_new(struct dlg_cell *dlg, time_t expire)
{
    SIP_Dialog *dialog;

    dialog = (SIP_Dialog*)shm_malloc(sizeof(SIP_Dialog));
    if (!dialog) {
        LM_ERR("out of memory while creating new SIP_Dialog structure\n");
        return NULL;
    }
    dialog->dlg = dlg;
    dialog->expire = expire;
    dialog->next = NULL;

    // we assume expire is always strictly positive on new dialogs
    update_stat(dialog_endpoints, 1);

    return dialog;
}


static void
SIP_Dialog_del(SIP_Dialog *dialog)
{
    if (!dialog)
        return;

    if (dialog->expire > 0)
        update_stat(dialog_endpoints, -1);
    shm_free(dialog);
}


// Purge expired dialogs from the linked list pointed by dialog
//
static SIP_Dialog*
SIP_Dialog_purge_expired(SIP_Dialog *dialog, time_t now)
{
    SIP_Dialog *next;

    if (dialog==NULL)
        return NULL;

    dialog->next = SIP_Dialog_purge_expired(dialog->next, now);

    if (now > dialog->expire) {
        next = dialog->next;
        SIP_Dialog_del(dialog);
        return next;
    }

    return dialog;
}


static INLINE void
SIP_Dialog_end(SIP_Dialog *dialog)
{
    if (dialog->expire > 0) {
        dialog->expire = 0;
        update_stat(dialog_endpoints, -1);
    }
}


// Helpers to handle registration and subscription timeouts for NAT_Contacts
//
static INLINE void
SIP_Registration_update(NAT_Contact *contact, time_t expire)
{
    if (expire > contact->registration_expire) {
        if (contact->registration_expire == 0)
            update_stat(registered_endpoints, 1);
        contact->registration_expire = expire;
    }
}

static INLINE void
SIP_Registration_expire(NAT_Contact *contact, time_t now)
{
    if (contact->registration_expire && now > contact->registration_expire) {
        update_stat(registered_endpoints, -1);
        contact->registration_expire = 0;
    }
}

static INLINE void
SIP_Subscription_update(NAT_Contact *contact, time_t expire)
{
    if (expire > contact->subscription_expire) {
        if (contact->subscription_expire == 0)
            update_stat(subscribed_endpoints, 1);
        contact->subscription_expire = expire;
    }
}

static INLINE void
SIP_Subscription_expire(NAT_Contact *contact, time_t now)
{
    if (contact->subscription_expire && now > contact->subscription_expire) {
        update_stat(subscribed_endpoints, -1);
        contact->subscription_expire = 0;
    }
}


// NAT_Contact structure handling functions
//

static NAT_Contact*
NAT_Contact_new(char *uri, struct socket_info *socket)
{
    NAT_Contact *contact;

    contact = (NAT_Contact*)shm_malloc(sizeof(NAT_Contact));
    if (!contact) {
        LM_ERR("out of memory while creating new NAT_Contact structure\n");
        return NULL;
    }
    memset(contact, 0, sizeof(NAT_Contact));

    contact->uri = shm_strdup(uri);
    if (!contact->uri) {
        LM_ERR("out of memory while creating new NAT_Contact structure\n");
        shm_free(contact);
        return NULL;
    }
    contact->socket = socket;

    update_stat(keepalive_endpoints, 1);

    return contact;
}


static void
NAT_Contact_del(NAT_Contact *contact)
{
    SIP_Dialog *dialog, *next;

    if (!contact)
        return;

    dialog = contact->dialogs;
    while (dialog) {
        next = dialog->next;
        SIP_Dialog_del(dialog);
        dialog = next;
    }

    if (contact->registration_expire > 0)
        update_stat(registered_endpoints, -1);
    if (contact->subscription_expire > 0)
        update_stat(subscribed_endpoints, -1);
    update_stat(keepalive_endpoints, -1);

    shm_free(contact->uri);
    shm_free(contact);
}


static Bool
NAT_Contact_match(NAT_Contact *contact, const char *uri)
{
    return strcmp(contact->uri, uri)==0;
}


static SIP_Dialog*
NAT_Contact_get_dialog(NAT_Contact *contact, struct dlg_cell *dlg)
{
    SIP_Dialog *dialog;

    dialog = contact->dialogs;

    while (dialog) {
        if (dialog->dlg == dlg)
            break;
        dialog = dialog->next;
    }

    return dialog;
}


static NAT_Contact*
NAT_Contact_purge_expired(NAT_Contact *contact, time_t now)
{
    NAT_Contact *next;

    if (contact==NULL)
        return NULL;

    contact->next = NAT_Contact_purge_expired(contact->next, now);

    SIP_Registration_expire(contact, now);
    SIP_Subscription_expire(contact, now);
    contact->dialogs = SIP_Dialog_purge_expired(contact->dialogs, now);

    if (!contact->registration_expire && !contact->subscription_expire && !contact->dialogs) {
        next = contact->next;
        NAT_Contact_del(contact);
        return next;
    }

    return contact;
}


// HashTable structure manipulation
//

#define HASH(table, key)  (hash_string(key) % (table)->size)

static INLINE unsigned
hash_string(const char *key)
{
    register unsigned ret = 0;
    register unsigned ctr = 0;

    while (*key) {
        ret ^= *(char*)key++ << ctr;
        ctr = (ctr + 1) % sizeof (char *);
    }

    return ret;
}


static HashTable*
HashTable_new(void)
{
    HashTable *table;
    int i, j;

    table = shm_malloc(sizeof(HashTable));
    if (!table) {
        LM_ERR("cannot allocate shared memory for hash table\n");
        return NULL;
    }
    memset(table, 0, sizeof(HashTable));

    table->size = HASH_SIZE;

    table->slots = shm_malloc(sizeof(HashSlot)*table->size);
    if (!table->slots) {
        LM_ERR("cannot allocate shared memory for hash table\n");
        shm_free(table);
        return NULL;
    }
    memset(table->slots, 0, sizeof(HashSlot)*table->size);

    for (i=0; i<table->size; i++) {
        if (!lock_init(&table->slots[i].lock)) {
            LM_ERR("cannot initialize hash table locks\n");
            for (j=0; j<i; j++)
                lock_destroy(&table->slots[j].lock);
            shm_free(table->slots);
            shm_free(table);
            return NULL;
        }
    }

    return table;
}


static void
HashTable_del(HashTable *table)
{
    NAT_Contact *contact, *next;
    int i;

    for (i=0; i < table->size; i++) {
        lock_get(&table->slots[i].lock);
        contact = table->slots[i].head;
        while (contact) {
            next = contact->next;
            NAT_Contact_del(contact);
            contact = next;
        }
        table->slots[i].head = NULL;
        lock_release(&table->slots[i].lock);
    }

    shm_free(table->slots);
    shm_free(table);
}


// This function assumes that the caller has locked the slot already
//
static NAT_Contact*
HashTable_search(HashTable *table, char *uri, unsigned slot)
{
    NAT_Contact *contact;

    contact = table->slots[slot].head;

    while (contact) {
        if (NAT_Contact_match(contact, uri))
            break;
        contact = contact->next;
    }

    return contact;
}


// Dialog_Param structure handling functions
//

static Dialog_Param*
Dialog_Param_new(void)
{
    Dialog_Param *param;

    param = shm_malloc(sizeof(Dialog_Param));
    if (!param) {
        LM_ERR("cannot allocate shared memory for dialog callback param\n");
        return NULL;
    }
    memset(param, 0, sizeof(Dialog_Param));

    param->callee_candidates.uri = shm_malloc(sizeof(char*) * URI_LIST_INITIAL_SIZE);
    if (!param->callee_candidates.uri) {
        LM_ERR("cannot allocate shared memory for callee_candidates uri list\n");
        shm_free(param);
        return NULL;
    }
    memset(param->callee_candidates.uri, 0, sizeof(char*) * URI_LIST_INITIAL_SIZE);
    param->callee_candidates.size = URI_LIST_INITIAL_SIZE;

    param->expire = time(NULL) + dialog_default_timeout;

    if (!lock_init(&param->lock)) {
        LM_ERR("cannot initialize dialog param structure lock\n");
        shm_free(param->callee_candidates.uri);
        shm_free(param);
        return NULL;
    }

    return param;
}


static void
Dialog_Param_del(Dialog_Param *param)
{
    int i;

    if (!param)
        return;

    lock_destroy(&param->lock);

    if (param->caller_uri)
        shm_free(param->caller_uri);
    if (param->callee_uri)
        shm_free(param->callee_uri);
    for (i=0; i<param->callee_candidates.count; i++)
        shm_free(param->callee_candidates.uri[i]);
    shm_free(param->callee_candidates.uri);
    shm_free(param);
}


// This function assumes the caller has locked the Dialog_Param while operating on it
//
static Bool
Dialog_Param_has_candidate(Dialog_Param *param, char *candidate)
{
    int i;

    for (i=0; i<param->callee_candidates.count; i++) {
        if (strcmp(candidate, param->callee_candidates.uri[i])==0) {
            return True;
        }
    }

    return False;
}


// This function assumes the caller has locked the Dialog_Param while operating on it
//
static Bool
Dialog_Param_add_candidate(Dialog_Param *param, char *candidate)
{
    char **new_uri, *new_candidate;
    int new_size;

    if (param->callee_candidates.count == param->callee_candidates.size) {
        new_size = param->callee_candidates.size + URI_LIST_RESIZE_INCREMENT;
        LM_DBG("growing callee_candidates list size from %d to %d entries\n", param->callee_candidates.size, new_size);
        new_uri = shm_realloc(param->callee_candidates.uri, new_size * sizeof(char*));
        if (!new_uri) {
            LM_ERR("failed to grow callee_candidates uri list\n");
            return False;
        }
        param->callee_candidates.uri = new_uri;
        param->callee_candidates.size = new_size;
    }

    new_candidate = shm_strdup(candidate);
    if (!new_candidate) {
        LM_ERR("cannot allocate shared memory for new candidate uri\n");
        return False;
    }

    param->callee_candidates.uri[param->callee_candidates.count] = new_candidate;
    param->callee_candidates.count++;

    return True;
}


// Miscellaneous helper functions
//

// returns str with leading whitespace removed
static INLINE void
ltrim(str *string)
{
    while (string->len>0 && isspace((int)*(string->s))) {
        string->len--;
        string->s++;
    }
}

// returns str with trailing whitespace removed
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

// returns str with leading and trailing whitespace removed
static INLINE void
trim(str *string)
{
    ltrim(string);
    rtrim(string);
}


static INLINE char*
shm_strdup(char *source)
{
    char *copy;

    if (!source)
        return NULL;

    copy = (char*)shm_malloc(strlen(source) + 1);
    if (!copy)
        return NULL;
    strcpy(copy, source);

    return copy;
}


static Bool
get_contact_uri(struct sip_msg* msg, struct sip_uri *uri, contact_t **_c)
{

    if ((parse_headers(msg, HDR_CONTACT_F, 0) == -1) || !msg->contact)
        return False;

    if (!msg->contact->parsed && parse_contact(msg->contact) < 0) {
        LM_ERR("cannot parse the Contact header\n");
        return False;
    }

    *_c = ((contact_body_t*)msg->contact->parsed)->contacts;

    if (*_c == NULL) {
        return False;
    }

    if (parse_uri((*_c)->uri.s, (*_c)->uri.len, uri) < 0 || uri->host.len <= 0) {
        LM_ERR("cannot parse the Contact URI\n");
        return False;
    }

    return True;
}


#define is_private_address(x) (rfc1918address(x)==1 ? 1 : 0)

// Test if IP in `address' belongs to a RFC1918 network
static INLINE int
rfc1918address(str *address)
{
    struct ip_addr *ip;
    uint32_t netaddr;
    int i;

    ip = str2ip(address);
    if (ip == NULL)
        return -1; // invalid address to test

    netaddr = ntohl(ip->u.addr32[0]);

    for (i=0; rfc1918nets[i].name!=NULL; i++) {
        if ((netaddr & rfc1918nets[i].mask)==rfc1918nets[i].address) {
            return 1;
        }
    }

    return 0;
}


// Test if address of signaling is different from address in 1st Via field
static Bool
test_source_address(struct sip_msg *msg)
{
    Bool different_ip, different_port;
    int via1_port;

    different_ip = received_via_test(msg);
    via1_port = (msg->via1->port ? msg->via1->port : SIP_PORT);
    different_port = (msg->rcv.src_port != via1_port);

    return (different_ip || different_port);
}


// Test if Contact field contains a private IP address as defined in RFC1918
static Bool
test_private_contact(struct sip_msg *msg)
{
    struct sip_uri uri;
    contact_t* contact;

    if (!get_contact_uri(msg, &uri, &contact))
        return False;

    return is_private_address(&(uri.host));
}


// Test if top Via field contains a private IP address as defined in RFC1918
static Bool
test_private_via(struct sip_msg *msg)
{
    return is_private_address(&(msg->via1->host));
}


// return the Expires header value (converted to an UNIX timestamp if > 0)
static int
get_expires(struct sip_msg *msg)
{
    exp_body_t *expires;

    if (parse_headers(msg, HDR_EXPIRES_F, 0) < 0) {
        LM_ERR("failed to parse the Expires header\n");
        return 0;
    }
    if (!msg->expires)
        return 0;

    if (parse_expires(msg->expires) < 0) {
        LM_ERR("failed to parse the Expires header body\n");
        return 0;
    }

    expires = (exp_body_t*)msg->expires->parsed;

    return ((expires->valid && expires->val) ? expires->val + time(NULL) : 0);
}


// return the highest expire value from all registered contacts in the request
static time_t
get_register_expire(struct sip_msg *request, struct sip_msg *reply)
{
    struct hdr_field contact_hdr, *hdr, *r_hdr;
    contact_body_t *contact_body, *r_contact_body;
    contact_t *contact, *r_contact;
    param_t *expires_param;
    time_t now, expire=0;
    unsigned exp;
    Bool matched;

    if (!request->contact)
        return 0;

    if (parse_headers(reply, HDR_EOH_F, 0) < 0) {
        LM_ERR("failed to parse headers for REGISTER reply\n");
        return 0;
    }

    if (!reply->contact)
        return 0;

    now = time(NULL);

    // request may be R/O (if we are called from the TM callback),
    // thus we copy the hdr_field structures before parsing them

    for (hdr=request->contact; hdr; hdr = next_sibling_hdr(hdr)) {
        if (!hdr->parsed) {
            memcpy(&contact_hdr, hdr, sizeof(struct hdr_field));
            if (parse_contact(&contact_hdr) < 0) {
                LM_ERR("failed to parse the Contact header body\n");
                continue;
            }
            contact_body = (contact_body_t*)contact_hdr.parsed;
        } else {
            contact_body = (contact_body_t*)hdr->parsed;
        }

        if (contact_body->star) {
            if (!hdr->parsed)
                clean_hdr_field(&contact_hdr);
            return 0;
        }

        for (contact=contact_body->contacts; contact; contact=contact->next) {
            for (r_hdr=reply->contact, matched=False; r_hdr && !matched; r_hdr=next_sibling_hdr(r_hdr)) {
                if (!r_hdr->parsed && parse_contact(r_hdr) < 0) {
                    LM_ERR("failed to parse the Contact header body in reply\n");
                    continue;
                }
                r_contact_body = (contact_body_t*)r_hdr->parsed;
                for (r_contact=r_contact_body->contacts; r_contact; r_contact=r_contact->next) {
                    if (STR_MATCH_STR(contact->uri, r_contact->uri)) {
                        expires_param = r_contact->expires;
                        if (expires_param && expires_param->body.len && str2int(&expires_param->body, &exp) == 0)
                            expire = max(expire, exp);
                        matched = True;
                        break;
                    }
                }
            }
        }

        if (!hdr->parsed) {
            clean_hdr_field(&contact_hdr);
        }
    }

    LM_DBG("maximum expire for all contacts: %u\n", (unsigned)expire);

    return (expire ? expire + now : 0);
}


static char*
get_source_uri(struct sip_msg *msg)
{
    static char uri[64];
    snprintf(uri, 64, "sip:%s:%d", ip_addr2a(&msg->rcv.src_ip), msg->rcv.src_port);
    return uri;
}


static void
keepalive_registration(struct sip_msg *request, time_t expire)
{
    NAT_Contact *contact;
    unsigned h;
    char *uri;

    uri = get_source_uri(request);

    h = HASH(nat_table, uri);
    lock_get(&nat_table->slots[h].lock);

    contact = HashTable_search(nat_table, uri, h);
    if (contact) {
        SIP_Registration_update(contact, expire);
    } else {
        contact = NAT_Contact_new(uri, request->rcv.bind_address);
        if (contact) {
            SIP_Registration_update(contact, expire);
            contact->next = nat_table->slots[h].head;
            nat_table->slots[h].head = contact;
        } else {
            LM_ERR("cannot allocate shared memory for new NAT contact\n");
        }
    }

    lock_release(&nat_table->slots[h].lock);
}


static void
keepalive_subscription(struct sip_msg *request, time_t expire)
{
    NAT_Contact *contact;
    unsigned h;
    char *uri;

    uri = get_source_uri(request);

    h = HASH(nat_table, uri);
    lock_get(&nat_table->slots[h].lock);

    contact = HashTable_search(nat_table, uri, h);
    if (contact) {
        SIP_Subscription_update(contact, expire);
    } else {
        contact = NAT_Contact_new(uri, request->rcv.bind_address);
        if (contact) {
            SIP_Subscription_update(contact, expire);
            contact->next = nat_table->slots[h].head;
            nat_table->slots[h].head = contact;
        } else {
            LM_ERR("cannot allocate shared memory for new NAT contact\n");
        }
    }

    lock_release(&nat_table->slots[h].lock);
}


static void
__dialog_early(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    Dialog_Param *param = (Dialog_Param*)*_params->param;
    NAT_Contact *contact;
    SIP_Dialog *dialog;
    unsigned h;
    char *uri;

    lock_get(&param->lock);

    if (param->confirmed) {
        // this 1xx is late; dialog already confirmed by 200 OK; ignore it
        lock_release(&param->lock);
        return;
    }

    uri = get_source_uri(_params->rpl);
    if (!Dialog_Param_has_candidate(param, uri)) {
        if (!Dialog_Param_add_candidate(param, uri)) {
            LM_ERR("cannot add callee candidate uri to the list\n");
        } else {
            h = HASH(nat_table, uri);
            lock_get(&nat_table->slots[h].lock);

            contact = HashTable_search(nat_table, uri, h);
            if (contact) {
                dialog = NAT_Contact_get_dialog(contact, dlg);
                if (!dialog) {
                    dialog = SIP_Dialog_new(dlg, param->expire);
                    if (dialog) {
                        dialog->next = contact->dialogs;
                        contact->dialogs = dialog;
                    } else {
                        LM_ERR("cannot allocate shared memory for new SIP dialog\n");
                    }
                }
            }

            lock_release(&nat_table->slots[h].lock);
        }
    }

    lock_release(&param->lock);
}


static void
__dialog_confirmed(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    Dialog_Param *param = (Dialog_Param*)*_params->param;
    NAT_Contact *contact;
    SIP_Dialog *dialog;
    char *callee_uri, *uri;
    unsigned h;
    int i;

    lock_get(&param->lock);

    param->confirmed = True;

    callee_uri = get_source_uri(_params->rpl);

    // remove all keepalives on unanswered branches
    for (i=0; i<param->callee_candidates.count; i++) {
        uri = param->callee_candidates.uri[i];

        if (strcmp(uri, callee_uri) != 0) {
            // this is an unanswered branch
            h = HASH(nat_table, uri);
            lock_get(&nat_table->slots[h].lock);

            contact = HashTable_search(nat_table, uri, h);
            if (contact) {
                dialog = NAT_Contact_get_dialog(contact, dlg);
                if (dialog) {
                    SIP_Dialog_end(dialog);
                }
            }

            lock_release(&nat_table->slots[h].lock);
        }

        shm_free(param->callee_candidates.uri[i]);
        param->callee_candidates.uri[i] = NULL;
    }

    param->callee_candidates.count = 0;

    // add dialog keepalive for answered branch, if needed and not already there
    h = HASH(nat_table, callee_uri);
    lock_get(&nat_table->slots[h].lock);

    contact = HashTable_search(nat_table, callee_uri, h);
    if (contact) {
        dialog = NAT_Contact_get_dialog(contact, dlg);
        if (!dialog) {
            dialog = SIP_Dialog_new(dlg, param->expire);
            if (dialog) {
                dialog->next = contact->dialogs;
                contact->dialogs = dialog;
            } else {
                LM_ERR("cannot allocate shared memory for new SIP dialog\n");
            }
        }
        // free old uri in case this callback is called more than once (shouldn't normally happen)
        if (param->callee_uri)
            shm_free(param->callee_uri);
        param->callee_uri = shm_strdup(callee_uri);
        if (!param->callee_uri) {
            LM_ERR("cannot allocate shared memory for callee_uri in dialog param\n");
        }
    }

    lock_release(&nat_table->slots[h].lock);

    lock_release(&param->lock);
}


static void
__dialog_destroy(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    Dialog_Param *param = (Dialog_Param*)*_params->param;
    NAT_Contact *contact;
    SIP_Dialog *dialog;
    unsigned h;
    int i;

    if (!param)
        return;

    // If nat_table is NULL, it's because it was already removed during
    // shutdown by mod_destroy. However we can still receive dialog destroy
    // notifications when the dialog module removes dialogs on shutdown.
    if (!nat_table) {
        Dialog_Param_del(param);
        *_params->param = NULL;
        return;
    }

    if (param->caller_uri) {
        h = HASH(nat_table, param->caller_uri);
        lock_get(&nat_table->slots[h].lock);

        contact = HashTable_search(nat_table, param->caller_uri, h);
        if (contact) {
            dialog = NAT_Contact_get_dialog(contact, dlg);
            if (dialog) {
                SIP_Dialog_end(dialog);
            }
        }

        lock_release(&nat_table->slots[h].lock);
    }

    if (param->callee_uri) {
        h = HASH(nat_table, param->callee_uri);
        lock_get(&nat_table->slots[h].lock);

        contact = HashTable_search(nat_table, param->callee_uri, h);
        if (contact) {
            dialog = NAT_Contact_get_dialog(contact, dlg);
            if (dialog) {
                SIP_Dialog_end(dialog);
            }
        }

        lock_release(&nat_table->slots[h].lock);
    }

    lock_get(&param->lock);

    // remove all keepalives on unanswered branches. this is neded because
    // we may transit from early to ended without going through confirmed
    for (i=0; i<param->callee_candidates.count; i++) {
        h = HASH(nat_table, param->callee_candidates.uri[i]);
        lock_get(&nat_table->slots[h].lock);

        contact = HashTable_search(nat_table, param->callee_candidates.uri[i], h);
        if (contact) {
            dialog = NAT_Contact_get_dialog(contact, dlg);
            if (dialog) {
                SIP_Dialog_end(dialog);
            }
        }

        lock_release(&nat_table->slots[h].lock);

        shm_free(param->callee_candidates.uri[i]);
        param->callee_candidates.uri[i] = NULL;
    }

    param->callee_candidates.count = 0;

    lock_release(&param->lock);

    Dialog_Param_del(param);

    *_params->param = NULL;
}


static void
__dialog_created(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
    struct sip_msg *request = _params->req;
    NAT_Contact *contact;
    SIP_Dialog *dialog;
    Dialog_Param *param;
    unsigned h;
    char *uri;

    if (request->REQ_METHOD != METHOD_INVITE)
        return;

    param = Dialog_Param_new();
    if (!param) {
        LM_ERR("cannot create dialog callback param\n");
        return;
    }

    if (dlg_api.register_dlgcb(dlg, DLGCB_DESTROY, __dialog_destroy, param, NULL) != 0) {
        LM_ERR("cannot register callback for dialog destruction\n");
        Dialog_Param_del(param);
        return;
    }

    if (dlg_api.register_dlgcb(dlg, DLGCB_EARLY, __dialog_early, param, NULL) != 0)
        LM_ERR("cannot register callback for dialog early replies\n");
    if (dlg_api.register_dlgcb(dlg, DLGCB_CONFIRMED_NA, __dialog_confirmed, param, NULL) != 0)
        LM_ERR("cannot register callback for dialog confirmation\n");

    if ((request->msg_flags & FL_DO_KEEPALIVE) == 0)
        return;

    uri = get_source_uri(request);
    param->caller_uri = shm_strdup(uri);
    if (!param->caller_uri) {
        LM_ERR("cannot allocate shared memory for caller_uri in dialog param\n");
        return;
    }

    h = HASH(nat_table, uri);
    lock_get(&nat_table->slots[h].lock);

    contact = HashTable_search(nat_table, uri, h);
    if (contact) {
        dialog = SIP_Dialog_new(dlg, param->expire);
        if (dialog) {
            dialog->next = contact->dialogs;
            contact->dialogs = dialog;
        } else {
            LM_ERR("cannot allocate shared memory for new SIP dialog\n");
        }
    } else {
        contact = NAT_Contact_new(uri, request->rcv.bind_address);
        if (contact) {
            contact->dialogs = SIP_Dialog_new(dlg, param->expire);
            if (contact->dialogs) {
                contact->next = nat_table->slots[h].head;
                nat_table->slots[h].head = contact;
            } else {
                LM_ERR("cannot allocate shared memory for new SIP dialog\n");
                NAT_Contact_del(contact);
            }
        } else {
            LM_ERR("cannot allocate shared memory for new NAT contact\n");
        }
    }

    lock_release(&nat_table->slots[h].lock);
}


// callback to handle all SL generated replies
//
static void
__sl_reply_out(sl_cbp_t *slcbp)
{
    struct sip_msg reply;
    struct sip_msg *request;
    time_t expire;

	request = slcbp->req;
    if (request->REQ_METHOD == METHOD_INVITE)
        return;

    if ((request->msg_flags & FL_DO_KEEPALIVE) == 0)
        return;

    if (slcbp->code >= 200 && slcbp->code < 300) {
        memset(&reply, 0, sizeof(struct sip_msg));
        reply.buf = slcbp->reply->s;
        reply.len = slcbp->reply->len;

        if (parse_msg(reply.buf, reply.len, &reply) != 0) {
            LM_ERR("cannot parse outgoing SL reply for keepalive"
					" information\n");
            return;
        }

        switch (request->REQ_METHOD) {
        case METHOD_SUBSCRIBE:
            expire = get_expires(&reply);
            if (expire > 0)
                keepalive_subscription(request, expire);
            break;
        case METHOD_REGISTER:
            expire = get_register_expire(request, &reply);
            if (expire > 0)
                keepalive_registration(request, expire);
            break;
        default:
            LM_ERR("called with keepalive flag set for unsupported method\n");
            break;
        }

        free_sip_msg(&reply);
    }
}


// callback to handle incoming replies for the request's transactions
//
static void
__tm_reply_in(struct cell *trans, int type, struct tmcb_params *param)
{
    time_t expire;

    if (param->req==NULL || param->rpl==NULL)
        return;

    if (param->code >= 200 && param->code < 300) {
        switch (param->req->REQ_METHOD) {
        case METHOD_SUBSCRIBE:
            expire = get_expires(param->rpl);
            if (expire > 0)
                keepalive_subscription(param->req, expire);
            break;
        case METHOD_REGISTER:
            expire = get_register_expire(param->req, param->rpl);
            if (expire > 0)
                keepalive_registration(param->req, expire);
            break;
        }
    }
}


// Keepalive NAT for an UA while it has registered contacts or active dialogs
//
static int
NAT_Keepalive(struct sip_msg *msg)
{

    if (keepalive_disabled)
        return -1;

    // keepalive is only supported for UDP dialogs
    if (msg->rcv.proto!=PROTO_UDP)
        return -1;

    switch (msg->REQ_METHOD) {

    case METHOD_REGISTER:
        // make the expires & contact headers available later in the TM cloned msg
        if (parse_headers(msg, HDR_EOH_F, 0) < 0) {
            LM_ERR("failed to parse headers in REGISTER request\n");
            return -1;
        }
        // fallthrough
    case METHOD_SUBSCRIBE:
        msg->msg_flags |= FL_DO_KEEPALIVE;
        if (tm_api.register_tmcb(msg, 0, TMCB_RESPONSE_IN, __tm_reply_in, 0, 0) <= 0) {
            LM_ERR("cannot register TM callback for incoming replies\n");
            return -1;
        }
        return 1;

    case METHOD_INVITE:
        if (!have_dlg_api) {
            LM_ERR("cannot keep alive dialog without the dialog module being loaded\n");
            return -1;
        }
        msg->msg_flags |= FL_DO_KEEPALIVE;
        setflag(msg, dialog_flag); // have the dialog module trace this dialog
        return 1;

    default:
        LM_ERR("unsupported method for keepalive\n");
        return -1;
    }

}


// Replace IP:Port in Contact field with the source address of the packet.
static int
FixContact(struct sip_msg *msg)
{
    str before_host, after, newip;
    unsigned short port, newport;
    contact_t* contact;
    struct lump* anchor;
    struct sip_uri uri;
    int len, offset;
    char *buf;

    if (!get_contact_uri(msg, &uri, &contact))
        return -1;

    newip.s = ip_addr2a(&msg->rcv.src_ip);
    newip.len = strlen(newip.s);
    newport = msg->rcv.src_port;

    port = uri.port_no ? uri.port_no : 5060;

    // Don't do anything if the address is the same, just return success.
    if (STR_MATCH_STR(uri.host, newip) && port==newport)
        return 1;

    if (uri.port.len == 0)
        uri.port.s = uri.host.s + uri.host.len;

    before_host.s   = contact->uri.s;
    before_host.len = uri.host.s - contact->uri.s;
    after.s   = uri.port.s + uri.port.len;
    after.len = contact->uri.s + contact->uri.len - after.s;

    len = before_host.len + newip.len + after.len + 20;

    // first try to alloc mem. if we fail we don't want to have the lump
    // deleted and not replaced. at least this way we keep the original.
    buf = pkg_malloc(len);
    if (buf == NULL) {
        LM_ERR("out of memory\n");
        return -1;
    }

    offset = contact->uri.s - msg->buf;
    anchor = del_lump(msg, offset, contact->uri.len, (enum _hdr_types_t)HDR_CONTACT_F);

    if (!anchor) {
        pkg_free(buf);
        return -1;
    }

    len = sprintf(buf, "%.*s%s:%d%.*s", before_host.len, before_host.s,
                  newip.s, newport, after.len, after.s);

    if (insert_new_lump_after(anchor, buf, len, (enum _hdr_types_t)HDR_CONTACT_F) == 0) {
        pkg_free(buf);
        return -1;
    }

    contact->uri.s   = buf;
    contact->uri.len = len;

    return 1;
}


static int
ClientNatTest(struct sip_msg *msg, unsigned int tests)
{
    int i;

    for (i=0; NAT_Tests[i].test!=NTNone; i++) {
        if ((tests & NAT_Tests[i].test)!=0 && NAT_Tests[i].proc(msg)) {
            return 1;
        }
    }

    return -1; // all failed
}


#define FROM_PREFIX "sip:keepalive@"

static void
send_keepalive(NAT_Contact *contact)
{
    char buffer[8192], *from_uri, *ptr;
    static char from[64] = FROM_PREFIX;
    static char *from_ip = from + sizeof(FROM_PREFIX) - 1;
    static struct socket_info *last_socket = NULL;
    struct hostent* hostent;
	struct dest_info dst;
    int nat_port, len;
    str nat_ip;
	unsigned short lport;
	char lproto;

    if (keepalive_params.from == NULL) {
        if (contact->socket != last_socket) {
            memcpy(from_ip, contact->socket->address_str.s, contact->socket->address_str.len);
            from_ip[contact->socket->address_str.len] = 0;
            last_socket = contact->socket;
        }
        from_uri = from;
    } else {
        from_uri = keepalive_params.from;
    }

    len = snprintf(buffer, sizeof(buffer),
                   "%s %s SIP/2.0\r\n"
                   "Via: SIP/2.0/UDP %.*s:%d;branch=0\r\n"
                   "From: %s;tag=%x\r\n"
                   "To: %s\r\n"
                   "Call-ID: %s-%x-%x@%.*s\r\n"
                   "CSeq: 1 %s\r\n"
                   "%s%s"
                   "Content-Length: 0\r\n\r\n",
                   keepalive_params.method, contact->uri,
                   contact->socket->address_str.len,
                   contact->socket->address_str.s, contact->socket->port_no,
                   from_uri, keepalive_params.from_tag++,
                   contact->uri, keepalive_params.callid_prefix,
                   keepalive_params.callid_counter++, get_ticks(),
                   contact->socket->address_str.len,
                   contact->socket->address_str.s,
                   keepalive_params.method,
                   keepalive_params.event_header,
                   keepalive_params.extra_headers);

    if (len >= sizeof(buffer)) {
        LM_ERR("keepalive message is longer than %lu bytes\n", (unsigned long)sizeof(buffer));
        return;
    }

	init_dest_info(&dst);
    //nat_ip.s = strchr(contact->uri, ':') + 1;
    nat_ip.s = &contact->uri[4]; // skip over "sip:"
    ptr = strchr(nat_ip.s, ':');
    nat_ip.len = ptr - nat_ip.s;
    nat_port = strtol(ptr+1, NULL, 10);
    lport = 0;
    lproto = PROTO_NONE;
    hostent = sip_resolvehost(&nat_ip, &lport, &lproto);
    hostent2su(&dst.to, hostent, 0, nat_port);
	dst.proto=PROTO_UDP;
	dst.send_sock=contact->socket;
    udp_send(&dst, buffer, len);
}


static void
keepalive_timer(unsigned int ticks, void *data)
{
    static unsigned iteration = 0;
    NAT_Contact *contact;
    HashSlot *slot;
    time_t now;
    int i;

    now = time(NULL);

    for (i=0; i<nat_table->size; i++) {

        if ((i % keepalive_interval) != iteration)
            continue;

        slot = &nat_table->slots[i];

        lock_get(&slot->lock);

        slot->head = NAT_Contact_purge_expired(slot->head, now);
        contact = slot->head;

        lock_release(&slot->lock);

        while (contact) {
            send_keepalive(contact);
            contact = contact->next;
        }
    }

    iteration = (iteration+1) % keepalive_interval;
}


// Functions to save and restore the keepalive NAT table. They should only be
// called from mod_init/mod_destroy as they access shm memory without locking
//

#define STATE_FILE_HEADER "# Automatically generated file from internal keepalive state. Do NOT modify!\n"

static void
save_keepalive_state(void)
{
    NAT_Contact *contact;
    FILE *f;
    int i;

    if (!keepalive_state_file)
        return;

    f = fopen(keepalive_state_file, "w");
    if (!f) {
        LM_ERR("failed to open keepalive state file for writing: %s\n", strerror(errno));
        return;
    }

    fprintf(f, STATE_FILE_HEADER);

    for (i=0; i<nat_table->size; i++) {
        contact = nat_table->slots[i].head;
        while (contact) {
            fprintf(f, "%s %.*s %ld %ld\n",
                    contact->uri,
                    contact->socket->sock_str.len, contact->socket->sock_str.s,
                    (long int)contact->registration_expire,
                    (long int)contact->subscription_expire);
            contact = contact->next;
        }
    }

    if (ferror(f))
        LM_ERR("couldn't write keepalive state file: %s\n", strerror(errno));

    fclose(f);
}


static void
restore_keepalive_state(void)
{
    char uri[64], socket[64];
    time_t rtime, stime, now;
    NAT_Contact *contact;
    struct socket_info *sock;
    int port, proto, res;
    unsigned h;
    str host;
    FILE *f;

    if (!keepalive_state_file)
        return;

    f = fopen(keepalive_state_file, "r");
    if (!f) {
        if (errno != ENOENT)
            LM_ERR("failed to open keepalive state file for reading: %s\n", strerror(errno));
        return;
    }

    now = time(NULL);

    res = fscanf(f, STATE_FILE_HEADER); // skip header

    while (True) {
        res = fscanf(f, "%63s %63s %ld %ld", uri, socket, &rtime, &stime);
        if (res == EOF) {
            if (ferror(f))
                LM_ERR("error while reading keepalive state file: %s\n", strerror(errno));
            break;
        } else if (res != 4) {
            LM_ERR("invalid/corrupted keepalive state file. ignoring remaining entries.\n");
            break;
        } else {
            if (now > rtime && now > stime)
                continue; // expired entry

            if (parse_phostport(socket, &host.s, &host.len, &port, &proto) < 0)
                continue;

            sock = grep_sock_info(&host, (unsigned short)port, (unsigned short)proto);
            if (!sock)
                continue; // socket no longer available since last time. ignore.

            h = HASH(nat_table, uri);
            contact = NAT_Contact_new(uri, sock);
            if (contact) {
                SIP_Registration_update(contact, rtime);
                SIP_Subscription_update(contact, stime);
                contact->next = nat_table->slots[h].head;
                nat_table->slots[h].head = contact;
            } else {
                LM_ERR("cannot allocate shared memory for new NAT contact\n");
                break;
            }
        }
    }

    fclose(f);
}


// Module management: initialization/destroy/function-parameter-fixing/...
//

static int
mod_init(void)
{
	sl_cbelem_t slcb;
    int *param;
	modparam_t type;

    if (keepalive_interval <= 0) {
        LM_NOTICE("keepalive functionality is disabled from the configuration\n");
        keepalive_disabled = True;
        return 0;
    }

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}
    // set SL module callback function
	memset(&slcb, 0, sizeof(sl_cbelem_t));
	slcb.type = SLCB_REPLY_READY;
	slcb.cbf = __sl_reply_out;
    if (slb.register_cb(&slcb) != 0) {
        LM_ERR("cannot register callback for stateless outgoing replies\n");
        return -1;
    }

    // bind to the TM API
    if (load_tm_api(&tm_api)!=0) {
        LM_ERR("cannot load the tm module API\n");
        return -1;
    }

    // bind to the dialog API
    if (load_dlg_api(&dlg_api)==0) {
        // load dlg_flag and default_timeout parameters from the dialog module
        param = find_param_export(find_module_by_name("dialog"),
				"dlg_flag", INT_PARAM, &type);
        if (param) {
		have_dlg_api = True;

		dialog_flag = *param;

		param = find_param_export(find_module_by_name("dialog"),
					"default_timeout", INT_PARAM, &type);
		if (!param) {
		    LM_ERR("cannot find default_timeout parameter in the dialog module\n");
		    return -1;
		}
		dialog_default_timeout = *param;

		// register dialog creation callback
		if (dlg_api.register_dlgcb(NULL, DLGCB_CREATED, __dialog_created, NULL, NULL) != 0) {
		    LM_ERR("cannot register callback for dialog creation\n");
		    return -1;
		}

		// register a pre-script callback to automatically enable dialog tracing
		if (register_script_cb(preprocess_request, PRE_SCRIPT_CB|REQUEST_CB, 0)!=0) {
		    LM_ERR("could not register request preprocessing callback\n");
		    return -1;
		}
	}
    }
    if (!have_dlg_api) {
        LM_NOTICE("keeping alive dialogs is disabled because the dialog module is not loaded\n");
    }

    // initialize the keepalive message parameters
    if (keepalive_params.from!=NULL && *(keepalive_params.from)==0) {
        LM_WARN("ignoring empty keepalive_from parameter\n");
        keepalive_params.from = NULL;
    }
    if (strcasecmp(keepalive_params.method, "NOTIFY")==0)
        keepalive_params.event_header = "Event: keep-alive\r\n";
    snprintf(keepalive_params.callid_prefix, 20, "%x", rand());
    keepalive_params.callid_counter = rand();
    keepalive_params.from_tag = rand();

#ifdef STATISTICS
    // we need the statistics initialized before restoring the keepalive state
    if (register_module_stats(exports.name, statistics) < 0) {
        LM_ERR("failed to initialize module statistics\n");
        return -1;
    }
#endif /*STATISTICS*/

    // create hash table to hold NAT contacts
    nat_table = HashTable_new();
    if (!nat_table) {
        LM_ERR("cannot create hash table to store NAT endpoints\n");
        return -1;
    }
    restore_keepalive_state();

    // check keepalive interval and add keepalive timer process
    if (keepalive_interval < 10) {
        LM_WARN("keepalive_interval should be at least 10 seconds\n");
        LM_NOTICE("using 10 seconds for keepalive_interval\n");
        keepalive_interval = 10;
    }
	register_dummy_timers(1);

    return 0;
}

static int
child_init(int rank)
{
	if (rank==PROC_MAIN) {
		if(fork_dummy_timer(PROC_TIMER, "TIMER NT", 1 /*socks flag*/,
					keepalive_timer, NULL, 1 /*sec*/)<0) {
			LM_ERR("failed to register keepalive timer process\n");
			return -1;
		}
	}
	return 0;
}

static void
mod_destroy(void)
{
    if (nat_table) {
        save_keepalive_state();
        HashTable_del(nat_table);
        nat_table = NULL;
    }
}


// Preprocess a request before it is processed in the main script route
//
// Here we enable dialog tracing to be able to automatically extend an
// existing registration keepalive to a destination, for the duration of
// the dialog, even if the dialog source is not kept alive by explicitly
// calling nat_keepalive(). This is needed to still be able to forward
// messages to the callee, even if the registration keepalive expires
// during the dialog and it is not renewed.
//
static int
preprocess_request(struct sip_msg *msg, unsigned int flags, void *_param)
{
    str totag;

    if (msg->first_line.u.request.method_value!=METHOD_INVITE)
        return 1;

    if (parse_headers(msg, HDR_TO_F, 0) == -1) {
        LM_ERR("failed to parse To header\n");
        return -1;
    }
    if (!msg->to) {
        LM_ERR("missing To header\n");
        return -1;
    }
    totag = get_to(msg)->tag_value;
    if (totag.s==0 || totag.len==0) {
        setflag(msg, dialog_flag);
    }

    return 1;
}


// Filter out replies to keepalive messages
//
static int
reply_filter(struct sip_msg *reply)
{
    struct cseq_body *cseq;
    static str prefix = {NULL, 0};
    str call_id;

    parse_headers(reply, HDR_VIA2_F, 0);
    if (reply->via2)
        return 1;

    // check if the method from CSeq header matches our method
    if (!reply->cseq && parse_headers(reply, HDR_CSEQ_F, 0) < 0) {
        LM_ERR("failed to parse the CSeq header\n");
        return -1;
    }
    if (!reply->cseq) {
        LM_ERR("missing CSeq header\n");
        return -1;
    }
    cseq = reply->cseq->parsed;
    if (!STR_MATCH(cseq->method, keepalive_params.method))
        return 1;

    // check if callid_prefix matches
    if (!reply->callid && parse_headers(reply, HDR_CALLID_F, 0) < 0) {
        LM_ERR("failed to parse the Call-ID header\n");
        return -1;
    }
    if (!reply->callid) {
        LM_ERR("missing Call-ID header\n");
        return -1;
    }
    call_id = reply->callid->body;
    if (prefix.s == NULL) {
        prefix.s = keepalive_params.callid_prefix;
        prefix.len = strlen(prefix.s);
    }
    if (!STR_HAS_PREFIX(call_id, prefix) || call_id.s[prefix.len]!='-')
        return 1;

    return 0;
}


// Pseudo variable management
//

static int
pv_parse_nat_contact_name(pv_spec_p sp, str *in)
{
    char *p;
    char *s;
    pv_spec_p nsp = 0;

    if(in==NULL || in->s==NULL || sp==NULL)
        return -1;
    p = in->s;
    if (*p==PV_MARKER) {
        nsp = (pv_spec_p)pkg_malloc(sizeof(pv_spec_t));
        if (nsp==NULL) {
            LM_ERR("cannot allocate private memory\n");
            return -1;
        }
        s = pv_parse_spec(in, nsp);
        if (s==NULL) {
            LM_ERR("invalid name [%.*s]\n", in->len, in->s);
            pv_spec_free(nsp);
            return -1;
        }
        sp->pvp.pvn.type = PV_NAME_PVAR;
        sp->pvp.pvn.u.dname = (void*)nsp;
        return 0;
    }

    sp->pvp.pvn.type = PV_NAME_INTSTR;
    sp->pvp.pvn.u.isname.type = AVP_NAME_STR;
    sp->pvp.pvn.u.isname.name.s = *in;

    return 0;
}


static int
pv_get_keepalive_socket(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
    static char uri[128];
    NAT_Contact *contact;
    pv_value_t tv;
    unsigned h;

    if (msg==NULL || param==NULL || res==NULL)
        return -1;

    if (pv_get_spec_name(msg, param, &tv)!=0 || (!(tv.flags&PV_VAL_STR))) {
        LM_ERR("invalid NAT contact uri\n");
        return -1;
    }

    if (tv.rs.len > sizeof(uri)-1) {
        LM_ERR("NAT contact uri too long\n");
        return -1;
    }

    strncpy(uri, tv.rs.s, tv.rs.len);
    uri[tv.rs.len] = 0;

    h = HASH(nat_table, uri);
    lock_get(&nat_table->slots[h].lock);

    contact = HashTable_search(nat_table, uri, h);
    if (!contact) {
        lock_release(&nat_table->slots[h].lock);
        return pv_get_null(msg, param, res);
    }

    res->rs = contact->socket->sock_str;
    res->flags = PV_VAL_STR;

    lock_release(&nat_table->slots[h].lock);

    return 0;
}


static int
pv_get_source_uri(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
    static char uri[128];

    if (msg==NULL || res==NULL)
        return -1;

    snprintf(uri, 64, "sip:%s:%d", ip_addr2a(&msg->rcv.src_ip), msg->rcv.src_port);

    switch (msg->rcv.proto) {
    case PROTO_TCP:
        strcat(uri, ";transport=tcp");
        break;
    case PROTO_TLS:
        strcat(uri, ";transport=tls");
        break;
    case PROTO_SCTP:
        strcat(uri, ";transport=sctp");
        break;
    case PROTO_WS:
    case PROTO_WSS:
        strcat(uri, ";transport=ws");
        break;
    }

    res->rs.s = uri;
    res->rs.len = strlen(uri);
    res->flags = PV_VAL_STR;

    return 0;
}


