/*
 * Copyright (C) 2008-2009 1&1 Internet AG
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


/**
 * \file
 * \brief SIP-utils :: Only allow one 183 message per call-id
 * \ingroup siputils
 * - Module; \ref siputils
 *
 * \section ring_utils UTILS :: Ringing functionality
 *
 * In a parallel forking scenario you may get several 183s with SDP. You don't want
 * that your customers hear more than one ringtone or answer machine in parallel
 * on the phone. So its necessary to drop the 183 in these cases and send a 180 instead.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "../../parser/msg_parser.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../timer.h"
#include "../../locking.h"
#include "../../md5.h"

#include "config.h"
#include "ring.h"


/*! list of calls for ringing functionality */
struct ring_record_t {
	struct ring_record_t *next;
	unsigned int time; /*!< timeout value */
	char callid[MAXCALLIDLEN+1]; /*!< callid of this call */
};

/*! hashtable for ringing records */
struct hashtable_entry_t {
	struct ring_record_t *head;
	struct ring_record_t *tail;
};

typedef struct hashtable_entry_t hashtable_t[HASHTABLESIZE];

/*! global hashtable */
static hashtable_t *hashtable = NULL;

static void insert(str callid);

static int contains(str callid);


/*!
 * \brief  Inserts callid of message into hashtable
 *
 * Inserts callid of message into hashtable. Any 183 messages with
 * this callid that occur in the next ring_timeout seconds, will be
 * converted to 180.
 * \param msg SIP message
 * \param unused1 unused
 * \param unused2 unused
 * \return 1 on success, -1 otherwise
 */
int ring_insert_callid(struct sip_msg *msg, char *unused1, char *unused2)
{
	/* could fail, eg if already parsed don't care about result */
	parse_headers(msg, HDR_CALLID_F, 0);

	if (msg->callid) {
		lock_get(ring_lock);
		if (!contains(msg->callid->body)) insert(msg->callid->body);
		lock_release(ring_lock);
	} else {
		LM_ERR("no callid\n");
		return -1;
	}

	return 1;
}


/*!
 * \brief Initialize the ring hashtable in shared memory
 */
void ring_init_hashtable(void)
{
	int i;

	hashtable = shm_malloc(sizeof(hashtable_t));
	assert(hashtable);
	for (i=0; i<HASHTABLESIZE; i++) {
		(*hashtable)[i].head = NULL;
		(*hashtable)[i].tail = NULL;
	}
}


/*!
 * \brief Destroy the ring hashtable
 */
void ring_destroy_hashtable(void)
{
	int i;

	if (hashtable) {
		for (i=0; i<HASHTABLESIZE; i++) {
			while ((*hashtable)[i].head) {
				struct ring_record_t* rr = (*hashtable)[i].head;
				(*hashtable)[i].head = rr->next;
				shm_free(rr);
			}
			(*hashtable)[i].tail = NULL;
		}

		shm_free(hashtable);
	}
}


/*!
 * \brief Hash helper function
 * \param buf hashed buffer
 * \param len length of buffer
 * \return hash value, can be 0
 */
static unsigned int hash(char *buf, int len)
{
	int i;
	unsigned int retval = 0;
	MD5_CTX md5context;
	char digest[16];
	
	MD5Init(&md5context);
	MD5Update(&md5context, buf, len);
	MD5Final(digest, &md5context);

	for (i=0; i<16; i++) {
		retval ^= ((unsigned int)((unsigned char)buf[i])) << i;
	}

	return retval;
}


/*!
 * \brief Expire entries on the hashtable
 * \param index array index that should expired
 */
static void remove_timeout(unsigned int index)
{
	int ring_timeout = cfg_get(siputils, siputils_cfg, ring_timeout);
	if(ring_timeout == 0){
		LM_ERR("Could not get timeout from cfg. This will expire all entries");
	}
	while ((*hashtable)[index].head && ((*hashtable)[index].head)->time + ring_timeout < get_ticks()) {
		struct ring_record_t* rr = (*hashtable)[index].head;
		(*hashtable)[index].head = rr->next;
		if ((*hashtable)[index].head == NULL) (*hashtable)[index].tail = NULL;
		LM_DBG("deleting ticks=%d %s\n", get_ticks(), rr->callid);
		shm_free(rr);
	}
}


/*!
 * \brief Insert a new entry on the hashtable
 * \param callid Call-ID string
 */
static void insert(str callid)
{
	unsigned int index = hash(callid.s, callid.len) & HASHTABLEMASK;
	struct ring_record_t* rr;
	
	remove_timeout(index);
	rr = shm_malloc(sizeof(struct ring_record_t));
	assert(rr);

	rr->next = NULL;
	rr->time = get_ticks();
	strncpy(rr->callid, callid.s, MIN(callid.len, MAXCALLIDLEN));
	rr->callid[MIN(callid.len, MAXCALLIDLEN)] = 0;

	if ((*hashtable)[index].tail) {
		(*hashtable)[index].tail->next = rr;
		(*hashtable)[index].tail = rr;
	}
	else {
		(*hashtable)[index].head = rr;
		(*hashtable)[index].tail = rr;
	}

	LM_DBG("inserting at %d %.*s ticks=%d\n", index, callid.len, callid.s, rr->time);
}


