/*
 * $Id$
 *
 * simple UAC for things such as SUBSCRIBE or SMS gateway;
 * no authentication and other UAC features -- just send
 * a message, retransmit and await a reply; forking is not
 * supported during client generation, in all other places
 * it is -- adding it should be simple
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include "../../dprint.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "../../md5utils.h"
#include "../../mem/mem.h"
#include "../../fifo_server.h"
#include "../../error.h"
#include "../../pt.h"
#include "../../crc.h"
#include "t_funcs.h"
#include "config.h"
#include "sip_msg.h"
#include "ut.h"
#include "t_msgbuilder.h"
#include "uac.h"

/* Call-ID has the following form: <callid_nr>-<pid>@<ip>
 * callid_nr is initialized as a random number and continually
 * increases; -<pid>@<ip> is kept in callid_suffix
 */

#define CALLID_SUFFIX_LEN (1 /* - */ + 5 /* pid */ \
	+ 42 /* embedded v4inv6 address can be looong '128.' */ \
	+ 2 /* parenthessis [] */ + 1 /* ZT 0 */ \
	+ 16 /* one never knows ;-) */ )
#define CALLID_NR_LEN 20

/* the character which separates random from constant part */
#define CID_SEP	'-'

/* length of FROM tags */
#define FROM_TAG_LEN (MD5_LEN +1 /* - */ + CRC16_LEN)

static unsigned long callid_nr;
static char *callid_suffix;
static int callid_suffix_len;
static int rand_len;	/* number of chars to display max rand */
static char callid[CALLID_NR_LEN+CALLID_SUFFIX_LEN];

char *uac_from="\"UAC Account\" <sip:uac@dev.null:9>";

str uac_from_str;

static char from_tag[ FROM_TAG_LEN+1 ];



int uac_init() {

	int i; 
	unsigned long uli;
	int rand_len_bits;
	int rand_cnt; /* number of rands() to be long enough */
	int rand_bits; /* length of rands() in bits */
	str src[3];

	if (RAND_MAX<TABLE_ENTRIES) {
		LOG(L_WARN, "Warning: uac does not spread "
			"accross the whole hash table\n");
	}

	/* calculate the initial call-id */

	/* how many bits and chars do we need to display the 
	 * whole ULONG number */
	for (rand_len_bits=0,uli=ULONG_MAX;uli;
			uli>>=1, rand_len_bits++ );
	rand_len=rand_len_bits/4;
	if (rand_len>CALLID_NR_LEN) {
		LOG(L_ERR, "ERROR: Too small callid buffer\n");
		return -1;
	}

	/* how long are the rand()s ? */
	for (rand_bits=0,i=RAND_MAX;i;i>>=1,rand_bits++);
	/* how many rands() fit in the ULONG ? */
	rand_cnt=rand_len_bits / rand_bits;

	/* now fill in the callid with as many random
	 * numbers as you can + 1 */
	callid_nr=rand(); /* this is the + 1 */
	while(rand_cnt) {
		rand_cnt--;
		callid_nr<<=rand_bits;
		callid_nr|=rand();
	}
	callid_suffix=callid+rand_len;
	DBG("CALLID initialization: %lx (len=%d)\n", 
			callid_nr, rand_len );
	DBG("CALLID0=%0*lx\n", rand_len, callid_nr );


	/* calculate the initial From tag */

	src[0].s="Long live SER server";
	src[0].len=strlen(src[0].s);
	src[1].s=sock_info[bind_idx].address_str.s;
	src[1].len=strlen(src[1].s);
	src[2].s=sock_info[bind_idx].port_no_str.s;
	src[2].len=strlen(src[2].s);

	MDStringArray( from_tag, src, 3 );
	from_tag[MD5_LEN]=CID_SEP;

	uac_from_str.s = uac_from;
	uac_from_str.len = strlen(uac_from);

	return 1;
}


int uac_child_init( int rank ) 
{
	callid_suffix_len=snprintf(callid_suffix,CALLID_SUFFIX_LEN,
			"%c%d@%.*s", CID_SEP, my_pid(), 
			sock_info[bind_idx].address_str.len,
			sock_info[bind_idx].address_str.s );
	if (callid_suffix_len==-1) {
		LOG(L_ERR, "ERROR: uac_child_init: buffer too small\n");
		return -1;
	}
	DBG("DEBUG: callid_suffix: %s\n", callid_suffix );
	return 1;
}

