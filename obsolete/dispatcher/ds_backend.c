/**
 * $Id$
 *
 * dispatcher module
 * 
 * Copyright (C) 2004-2006 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../trim.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../dset.h"

#include "dispatcher.h"

/******************************************************************************
 *
 * ds_destroy_lists()
 *
 * free all memory occupied by dispatcher module
 *
 *****************************************************************************/
 
int ds_destroy_lists()
{
	extern int *ds_activelist;	
	extern char ***ds_setp_a, ***ds_setp_b;
	extern int *ds_setlen_a, *ds_setlen_b;
	int set, node;

	/* assume the whole structure has been allocated if ds_activelist
	 * is non-NULL
	 * hint: when using ser -c memory wasn't allocated
	 */
	if (ds_activelist == NULL)
		return 0;

	/* free sets and nodes */
	for (set = 0; set < DS_MAX_SETS; set++) {
		for (node = 0; node < DS_MAX_NODES; node++) {
			shm_free(ds_setp_a[set][node]);
			shm_free(ds_setp_b[set][node]);
		}
		shm_free(ds_setp_a[set]);
		shm_free(ds_setp_b[set]);
	}
	/* free counters */
	shm_free(ds_setlen_a);
	shm_free(ds_setlen_b);

	/* eventually, free ds_activelist */
	shm_free(ds_activelist);

	return 0;
}

/******************************************************************************
 *
 * ds_get_hash()
 *
 * obtain hash from given strings
 *
 *****************************************************************************/

unsigned int ds_get_hash(str *x, str *y)
{
	char* p;
	register unsigned v;
	register unsigned h;

	if(!x && !y)
		return 0;
	h=0;
	if(x)
	{
		p=x->s;
		if (x->len>=4){
			for (;p<=(x->s+x->len-4); p+=4){
				v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
				h+=v^(v>>3);
			}
		};
		v=0;
		for (;p<(x->s+x->len); p++)
		{ 
			v<<=8; 
			v+=*p;
		}
		h+=v^(v>>3);
	}
	if(y)
	{
		p=y->s;
		if (y->len>=4){
			for (;p<=(y->s+y->len-4); p+=4){
				v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
				h+=v^(v>>3);
			}
		};
		v=0;
		for (;p<(y->s+y->len); p++)
		{ 
			v<<=8; 
			v+=*p;
		}
		h+=v^(v>>3);
	}
	h=((h)+(h>>11))+((h>>13)+(h>>23));

	return (h)?h:1;
}


/*
 * gets the part of the uri we will use as a key for hashing
 * params:  key1       - will be filled with first part of the key
 *                       (uri user or "" if no user)
 *          key2       - will be filled with the second part of the key
 *                       (uri host:port)
 *          uri        - str with the whole uri
 *          parsed_uri - struct sip_uri pointer with the parsed uri
 *                       (it must point inside uri). It can be null
 *                       (in this case the uri will be parsed internally).
 *          flags  -    if & DS_HASH_USER_ONLY, only the user part of the uri
 *                      will be used
 * returns: -1 on error, 0 on success
 */
static inline int get_uri_hash_keys(str* key1, str* key2,
							str* uri, struct sip_uri* parsed_uri, int flags)
{
	struct sip_uri tmp_p_uri; /* used only if parsed_uri==0 */
	
	if (parsed_uri==0){
		if (parse_uri(uri->s, uri->len, &tmp_p_uri)<0){
			LOG(L_ERR, "DISPATCHER: get_uri_hash_keys: invalid uri %.*s\n",
					uri->len, uri->len?uri->s:"");
			goto error;
		}
		parsed_uri=&tmp_p_uri;
	}
	/* uri sanity checks */
	if (parsed_uri->host.s==0){
			LOG(L_ERR, "DISPATCHER: get_uri_hash_keys: invalid uri, no host"
					   "present: %.*s\n", uri->len, uri->len?uri->s:"");
			goto error;
	}
	
	/* we want: user@host:port if port !=5060
	 *          user@host if port==5060
	 *          user if the user flag is set*/
	*key1=parsed_uri->user;
	key2->s=0;
	key2->len=0;
	if ((!(flags & (DS_HASH_USER_ONLY | DS_HASH_USER_OR_HOST))) ||
		((key1->s==0) && (flags & DS_HASH_USER_OR_HOST))){
		/* key2=host */
		*key2=parsed_uri->host;
		/* add port if needed */
		if (parsed_uri->port.s!=0){ /* uri has a port */
			/* skip port if == 5060 or sips and == 5061 */
			if (parsed_uri->port_no !=
					((parsed_uri->type==SIPS_URI_T)?SIPS_PORT:SIP_PORT))
				key2->len+=parsed_uri->port.len+1 /* ':' */;
		}
	}
	if (key1->s==0 && (flags & DS_HASH_USER_ONLY)){
		LOG(L_WARN, "DISPATCHER: get_uri_hash_keys: empty username in:"
					" %.*s\n", uri->len, uri->len?uri->s:"");
	}
	return 0;
error:
	return -1;
}