/*!
 * \brief Helper functions that checks if the hash table contains the callid
 * \param callid Call-ID that is searched
 * \return 1 when callid could be found, 0 when not found
 */
static int contains(str callid)
{
	unsigned int index = hash(callid.s, callid.len) & HASHTABLEMASK;
	struct ring_record_t* rr;

	remove_timeout(index);

	rr = (*hashtable)[index].head;
	while (rr) {
		if (strncmp(rr->callid, callid.s, callid.len) == 0) return 1;
		rr = rr->next;
	}
	return 0;
}


/*!
 * \brief Convert a 183  to a 180 message.
 * \param msg SIP message
 */
static int conv183(struct sip_msg *msg)
{
	/* content-length and content-type headers are removed */
	char *del1_start = strstr(msg->buf, "Content-Length:");
	char *del2_start = strstr(msg->buf, "Content-Type:");
	char *del1_end;
	char *del2_end;
	char *eoh;
	char *chunk1_start;
	int chunk1_len;
	char *chunk1_dst;
	char *chunk2_start;
	int chunk2_len;
	char *chunk2_dst;
	char *chunk3_start;
	int chunk3_len;
	char *chunk3_dst;
	
	if (del1_start>del2_start) {
		char *tmp = del1_start;
		del1_start = del2_start;
		del2_start = tmp;
	}

	del1_end = NULL;
	if (del1_start) {
		del1_end = strstr(del1_start, "\r\n");
		if (del1_end) del1_end+=2;
	}
	del2_end = NULL;
	if (del2_start) {
		del2_end = strstr(del2_start, "\r\n");
		if (del2_end) del2_end+=2;
	}

	/* 180 message does not need session description */
	eoh = strstr(msg->buf, "\r\n\r\n");
	if (eoh) eoh+=2;

	if ((!del1_start) || (!del2_start) || (!del1_end) || (!del2_end) || (!eoh)) {
		LM_ERR("got invalid 183 message\n");
		return -1;
	}

	/*
	 * if message is parsed further than first deletion, offsets of parsed strings would
	 * not be correct any more. In that case do not convert! If this error is reported,
	 * check if other pre script callbacks are installed before the one of this module.
	 */
	if (msg->unparsed>del1_start) {
		LM_ERR("183 message got parsed too far!\n");
		return -1;
	}

	/* setting new status */
	msg->first_line.u.reply.statuscode=180;
	msg->first_line.u.reply.status.s[2]='0';
	// don't change length of reason string
	strncpy(msg->first_line.u.reply.reason.s, "Ringing                                           ", msg->first_line.u.reply.reason.len);

	/* calculate addresses of chunks to be moved */
	chunk1_start = del1_end;
	chunk1_len     = (int)(long)(del2_start-del1_end);
	chunk1_dst   = del1_start;

	chunk2_start = del2_end;
	chunk2_len     = (int)(long)(eoh-del2_end);
	chunk2_dst   = chunk1_dst+chunk1_len;

	chunk3_start = "Content-Length: 0\r\n\r\n";
	chunk3_len     = strlen(chunk3_start);
	chunk3_dst   = chunk2_dst+chunk2_len;

	// move chunks
	memmove(chunk1_dst, chunk1_start, chunk1_len);
	memmove(chunk2_dst, chunk2_start, chunk2_len);
	memmove(chunk3_dst, chunk3_start, chunk3_len);

	/* terminate string with zero */
	*(chunk3_dst+chunk3_len)='\0';

	/* update message length */
	msg->len = strlen(msg->buf);

	return 0;
}


/*!
 * \brief Callback function that does the work inside the server.
 * \param msg SIP message
 * \param flags unused
 * \param bar unused
 * \return 1 on success, -1 on failure
 */
int ring_filter(struct sip_msg *msg, unsigned int flags, void *bar)
{
	int contains_callid;

	if (msg->first_line.type == SIP_REPLY && msg->first_line.u.reply.statuscode == 183) {
		/* could fail, eg if already parsed, don't care about result */
		parse_headers(msg, HDR_CALLID_F, 0);

		if (msg->callid) {
			lock_get(ring_lock);
			contains_callid=contains(msg->callid->body);
			lock_release(ring_lock);

			if (contains_callid) {
				LM_DBG("converting 183 to 180 for %.*s\n", msg->callid->body.len, msg->callid->body.s);
				if (conv183(msg)!=0) return -1;
			}
		} else {
			LM_ERR("no callid\n");
			return -1;
		}
	}

	return 1;
}


/*!
 * \brief Fixup function for the ring_insert_callid function
 * \param param unused
 * \param param_no unused
 * \return 0
 */
int ring_fixup(void ** param, int param_no) {
	int ring_timeout = cfg_get(siputils, siputils_cfg, ring_timeout);
	if (ring_timeout == 0) {
		LM_ERR("ring_insert_callid functionality deactivated, you need to set a positive ring_timeout\n");
		return -1;
	}
	return 0;
}