int t_uac( str *msg_type, str *dst, 
	str *headers, str *body, str *from, 
	transaction_cb completion_cb, void *cbp, 
	dlg_t dlg)
{

	int r;
	struct cell *new_cell;
	struct proxy_l *proxy;
	int branch;
	int ret;
	unsigned int req_len;
	char *buf;
	union sockaddr_union to;
	struct socket_info* send_sock;
	struct retr_buf *request;
	str dummy_from;
	str callid_s;
	str fromtag;

	/* make -Wall shut up */
	ret=0;

	proxy=uri2proxy( dst );
	if (proxy==0) {
		ser_error=ret=E_BAD_ADDRESS;
		LOG(L_ERR, "ERROR: t_uac: can't create a dst proxy\n");
		goto done;
	}
	branch=0;
	/* might go away -- we ignore it in send_pr_buffer anyway */
	/* T->uac[branch].request.to_len=sizeof(union sockaddr_union); */
	hostent2su(&to, &proxy->host, proxy->addr_idx, 
		(proxy->port)?htons(proxy->port):htons(SIP_PORT));
	send_sock=get_send_socket( &to );
	if (send_sock==0) {
		LOG(L_ERR, "ERROR: t_uac: no corresponding listening socket "
			"for af %d\n", to.s.sa_family );
		ret=E_NO_SOCKET;
		goto error00;
	}

	/* update callid */
	/* generate_callid(); */
	callid_nr++;
	r=snprintf(callid, rand_len+1, "%0*lx", rand_len, callid_nr );
	if (r==-1) {
		LOG(L_CRIT, "BUG: SORRY, callid calculation failed\n");
		goto error00;
	}
	/* fix the ZT 0 */
	callid[rand_len]=CID_SEP;
	callid_s.s=callid;
	callid_s.len=rand_len+callid_suffix_len;
	DBG("DEBUG: sufix_len = %d\n",callid_suffix_len);
	DBG("DEBUG: NEW CALLID:%.*s[%d]:\n", callid_s.len, callid_s.s 
		, callid_s.len);
	new_cell = build_cell( NULL ) ; 
	if (!new_cell) {
		ret=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: t_uac: short of cell shmem\n");
		goto error00;
	}
	new_cell->completion_cb=completion_cb;
	new_cell->cbp=cbp;
	/* cbp is installed -- tell error handling bellow not to free it */
	cbp=0;
	new_cell->is_invite=msg_type->len==INVITE_LEN 
		&& memcmp(msg_type->s, INVITE, INVITE_LEN)==0;
	new_cell->local=1;
	new_cell->kr=REQ_FWDED;


	request=&new_cell->uac[branch].request;
	request->to=to;
	request->send_sock=send_sock;

	/* need to put in table to calculate label which is needed for printing */
	LOCK_HASH(new_cell->hash_index);
	insert_into_hash_table_unsafe(  new_cell );
	UNLOCK_HASH(new_cell->hash_index);

	if (from) dummy_from=*from; else { dummy_from.s=0; dummy_from.len=0; }
	/* calculate from tag from callid */
	crcitt_string_array(&from_tag[MD5_LEN+1], &callid_s, 1 );
	fromtag.s=from_tag; fromtag.len=FROM_TAG_LEN;
	buf=build_uac_request(  *msg_type, *dst, 
			dummy_from, fromtag,
			DEFAULT_CSEQ, callid_s, 
			*headers, *body, branch,
			new_cell, /* t carries hash_index, label, md5,
				uac[].send_sock and other pieces of
				information needed to print a message*/
		&req_len );
	if (!buf) {
		ret=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: t_uac: short of req shmem\n");
		goto error01;
	}      
	new_cell->method.s=buf;new_cell->method.len=msg_type->len;


	request->buffer = buf;
	request->buffer_len = req_len;
	new_cell->nr_of_outgoings++;


	proxy->tx++;
	proxy->tx_bytes+=req_len;

	if (SEND_BUFFER( request)==-1) {
		LOG(L_ERR, "ERROR: t_uac: UAC sending to %.*s failed\n",
			dst->len, dst->s );
		proxy->errors++;
		proxy->ok=0;
	}
	start_retr( request );

	/* success */
	return 1;

error01:
	LOCK_HASH(new_cell->hash_index);
	remove_from_hash_table_unsafe( new_cell );
	UNLOCK_HASH(new_cell->hash_index);
	free_cell(new_cell);
error00:
	free_proxy( proxy );
	free( proxy );
done: 
	/* if we did not install cbp, release it now */
	if (cbp) shm_free(cbp);
	return ser_error=ret;
}