/**
 *
 */
int ds_hash_fromuri(struct sip_msg *msg, unsigned int *hash)
{
	str from;
	str key1;
	str key2;
	
	if(msg==NULL || hash == NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_fromuri: bad parameters\n");
		return -1;
	}
	
	if(parse_from_header(msg)==-1)
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_fromuri:ERROR cannot parse From hdr\n");
		return -1;
	}
	
	if(msg->from==NULL || get_from(msg)==NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_fromuri:ERROR cannot get From uri\n");
		return -1;
	}
	
	from   = get_from(msg)->uri;
	trim(&from);
	if (get_uri_hash_keys(&key1, &key2, &from, 0, ds_flags)<0)
		return -1;
	*hash = ds_get_hash(&key1, &key2);
	
	return 0;
}



/**
 *
 */
int ds_hash_touri(struct sip_msg *msg, unsigned int *hash)
{
	str to;
	str key1;
	str key2;
	
	if(msg==NULL || hash == NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_touri: bad parameters\n");
		return -1;
	}
	if ((msg->to==0) && ((parse_headers(msg, HDR_TO_F, 0)==-1) ||
				(msg->to==0)))
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_touri:ERROR cannot parse To hdr\n");
		return -1;
	}
	
	
	to   = get_to(msg)->uri;
	trim(&to);
	
	if (get_uri_hash_keys(&key1, &key2, &to, 0, ds_flags)<0)
		return -1;
	*hash = ds_get_hash(&key1, &key2);
	
	return 0;
}



/**
 *
 */
int ds_hash_callid(struct sip_msg *msg, unsigned int *hash)
{
	str cid;
	if(msg==NULL || hash == NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_callid: bad parameters\n");
		return -1;
	}
	
	if(msg->callid==NULL && ((parse_headers(msg, HDR_CALLID_F, 0)==-1) ||
				(msg->callid==NULL)) )
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_callid:ERROR cannot parse Call-Id\n");
		return -1;
	}
	
	cid.s   = msg->callid->body.s;
	cid.len = msg->callid->body.len;
	trim(&cid);
	
	*hash = ds_get_hash(&cid, NULL);
	
	return 0;
}



int ds_hash_ruri(struct sip_msg *msg, unsigned int *hash)
{
	str* uri;
	str key1;
	str key2;
	
	
	if(msg==NULL || hash == NULL)
	{
		LOG(L_ERR, "DISPATCHER:ds_hash_ruri: bad parameters\n");
		return -1;
	}
	if (parse_sip_msg_uri(msg)<0){
		LOG(L_ERR, "DISPATCHER: ds_hash_ruri: ERROR: bad request uri\n");
		return -1;
	}
	
	uri=GET_RURI(msg);
	if (get_uri_hash_keys(&key1, &key2, uri, &msg->parsed_uri, ds_flags)<0)
		return -1;
	
	*hash = ds_get_hash(&key1, &key2);
	return 0;
}

/* from dispatcher */
static int set_new_uri_simple(struct sip_msg *msg, str *uri)
{
	if (msg->new_uri.s)
	{
		pkg_free(msg->new_uri.s);
		msg->new_uri.len=0;
	}

	msg->parsed_uri_ok=0;
	msg->new_uri.s = (char*)pkg_malloc(uri->len+1);
	if (msg->new_uri.s==0)
	{
		ERR("no more pkg memory\n");
		return -1;
	}
	memcpy(msg->new_uri.s, uri->s, uri->len);
	msg->new_uri.s[uri->len]=0;
	msg->new_uri.len=uri->len;
	ruri_mark_new();
	return 0;
}