/*
 * Send a request within a dialog
 */
int t_uac_dlg(str* msg,                     /* Type of the message - MESSAGE, OPTIONS etc. */
	      str* dst,                     /* Real destination (can be different than R-URI) */
	      str* ruri,                    /* Request-URI */
	      str* to,                      /* To - w/o tag*/
	      str* from,                    /* From - w/o tag*/
	      str* totag,                   /* To tag */
	      str* fromtag,                 /* From tag */
	      int* cseq,                    /* Variable holding CSeq */
	      str* cid,                     /* Call-ID */
	      str* headers,                 /* Optional headers including CRLF */
	      str* body,                    /* Message body */
	      transaction_cb completion_cb, /* Callback parameter */
	      void* cbp                     /* Callback pointer */
	      )
{

	int r, branch, ret;
	unsigned int req_len;
	char *buf;
	struct cell *new_cell;
	struct proxy_l *proxy;
	union sockaddr_union to_su;
	struct socket_info* send_sock;
	struct retr_buf *request;
	str callid_s, ftag, tmp;

	/* make -Wall shut up */
	ret=0;

	proxy = uri2proxy((dst) ? (dst) : ((ruri) ? (ruri) : (to)));
	if (proxy == 0) {
		ser_error = ret = E_BAD_ADDRESS;
		LOG(L_ERR, "ERROR: t_uac_dlg: Can't create a dst proxy\n");
		goto done;
	}

	branch=0;
	hostent2su(&to_su, &proxy->host, proxy->addr_idx, (proxy->port) ? htons(proxy->port) : htons(SIP_PORT));
	send_sock=get_send_socket(&to_su);
	if (send_sock == 0) {
		LOG(L_ERR, "ERROR: t_uac_dlg: no corresponding listening socket for af %d\n", to_su.s.sa_family );
		ret = E_NO_SOCKET;
		goto error00;
	}
	
	     /* No Call-ID given, calculate it */
	if (cid == 0) {
		callid_nr++;
		r = snprintf(callid, rand_len + 1, "%0*lx", rand_len, callid_nr);
		if (r == -1) {
			LOG(L_CRIT, "BUG: SORRY, callid calculation failed\n");
			goto error00;
		}

		     /* fix the ZT 0 */
		callid[rand_len] = CID_SEP;
		callid_s.s = callid;
		callid_s.len = rand_len + callid_suffix_len;
	}

	new_cell = build_cell(0); 
	if (!new_cell) {
		ret = E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: t_uac: short of cell shmem\n");
		goto error00;
	}

	new_cell->completion_cb = completion_cb;
	new_cell->cbp = cbp;

	/* cbp is installed -- tell error handling bellow not to free it */
	cbp = 0;

	new_cell->is_invite = msg->len == INVITE_LEN && memcmp(msg->s, INVITE, INVITE_LEN) == 0;
	new_cell->local= 1 ;
	new_cell->kr = REQ_FWDED;

	request = &new_cell->uac[branch].request;
	request->to = to_su;
	request->send_sock = send_sock;

	/* need to put in table to calculate label which is needed for printing */
	LOCK_HASH(new_cell->hash_index);
	insert_into_hash_table_unsafe(new_cell);
	UNLOCK_HASH(new_cell->hash_index);

	if (fromtag == 0) {
		     /* calculate from tag from callid */
		crcitt_string_array(&from_tag[MD5_LEN + 1], (cid) ? (cid) : (&callid_s), 1);
		ftag.s = from_tag; 
		ftag.len = FROM_TAG_LEN;
	}

	buf = build_uac_request_dlg(msg, 
				    (ruri) ? (ruri) : (to),
				    to, 
				    (from) ? (from) : (&uac_from_str), 
				    totag,
				    (fromtag) ? (fromtag) : (&ftag), 
				    (cseq) ? (*cseq) : DEFAULT_CSEQ, 
				    (cid) ? (cid) : (&callid_s), 
				    headers, 
				    body, 
				    branch,
				    new_cell,
				    &req_len);
	if (!buf) {
		ret = E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: t_uac: short of req shmem\n");
		goto error01;
	}
	new_cell->method.s = buf;
	new_cell->method.len = msg->len;

	request->buffer = buf;
	request->buffer_len = req_len;
	new_cell->nr_of_outgoings++;

	proxy->tx++;
	proxy->tx_bytes += req_len;

	if (SEND_BUFFER(request) == -1) {
		if (dst) {
			tmp = *dst;
		} else if (ruri) {
			tmp = *ruri;
		} else {
			tmp = *to;
		}
		LOG(L_ERR, "ERROR: t_uac: UAC sending to \'%.*s\' failed\n", tmp.len, tmp.s);
		proxy->errors++;
		proxy->ok = 0;
	}
	
	start_retr(request);

	/* success */
	return 1;

error01:
	LOCK_HASH(new_cell->hash_index);
	remove_from_hash_table_unsafe(new_cell);
	UNLOCK_HASH(new_cell->hash_index);
	free_cell(new_cell);

error00:
	free_proxy(proxy);
	free(proxy);

done: 
	/* if we did not install cbp, release it now */
	if (cbp) shm_free(cbp);
	return ser_error = ret;
}