/* from dispatcher */
static int set_new_uri_with_user(struct sip_msg *msg, str *uri, str *user)
{
	struct sip_uri dst;
	int start_len, stop_len;
	
	if (parse_uri(uri->s, uri->len, &dst) < 0) {
		ERR("can't parse destination URI\n");
		return -1;
	}
	if ((!dst.host.s) || (dst.host.len <= 0)) {
		ERR("destination URI host not set\n");
		return -1;
	}
	if (dst.user.s && (dst.user.len > 0)) {
		DBG("user already exists\n");
		/* don't replace the user */
		return set_new_uri_simple(msg, uri);
	}
	
	if (msg->new_uri.s)
	{
		pkg_free(msg->new_uri.s);
		msg->new_uri.len=0;
	}
	
	start_len = dst.host.s - uri->s;
	stop_len = uri->len - start_len;
	
	msg->parsed_uri_ok=0;
	msg->new_uri.s = (char*)pkg_malloc(uri->len+1+user->len+1);
	if (msg->new_uri.s==0)
	{
		ERR("no more pkg memory\n");
		return -1;
	}
	memcpy(msg->new_uri.s, uri->s, start_len);
	memcpy(msg->new_uri.s + start_len, user->s, user->len);
	*(msg->new_uri.s + start_len + user->len) = '@';
	memcpy(msg->new_uri.s + start_len + user->len + 1, dst.host.s, stop_len);
	
	msg->new_uri.len=uri->len + user->len + 1;
	msg->new_uri.s[msg->new_uri.len]=0;
	ruri_mark_new();
	
	return 0;
}

static int set_new_uri(struct sip_msg *msg, str *uri)
{
	struct to_body* to;
	struct sip_uri to_uri;
	
	/* we need to leave original user */
	to = get_to(msg);
	if (to) {
		if (parse_uri(to->uri.s, to->uri.len, &to_uri) >= 0) {
			if (to_uri.user.s && (to_uri.user.len > 0)) {
				return set_new_uri_with_user(msg, uri, &to_uri.user);
			}
		}
	}

	return set_new_uri_simple(msg, uri);
}


/******************************************************************************
 *
 * ds_select_dst_impl()
 *
 * use requested algorithm to calculate hash and pull an URI off the
 * active dispatcher list.
 * set dst_uri or new_uri accordingly
 * Attention: if specific hash algorithms fail the module falls back
 *            to a hash on the Call-ID
 *
 *****************************************************************************/

int ds_select_dst_impl(struct sip_msg *msg, char *set_, char *alg_, int set_new)
{
	extern int *ds_activelist;
	extern char ***ds_setp_a, ***ds_setp_b;
	extern int *ds_setlen_a, *ds_setlen_b;

	int set, alg;
	unsigned int hash;

	str uri;

	if(msg==NULL)
    {
        LOG(L_ERR, "DISPATCHER:ds_select_dst: bad parameters\n");
        return -1;
    }

	if ( get_int_fparam(&set, msg, (fparam_t*)set_) < 0 ) {
	  LOG(L_ERR, "DISPATCHER:ds_select_dst: bad set value (%d)\n", set);
	  return -1;
	}

	if ( get_int_fparam(&alg, msg, (fparam_t*)alg_) < 0 ) {
	  LOG(L_ERR, "DISPATCHER:ds_select_dst: bad algorithm (%d)\n", alg);
	  return -1;
	}

    if ((set < 0) || (set >= DS_MAX_SETS)) {
        LOG(L_ERR, "DISPATCHER:ds_select_dst: bad set offset (%d)\n", set);
        return -1;
    }
    if ((alg < 0) || (alg > 4)) {
        LOG(L_ERR, "DISPATCHER:ds_select_dst: invalid algorithm\n");
        return -1;
    }

	if (((*ds_activelist == 0) ?  ds_setlen_a[set] : ds_setlen_b[set]) <= 0) {
        LOG(L_ERR, "DISPATCHER:ds_select_dst: empty destination set\n");
        return -1;
    }
    if (msg->dst_uri.s != NULL || msg->dst_uri.len > 0) {
		if (msg->dst_uri.s)
            pkg_free(msg->dst_uri.s);
        msg->dst_uri.s = NULL;
        msg->dst_uri.len = 0;
    }
    /* get hash */
    hash = 0;
    switch (alg) {
		/* see bottom for case '0' */
        case 1: /* hash from uri */
            if (ds_hash_fromuri(msg, &hash) != 0) {
                if (ds_hash_callid(msg, &hash) != 0) {
                    LOG(L_ERR, "DISPATCHER:ds_select_dst: cannot determine from uri hash\n");
                    return -1;
                }
            }
            break;
        case 2: /* hash to uri */
            if (ds_hash_touri(msg, &hash) != 0) {
                if (ds_hash_callid(msg, &hash) != 0) {
                    LOG(L_ERR, "DISPATCHER:ds_select_dst: cannot determine from uri hash\n");
                    return -1;
                }
            }
            break;
        case 3: /* hash Request uri */
            if (ds_hash_ruri(msg, &hash) != 0) {
                if (ds_hash_callid(msg, &hash) != 0) {
                    LOG(L_ERR, "DISPATCHER:ds_select_dst: cannot determine from uri hash\n");
                    return -1;
                }
            }
            break;
		case 4:	/* Call ID hash, shifted right once to skip low bit
				 * This should allow for even distribution when using
				 * Call ID hash twice (i.e. fe + be)
				 */
            if (ds_hash_callid(msg, &hash) != 0) {
                LOG(L_ERR,
                    "DISPATCHER:ds_select_dst: cannot determine callid hash\n");
                hash = 0; /* bad default, just to be sure */
                return -1;
            }
			hash = hash >> 4; /* should be enough for even more backends */
			break;
        case 0: /* hash call id */ /* fall-through */
        default:
            if (ds_hash_callid(msg, &hash) != 0) {
                LOG(L_ERR,
                    "DISPATCHER:ds_select_dst: cannot determine callid hash\n");
                hash = 0; /* bad default, just to be sure */
                return -1;
            }
			break; /* make gcc happy */
    }

    DBG("DISPATCHER:ds_select_dst: hash: [%u]\n", hash);
    /* node list offset from hash */
    if (*ds_activelist == 0) {
        hash = hash % ds_setlen_a[set];
        uri.s = ds_setp_a[set][hash];
        uri.len = strlen(ds_setp_a[set][hash]);
    } else {
        hash = hash % ds_setlen_b[set];
        uri.s = ds_setp_b[set][hash];
        uri.len = strlen(ds_setp_b[set][hash]);
    }

	if (!set_new) {
		if (set_dst_uri(msg, &uri) < 0) {
            LOG(L_ERR,
				"DISPATCHER:dst_select_dst: Error while setting dst_uri\n");
            return -1;
        }
		/* dst_uri changed, so it makes sense to re-use the current uri for
			forking */
		ruri_mark_new(); /* re-use uri for serial forking */
    	DBG("DISPATCHER:ds_select_dst: selected [%d-%d-%d] <%.*s>\n",
        	alg, set, hash, msg->dst_uri.len, msg->dst_uri.s);
	} else {
		if (set_new_uri(msg, &uri) < 0) {
            LOG(L_ERR,
				"DISPATCHER:dst_select_dst: Error while setting new_uri\n");
            return -1;
        }
    	DBG("DISPATCHER:ds_select_dst: selected [%d-%d-%d] <%.*s>\n",
        	alg, set, hash, msg->new_uri.len, msg->dst_uri.s);
    }

    return 1;
}

/**
 * from dispatcher
 */
int ds_select_dst(struct sip_msg *msg, char *set, char *alg)
{
	return ds_select_dst_impl(msg, set, alg, 0);
}

/**
 * from dispatcher
 */
int ds_select_new(struct sip_msg *msg, char *set, char *alg)
{
	return ds_select_dst_impl(msg, set, alg, 1);
}



/******************************************************************************
 *
 * ds_init_memory()
 *
 * init memory structure
 *
 * - ds_activelist: active list (0 or 1)
 * - ds_setp_a/ds_setp_b: each active config has up to DS_MAX_SETS so
 *   called sets. Each set references to a node list with DS_MAX_NODES
 *   slots that hold SIP URIs up to DS_MAX_URILEN-1 Bytes
 *
 *****************************************************************************/

#define MALLOC_ERR  LOG(L_ERR, \
        "ERROR:DISPATCHER:init_dispatcher_mem: shm_malloc() failed\n");