static void fifo_callback( struct cell *t, struct sip_msg *msg,
	int code, void *param)
{

	char *filename;
	str text;

	DBG("DEBUG: fifo UAC completed with status %d\n", code);
	if (!t->cbp) {
		LOG(L_INFO, "INFO: fifo UAC completed with status %d\n", code);
		return;
	}

	filename=(char *)(t->cbp);
	get_reply_status(&text,msg,code);
	if (text.s==0) {
		LOG(L_ERR, "ERROR: fifo_callback: get_reply_status failed\n");
		fifo_reply(filename, "500 fifo_callback: get_reply_status failed\n");
		return;
	}
	fifo_reply(filename, "%.*s", text.len, text.s );
	pkg_free(text.s);
	DBG("DEBUG: fifo_callback sucesssfuly completed\n");
}	

/* to be obsoleted in favor of fifo_uac_from */
int fifo_uac( FILE *stream, char *response_file ) 
{
	char method[MAX_METHOD];
	char header[MAX_HEADER];
	char body[MAX_BODY];
	char dst[MAX_DST];
	str sm, sh, sb, sd;
	char *shmem_file;
	int fn_len;
	int ret;
	int sip_error;
	char err_buf[MAX_REASON_LEN];

	sm.s=method; sh.s=header; sb.s=body; sd.s=dst;
	if (!read_line(method, MAX_METHOD, stream,&sm.len)||sm.len==0) {
		/* line breaking must have failed -- consume the rest
		   and proceed to a new request
		*/
		LOG(L_ERR, "ERROR: fifo_uac: method expected\n");
		fifo_reply(response_file, 
			"400 fifo_uac: method expected");
		return 1;
	}
	DBG("DEBUG: fifo_uac: method: %.*s\n", sm.len, method );
	if (!read_line(dst, MAX_DST, stream, &sd.len)||sd.len==0) {
		fifo_reply(response_file, 
			"400 fifo_uac: destination expected\n");
		LOG(L_ERR, "ERROR: fifo_uac: destination expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_uac:  dst: %.*s\n", sd.len, dst );
	/* now read header fields line by line */
	if (!read_line_set(header, MAX_HEADER, stream, &sh.len)) {
		fifo_reply(response_file, 
			"400 fifo_uac: HFs expected\n");
		LOG(L_ERR, "ERROR: fifo_uac: header fields expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_uac: header: %.*s\n", sh.len, header );
	/* and eventually body */
	if (!read_body(body, MAX_BODY, stream, &sb.len)) {
		fifo_reply(response_file, 
			"400 fifo_uac: body expected\n");
		LOG(L_ERR, "ERROR: fifo_uac: body expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_uac: body: %.*s\n", sb.len, body );
	DBG("DEBUG: fifo_uac: EoL -- proceeding to transaction creation\n");
	/* we got it all, initiate transaction now! */
	if (response_file) {
		fn_len=strlen(response_file)+1;
		shmem_file=shm_malloc(fn_len);
		if (shmem_file==0) {
			LOG(L_ERR, "ERROR: fifo_uac: no shmem\n");
			fifo_reply(response_file, 
				"500 fifo_uac: no shmem for shmem_file\n");
			return 1;
		}
		memcpy(shmem_file, response_file, fn_len );
	} else {
		shmem_file=0;
	}
	ret=t_uac(&sm,&sd,&sh,&sb, 0 /* default from */,
		fifo_callback,shmem_file,0 /* no dialog */);
	if (ret>0) {
		if (err2reason_phrase(ret, &sip_error, err_buf,
				sizeof(err_buf), "FIFO/UAC" ) > 0 ) 
		{
			fifo_reply(response_file, "500 FIFO/UAC error: %d\n",
				ret );
		} else {
			fifo_reply(response_file, err_buf );
		}
	}
	return 1;
}

/* syntax:

	:t_uac_from:[file] EOL
	method EOL
	[from] EOL (if none, server's default from is taken)
	dst EOL (put in r-uri and To)
	[CR-LF separated HFs]* EOL
	EOL
	[body] EOL
	EOL

*/

int fifo_uac_from( FILE *stream, char *response_file ) 
{
	char method[MAX_METHOD];
	char header[MAX_HEADER];
	char body[MAX_BODY];
	char dst[MAX_DST];
	char from[MAX_FROM];
	str sm, sh, sb, sd, sf;
	char *shmem_file;
	int fn_len;
	int ret;
	int sip_error;
	char err_buf[MAX_REASON_LEN];
	int err_ret;

	sm.s=method; sh.s=header; sb.s=body; sd.s=dst;sf.s=from;

	if (!read_line(method, MAX_METHOD, stream,&sm.len)||sm.len==0) {
		/* line breaking must have failed -- consume the rest
		   and proceed to a new request
		*/
		LOG(L_ERR, "ERROR: fifo_uac: method expected\n");
		fifo_reply(response_file, 
			"400 fifo_uac: method expected");
		return 1;
	}
	DBG("DEBUG: fifo_uac: method: %.*s\n", sm.len, method );
	if (!read_line(from, MAX_FROM, stream, &sf.len)) {
		fifo_reply(response_file, 
			"400 fifo_uac: from expected\n");
		LOG(L_ERR, "ERROR: fifo_uac: from expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_uac:  from: %.*s\n", sf.len, from);
	if (!read_line(dst, MAX_DST, stream, &sd.len)||sd.len==0) {
		fifo_reply(response_file, 
			"400 fifo_uac: destination expected\n");
		LOG(L_ERR, "ERROR: fifo_uac: destination expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_uac:  dst: %.*s\n", sd.len, dst );
	/* now read header fields line by line */
	if (!read_line_set(header, MAX_HEADER, stream, &sh.len)) {
		fifo_reply(response_file, 
			"400 fifo_uac: HFs expected\n");
		LOG(L_ERR, "ERROR: fifo_uac: header fields expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_uac: header: %.*s\n", sh.len, header );
	/* and eventually body */
	if (!read_body(body, MAX_BODY, stream, &sb.len)) {
		fifo_reply(response_file, 
			"400 fifo_uac: body expected\n");
		LOG(L_ERR, "ERROR: fifo_uac: body expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_uac: body: %.*s\n", sb.len, body );
	DBG("DEBUG: fifo_uac: EoL -- proceeding to transaction creation\n");
	/* we got it all, initiate transaction now! */
	if (response_file) {
		fn_len=strlen(response_file)+1;
		shmem_file=shm_malloc(fn_len);
		if (shmem_file==0) {
			LOG(L_ERR, "ERROR: fifo_uac: no shmem\n");
			fifo_reply(response_file, 
				"500 fifo_uac: no memory for shmem_file\n");
			return 1;
		}
		memcpy(shmem_file, response_file, fn_len );
	} else {
		shmem_file=0;
	}
	/* HACK: there is yet a shortcoming -- if t_uac fails, callback
	   will not be triggered and no feedback will be printed
	   to shmem_file
	*/
	ret=t_uac(&sm,&sd,&sh,&sb, sf.len==0 ? 0 : &sf /* default from */,
		fifo_callback,shmem_file,0 /* no dialog */);
	if (ret<=0) {
		err_ret=err2reason_phrase(ret, &sip_error, err_buf,
				sizeof(err_buf), "FIFO/UAC" ) ;
		if (err_ret > 0 )
		{
			fifo_reply(response_file, "%d %s", sip_error, err_buf );
		} else {
			fifo_reply(response_file, "500 FIFO/UAC error: %d\n",
				ret );
		}
	}
	return 1;

}