int ds_init_memory() {

    extern int *ds_activelist;
    extern char ***ds_setp_a, ***ds_setp_b;
    extern int *ds_setlen_a, *ds_setlen_b;

    int set, node;

    /* active list */
    ds_activelist = (int *) shm_malloc(sizeof(int));
    if (ds_activelist == NULL) {
        MALLOC_ERR;
        return -1;
    }
    *ds_activelist = 0;

    ds_setp_a = (char ***) shm_malloc(sizeof(char **) * DS_MAX_SETS);
    if (ds_setp_a == NULL) {
        MALLOC_ERR;
        return -1;
    }
    /* attach node list to each set */
    for (set = 0; set < DS_MAX_SETS; set++) {
        ds_setp_a[set] = (char **) shm_malloc(sizeof(char *) * DS_MAX_NODES);
        if (ds_setp_a[set] == NULL) {
            MALLOC_ERR;
            return -1;
        }
        /* init each node */
        for (node = 0; node < DS_MAX_NODES; node++) {
            ds_setp_a[set][node] = (char *)
                    shm_malloc(sizeof(char) * DS_MAX_URILEN);
            if (ds_setp_a[set][node] == NULL) {
                MALLOC_ERR;
                return -1;
            }
            *ds_setp_a[set][node] = '\0';
        }
    }

    ds_setp_b = (char ***) shm_malloc(sizeof(char **) * DS_MAX_SETS);
    if (ds_setp_b == NULL) {
        MALLOC_ERR;
        return -1;
    }
    /* attach node list to each set */
    for (set = 0; set < DS_MAX_SETS; set++) {
        ds_setp_b[set] = (char **) shm_malloc(sizeof(char *) * DS_MAX_NODES);
        if (ds_setp_b[set] == NULL) {
            MALLOC_ERR;
            return -1;
        }
        /* init each node */
        for (node = 0; node < DS_MAX_NODES; node++) {
            ds_setp_b[set][node] = (char *)
                    shm_malloc(sizeof(char) * DS_MAX_URILEN);
            if (ds_setp_b[set][node] == NULL) {
                MALLOC_ERR;
                return -1;
            }
            *ds_setp_b[set][node] = '\0';
        }
    }

    /* set length counters */
    ds_setlen_a = (int *) shm_malloc(sizeof(int) * DS_MAX_SETS);
    if (ds_setlen_a == NULL) {
        MALLOC_ERR;
        return -1;
    }
    ds_setlen_b = (int *) shm_malloc(sizeof(int) * DS_MAX_SETS);
    if (ds_setlen_b == NULL) {
        MALLOC_ERR;
        return -1;
    }
    for (set = 0; set < DS_MAX_SETS; set++) {
        ds_setlen_a[set] = 0;
        ds_setlen_b[set] = 0;
    }
    return 0;
}

/******************************************************************************
 *
 * ds_clean_list()
 *
 * empty the in-active config so we can reload a new one
 * this does not free() memory!
 *
 *****************************************************************************/

void ds_clean_list(void) {

    extern int *ds_activelist;
    extern char ***ds_setp_a, ***ds_setp_b;
    extern int *ds_setlen_a, *ds_setlen_b;

    int set, node;

    for (set = 0; set < DS_MAX_SETS; set++) {
        for (node = 0; node < DS_MAX_NODES; node++) {
            if (*ds_activelist == 0) {
                *ds_setp_b[set][node] = '\0';
            } else {
                *ds_setp_a[set][node] = '\0';
            }
        }
        if (*ds_activelist == 0) {
            ds_setlen_b[set] = 0;
        } else {
            ds_setlen_a[set] = 0;
        }
    }
    return;
}

/******************************************************************************
 *
 * ds_load_list()
 *
 * (re)load dispatcher module config file using the dispatcher module
 * parser.
 * Save config in the in-active shared memory section
 * Attention: We do not bail out with an error if we try to store more
 *            sets/nodes than we have room for since we might be running
 *            live and don't want SER to stop processing packets, do we?
 *
 *****************************************************************************/

int ds_load_list (char *lfile) {

    /* storage */
    extern int *ds_activelist;
    extern char ***ds_setp_a, ***ds_setp_b;
    extern int *ds_setlen_a, *ds_setlen_b;

    /* parser related */
    char line[MAX_LINE_LEN], *p;
    int set;
    str uri;
    struct sip_uri puri;
    FILE *f = NULL;

    DBG("ds_load_list() invoked\n");

    /* clean up temporary list before saving updated config */
    (void) ds_clean_list();

    if (lfile == NULL || strlen(lfile) <= 0) {
        LOG(L_ERR, "DISPATCHER:ds_load_list: cannot open list file [%s]\n",
            lfile);
        return -1;
    }

    f = fopen(lfile, "r");
    if (f == NULL) {
        LOG(L_ERR, "DISPATCHER:ds_load_list: cannot open list file [%s]\n",
            lfile);
        return -1;
    }

    p = fgets(line, MAX_LINE_LEN-1, f);
    while (p)
    {
        /* eat all white spaces */
        while (*p && (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n'))
            p++;
        if (*p=='\0' || *p=='#')
            goto next_line;

        /* get set id */
        set = 0;
        while(*p>='0' && *p<='9')
        {
            set = set*10+ (*p-'0');
            p++;
        }

        /* eat all white spaces */
        while(*p && (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n'))
            p++;
        if(*p=='\0' || *p=='#')
        {
            LOG(L_ERR, "DISPATCHER:ds_load_list: bad line [%s]\n", line);
            goto error;
        }
        /* get uri */
        uri.s = p;
        while(*p && *p!=' ' && *p!='\t' && *p!='\r' && *p!='\n' && *p!='#')
            p++;
        uri.len = p-uri.s;

        /* check uri */
        if(parse_uri(uri.s, uri.len, &puri)!=0)
        {
            LOG(L_ERR, "DISPATCHER:ds_load_list: bad uri [%.*s]\n",
                    uri.len, uri.s);
            goto next_line;
        }

        /* now we have set id and get uri -> save to shared mem */
        if ((set > DS_MAX_SETS-1) || (uri.len > DS_MAX_URILEN))
        {
            LOG(L_ERR, "DISPATCHER:ds_load_list: increase DS_MAX_SETS or DS_MAX_URILEN ...\n");
            goto next_line;
        }
        /* save correct line from config file */
        DBG("content: set %d, str: %.*s\n", set, uri.len, uri.s);

        if (*ds_activelist == 0) {
            DBG("[%d] active nodes in this set so far: %d\n",
                *ds_activelist, ds_setlen_b[set]);
            if (ds_setlen_b[set] >= (DS_MAX_NODES-1)) {
                LOG(L_ERR, "DISPATCHER:ds_load_list: increase DS_MAX_NODES!\n");
                goto next_line;
            }
            snprintf((char *)ds_setp_b[set][ds_setlen_b[set]], DS_MAX_URILEN-1,
                    "%.*s", uri.len, uri.s);
            ds_setlen_b[set]++;
            DBG("[%d] active nodes in this set now: %d\n",
                *ds_activelist, ds_setlen_b[set]);
            DBG("node now contains: %s\n", (char *)ds_setp_b[set][ds_setlen_b[set]-1]);
        } else {
            DBG("[%d] active nodes in this set so far: %d\n",
                *ds_activelist, ds_setlen_a[set]);
            if (ds_setlen_a[set] >= (DS_MAX_NODES-1)) {
                LOG(L_ERR, "DISPATCHER:ds_load_list: increase DS_MAX_NODES!\n");
                goto next_line;
            }
            snprintf((char *)ds_setp_a[set][ds_setlen_a[set]], DS_MAX_URILEN-1,
                    "%.*s", uri.len, uri.s);
            ds_setlen_a[set]++;
            DBG("[%d] active nodes in this set now: %d\n",
                *ds_activelist, ds_setlen_a[set]);
            DBG("node now contains: %s\n", (char *)ds_setp_a[set][ds_setlen_a[set]-1]);
        }

next_line:
        p = fgets(line, MAX_LINE_LEN-1, f);
    }
    if (f != NULL)
        fclose(f);
    /* see if there are any active sets at all */
    if (*ds_activelist == 0) {
        int found = 0;
        int i;
        for (i = 0; i < DS_MAX_SETS; i++) {
            if (ds_setlen_b[i] > 0)
                found++;
        }
        if (!found)
            return -1;
    } else {
        int found = 0;
        int i;
        for (i = 0; i < DS_MAX_SETS; i++) {
            if (ds_setlen_a[i] > 0)
                found++;
        }
        if (!found)
            return -1;

    }
    return 0;
error:

    if (f != NULL)
        fclose(f);

    return -1;
}
