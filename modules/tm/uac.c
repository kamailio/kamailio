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
 *
 * ***************************************************
 *             IMPORTANT NOTE
 *
 *    All UACs but t_uac_dlg are being deprecated now
 *    and will be removed from future versions of TM
 *    module. Eliminate all dependancies on them asap.
 *    For backwards compatibility (NOT RECOMMENDED)
 *    turn off DEPRECATE_OLD_STUFF in defs.h. Similarly,
 *    there is a new FIFO UAC.
 *
 * ****************************************************
 *
 * History:
 * --------
 * 2003-03-01  kr set through a function now (jiri)
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-02-27 FIFO/UAC now dumps reply -- good for CTD (jiri)
 * 2003-02-13  t_uac, t _uac_dlg, gethfblock, uri2proxy changed to use 
 *              proto & rb->dst (andrei)
 * 2003-01-29  scratchpad removed (jiri)
 * 2003-01-27  fifo:t_uac_dlg completed (jiri)
 * 2003-01-23  t_uac_dlg now uses get_out_socket (jiri)
 */


#include "../../comp_defs.h"
#include "defs.h"


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
#include "../../parser/parse_from.h"
#include "../../parser/msg_parser.h"
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

/* header fields which are explicitely processed and are not copied
 * from FIFO line-by-line
 */
#define skip_hf(_hf) (((_hf)->type==HDR_FROM) || ((_hf)->type==HDR_TO) \
	|| ((_hf)->type==HDR_CALLID) || ((_hf)->type==HDR_CSEQ))

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

struct str_list {
	str s;
	struct str_list *next;
};

static unsigned long callid_nr;
static char *callid_suffix;
static int callid_suffix_len;
static int rand_len;	/* number of chars to display max rand */
static char callid[CALLID_NR_LEN+CALLID_SUFFIX_LEN];

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

	return 1;
}


int uac_child_init( int rank ) 
{
	callid_suffix_len=snprintf(callid_suffix,CALLID_SUFFIX_LEN,
			"%c%d@%.*s", CID_SEP, my_pid(), 
			sock_info[bind_idx].address_str.len,
			sock_info[bind_idx].address_str.s );
	if (callid_suffix_len==-1 || callid_suffix_len>=CALLID_SUFFIX_LEN) {
		LOG(L_ERR, "ERROR: uac_child_init: buffer too small\n");
		return -1;
	}
	DBG("DEBUG: callid_suffix: %s\n", callid_suffix );
	return 1;
}

static struct socket_info *uri2sock( str *uri, union sockaddr_union *to_su,
									 int proto )
{
	struct proxy_l *proxy;
	struct socket_info* send_sock;

	proxy = uri2proxy(uri, proto);
	if (proxy == 0) {
		ser_error = E_BAD_ADDRESS;
		LOG(L_ERR, "ERROR: uri2sock: Can't create a dst proxy\n");
		return 0;
	}

	hostent2su(to_su, &proxy->host, proxy->addr_idx, 
			(proxy->port) ? htons(proxy->port) : htons(SIP_PORT));
	send_sock=get_out_socket(to_su, proto);
	if (send_sock == 0) {
		LOG(L_ERR, "ERROR: uri2sock: no corresponding socket for af %d\n", 
						to_su->s.sa_family );
		ser_error = E_NO_SOCKET;
	}


	free_proxy(proxy);
	free(proxy);
	return send_sock;
}
	


/*
 * Send a request within a dialog
 * 
 * Some parameters are required, some are optional (i.e., ephemeral,
 * default or empty values are created if 0 is passed as parameter). The
 * optional parameters are typically used to set some header fields
 * to dialog-related values (as opposed to having them set to
 * ephemeral values).
 *
 * Required:
 * - msg ..   specifies type of message, such as "OPTIONS"
 * - ruri ..  specifies request URI; 
 * - from ..  value of From header field (if it already includes from tag, 
 *            the fromtag parameter MUST point to en empty string; if 
 *            fromtag is 0, an ephemeral tag is always appended)
 * - to ...   value of To header field (if it already includes to tag in it,
 *            or you do not wish to set a to-tag the totag parameter MUST be 0)
 * 
 * Optional:
 * - dst     transport destination (expressed as URI) -- if present,
 *           request is physically forwarded to address indicated in it,
 *           overriding the transport address in ruri; useful for use with 
 *           outbound proxies or loose routers (that is where the first 
 *           element of route set comes in)
 * - fromtag from HF tag -- dialog-less applications do not to set it (==0),
 *           in which case an ephemeral value is created; if fromtag present,
 *           its appended to the From header field; it may be also present 
 *           and point to an empty string -- that only makes sense if
 *           application includes the tag in From and does not care to
 *           separate the tag from the rest of header field
 * - totag   To HF tag; if 0, no to-tag is appended (unless it is already
 *           part of to)
 * - cid ..  callid; if 0, ephemeral value is created; transactions
 *           within a dialog need to set this value to dialog's callid
 * - cseq .. CSeq; if 0, default value (DEFAULT_CSEQ) is used; transactions
 *           within a dialog need to set this value to current local cseq,
 *           which grows continously with transactions sent
 * - headers .. block of header fields that will be included in the
 *           message. It MAY NOT include header fields already described
 *           in other parameters (From, to, cid, cseq) or created 
 *           automatically   (Content_length)   otherwise the parameter
 *           would appear multiple times. It MUST include all additional
 *           header fields required for a given SIP message, like Content-Type 
 *           for any messages including a body or Contact for INVITEs.
 * - body .. if present, body and Content-Length is appended to the 
 *           SIP message; Content-Type then needs to be present inside
 *           'headers' parameter
 * - cb ..   callback to be called when transaction completes; if none
 *           present, no callback will be called
 * - cbp ..  callback parameter -- value passed to callback function
 *           when called
 *
 */
int t_uac_dlg(str* msg,                     /* Type of the message - MESSAGE, OPTIONS etc. */
	      str* dst,                     /* Real destination (can be different than R-URI) */
		  int proto,
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
	struct socket_info* send_sock;
	struct retr_buf *request;
	str callid_s, ftag, tmp;
	union sockaddr_union to_su;

	/* make -Wall shut up */
	ret=0;

	/* check for invalid parameter */
	if (!msg || !msg->s
				|| !ruri || !ruri->s
				|| !from || !from->s
				|| !to || !to->s ) {
		LOG(L_ERR, "ERROR: t_uac_dlg: invalid parameters\n");
		ser_error = ret = E_INVALID_PARAMS;
		goto done;
	}

	send_sock=uri2sock( dst? dst: ruri, &to_su, proto );
	if (send_sock==0) {
		LOG(L_ERR, "ERROR: t_uac_dlg: no socket found\n");
		goto error00;
	}

	branch=0;
	     /* No Call-ID given, calculate it */
	if (cid == 0) {
		callid_nr++;
		r = snprintf(callid, rand_len + 1, "%0*lx", rand_len, callid_nr);
		if (r == -1 || r>=rand_len+1) {
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
	set_kr(REQ_FWDED);

	request = &new_cell->uac[branch].request;
	request->dst.to = to_su;
	request->dst.send_sock = send_sock;
	request->dst.proto = proto;
	request->dst.proto_reserved1 = 0;

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
				    ruri,
				    to, 
				    from,
				    totag,
				    (fromtag) ? (fromtag) : (&ftag), 
				    (cseq) ? (*cseq) : DEFAULT_CSEQ, 
				    (cid) ? (cid) : (&callid_s), 
				    headers, 
				    body, 
				    branch,
				    new_cell,
				    &req_len,
					send_sock);
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

/*
	proxy->tx++;
	proxy->tx_bytes += req_len;
*/

	if (SEND_BUFFER(request) == -1) {
		if (dst) {
			tmp = *dst;
		} else {
			tmp = *ruri;
		}
		LOG(L_ERR, "ERROR: t_uac: UAC sending to \'%.*s\' failed\n", tmp.len, tmp.s);
/*
		proxy->errors++;
		proxy->ok = 0;
*/
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
/*
	free_proxy(proxy);
	free(proxy);
*/

done: 
	/* if we did not install cbp, release it now */
	if (cbp) shm_free(cbp);
	return ser_error = ret;
}


static void fifo_callback( struct cell *t, struct sip_msg *reply,
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
	if (reply==FAKED_REPLY) {
		get_reply_status(&text,reply,code);
		if (text.s==0) {
			LOG(L_ERR, "ERROR: fifo_callback: get_reply_status failed\n");
			fifo_reply(filename, "500 fifo_callback: get_reply_status failed\n");
			return;
		}
		fifo_reply(filename, "%.*s", text.len, text.s );
		pkg_free(text.s);
	} else {
		text.s=reply->first_line.u.reply.status.s;
		text.len=reply->len-(reply->first_line.u.reply.status.s-reply->buf);
		fifo_reply(filename, "%.*s", text.len, text.s );
	}
	DBG("DEBUG: fifo_callback sucesssfuly completed\n");
}	

static struct str_list *new_str(char *s, int len, struct str_list **last, int *total)
{
	struct str_list *new;
	new=pkg_malloc(sizeof(struct str_list));
	if (!new) {
		LOG(L_ERR, "ERROR: get_hfblock: not enough mem\n");
		return 0;
	}
	new->s.s=s;
	new->s.len=len;
	new->next=0;

	(*last)->next=new;
	*last=new;
	*total+=len;

	return new;
}


static char *get_hfblock(str *uri, struct hdr_field *hf, int *l, int proto) 
{
	struct str_list sl, *last, *new, *i, *foo;
	int hf_avail, frag_len, total_len;
	char *begin, *needle, *dst, *ret, *d;
	str *sock_name, *portname;
	union sockaddr_union to_su;
	struct socket_info* send_sock;

	ret=0; /* pesimist: assume failure */
	total_len=0;
	last=&sl;
	last->next=0;
	portname=sock_name=0;

	for (; hf; hf=hf->next) {
		if (skip_hf(hf)) continue;

		begin=needle=hf->name.s; 
		hf_avail=hf->len;

		/* substitution loop */
		while(hf_avail) {
			d=memchr(needle, SUBST_CHAR, hf_avail);
			if (!d || d+1>=needle+hf_avail) { /* nothing to substitute */
				new=new_str(begin, hf_avail, &last, &total_len); 
				if (!new) goto error;
				break;
			} else {
				frag_len=d-begin;
				d++; /* d not at the second substitution char */
				switch(*d) {
					case SUBST_CHAR:	/* double SUBST_CHAR: IP */
						/* string before substitute */
						new=new_str(begin, frag_len, &last, &total_len); 
						if (!new) goto error;
						/* substitute */
						if (!sock_name) {
							send_sock=uri2sock( uri, &to_su, proto );
							if (!send_sock) {
								LOG(L_ERR, "ERROR: get_hf_block: send_sock failed\n");
								goto error;
							}
							sock_name=&send_sock->address_str;
							portname=&send_sock->port_no_str;
						}
						new=new_str(sock_name->s, sock_name->len,
								&last, &total_len );
						if (!new) goto error;
						new=new_str(portname->s, portname->len,
								&last, &total_len );
						if (!new) goto error;
						/* keep going ... */
						begin=needle=d+1;hf_avail-=frag_len+2;
						continue;
					default:
						/* no valid substitution char -- keep going */
						hf_avail-=frag_len+1;
						needle=d;
				}
			} /* possible substitute */
		} /* substitution loop */
		/* proceed to next header */
		/* new=new_str(CRLF, CRLF_LEN, &last, &total_len );
		if (!new) goto error; */
		DBG("DEBUG: get_hf_block: one more hf processed\n");
	} /* header loop */


	/* construct a single header block now */
	ret=pkg_malloc(total_len);
	if (!ret) {
		LOG(L_ERR, "ERROR: get_hf_block no pkg mem for hf block\n");
		goto error;
	}
	i=sl.next;
	dst=ret;
	while(i) {
		foo=i;
		i=i->next;
		memcpy(dst, foo->s.s, foo->s.len);
		dst+=foo->s.len;
		pkg_free(foo);
	}
	*l=total_len;
	return ret;

error:
	i=sl.next;
	while(i) {
		foo=i;
		i=i->next;
		pkg_free(foo);
	}
	*l=0;
	return 0;
}

static void fifo_uac_error(char *reply_fifo, int code, char *msg)
{
	LOG(L_ERR, "ERROR: fifo_uac: %s\n", msg ); 
	fifo_reply(reply_fifo, "%d fifo_uac: %s", code, msg);
}

/* syntax:

	:t_uac_dlg:[file] EOL
	method EOL
	r-uri EOL 
	dst EOL 				// ("." if no outbound server used)
							// must be used with dialogs/lr
	<EOL separated HFs>+	// From and To must be present at least;
							// dialog-apps must include tag in From
 							// (an ephemeral is appended otherwise)
							// and supply CSeq/CallId
	.[EOL]
	[body] 
	.EOL


	there is also the possibility to have server placed its
    hostname:portnumber in header fields -- just put double
	exclamation mark in any of the optional header fields
	(i.e., any but From/To/CallID,CSeq), they will be 
	substituted hn:pn

Example:

sc fifo t_uac_dlg MESSAGE sip:joe@192.168.2.1 \
	. \ # no outbound proxy
	'From:sender@iptel.org;tagd=123'  \ # no to-tag -> ephemeral
	'To:sender@iptel.org' \
	'Foo: sip:user@!! '  \ # expansion here
	'CSEQ: 11 MESSAGE   ' \
	. \ # EoH
	.	# empty body
---
U 192.168.2.16:5060 -> 192.168.2.1:5060
MESSAGE sip:joe@192.168.2.1 SIP/2.0..
Via: SIP/2.0/UDP 192.168.2.16;branch=z9hG4bK760c.922ea6a1.0..
To: sender@iptel.org..
From: sender@iptel.org;tagd=123;tag=5405e669bc2980663aed2624dc31396f-fa77..
CSeq: 11 MESSAGE..
Call-ID: e863bf56-22255@192.168.2.16..
Content-Length: 0..
User-Agent: Sip EXpress router (0.8.11pre4-tcp1-locking (i386/linux))..
Foo: sip:user@192.168.2.16:5060..
..


*/

int fifo_uac_dlg( FILE *stream, char *response_file ) 
{
	char method_buf[MAX_METHOD];
	char ruri_buf[MAX_URI_SIZE];
	char outbound_buf[MAX_URI_SIZE];
	char header_buf[MAX_HEADER]; 
	char body_buf[MAX_BODY]; 
	str method, ruri, outbound, header, body;
	str hfb; /* header field block */
	struct sip_uri parsed_ruri, parsed_outbound;
	str dummy_empty;
	int fromtag;
	int cseq;
	struct cseq_body *parsed_cseq;
	int i;
	char c;
	struct to_body *parsed_from;


	char *shmem_file;
	int fn_len;
	int ret;
	int sip_error;
	char err_buf[MAX_REASON_LEN];
	int err_ret;
	struct sip_msg faked_msg;


	if (!read_line(method_buf, MAX_METHOD, stream,&method.len)
					||method.len==0) {
		/* line breaking must have failed -- consume the rest
		   and proceed to a new request
		*/
		fifo_uac_error(response_file, 400, "method expected");
		return 1;
	}
	method.s=method_buf;
	DBG("DEBUG: fifo_uac: method: %.*s\n", method.len, method.s );

	if (!read_line(ruri_buf, MAX_URI_SIZE, stream, &ruri.len)
					|| ruri.len==0) {
		fifo_uac_error(response_file, 400, "ruri expected");
		return 1;
	}
	if (parse_uri(ruri_buf, ruri.len, &parsed_ruri) < 0 ) {
		fifo_uac_error(response_file, 400, "ruri invalid\n");
		return 1;
	}
	ruri.s=ruri_buf;
	DBG("DEBUG: fifo_uac:  ruri: %.*s\n", ruri.len, ruri.s);

	if (!read_line(outbound_buf, MAX_URI_SIZE, stream, &outbound.len)
					||outbound.len==0) {
		fifo_uac_error(response_file, 400, "outbound address expected\n");
		return 1;
	}
	if (outbound.len==1 && outbound_buf[0]=='.' ) {
		DBG("DEBUG: fifo_uac: outbound empty\n");
		outbound.s=0; outbound.len=0;
	} else if (parse_uri(outbound_buf, outbound.len, 
							&parsed_outbound) < 0 ) {
		fifo_uac_error(response_file, 400, "outbound uri invalid\n");
		return 1;
	} else {
		outbound.s=outbound_buf;
		DBG("DEBUG: fifo_uac:  dst: %.*s\n", outbound.len, outbound.s);
	}


	/* now read and parse header fields */
	if (!read_line_set(header_buf, MAX_HEADER, stream, &header.len)
					|| header.len==0 ) {
		fifo_uac_error(response_file, 400, "HFs expected");
		return 1;
	}
	header.s=header_buf;
	DBG("DEBUG: fifo_uac: header: %.*s\n", header.len, header.s );
	/* use SIP parser to look at what is in the FIFO request */
	memset(&faked_msg, 0, sizeof(struct sip_msg));
	faked_msg.len=header.len; 
	faked_msg.buf=faked_msg.unparsed=header_buf;
	if (parse_headers(&faked_msg, HDR_EOH, 0)==-1 ) {
			DBG("DEBUG: fifo_uac: parse_headers failed\n");
			fifo_uac_error(response_file, 400, "HFs unparseable");
			goto error;
	}
	DBG("DEBUG: fifo_uac: parse_headers succeeded\n");

	/* and eventually body */
	if (!read_body(body_buf, MAX_BODY, stream, &body.len)) {
		fifo_uac_error(response_file, 400, "body expected");
		goto error;
	}
	body.s=body_buf;
	DBG("DEBUG: fifo_uac: body: %.*s\n", body.len, body.s );


	/* at this moment, we collected all the things we got, let's
	 * verify user has not forgotten something */
	if (body.len && !faked_msg.content_type) {
		fifo_uac_error(response_file, 400, "Content_type missing");
		goto error;
	}
	if (body.len && faked_msg.content_length) {
		fifo_uac_error(response_file, 400, "Content_length disallowed");
		goto error;
	}
	if (!faked_msg.to) {
		fifo_uac_error(response_file, 400, "To missing");
		goto error;
	}
	if (!faked_msg.from) {
		fifo_uac_error(response_file, 400, "From missing");
		goto error;
	}
	/* we also need to know if there is from-tag and add it otherwise */
	if (parse_from_header(&faked_msg)<0) {
		fifo_uac_error(response_file, 400, "Error in From");
		goto error;
	}
	parsed_from=(struct to_body*)faked_msg.from->parsed;
	fromtag=parsed_from->tag_value.s &&
			parsed_from->tag_value.len;
	cseq=0;
	if (faked_msg.cseq && (parsed_cseq=get_cseq(&faked_msg))) {
		for (i=0; i<parsed_cseq->number.len; i++ ) {
			c=parsed_cseq->number.s[i];
			if (c>='0' && c<'9' ) cseq=cseq*10+c-'0';
			else {
				fifo_uac_error(response_file, 400, "non-nummerical CSeq");
				goto error;
			}
		}
		if (parsed_cseq->method.len!=method.len 
				|| memcmp(parsed_cseq->method.s, method.s, method.len)!=0) {
			fifo_uac_error(response_file, 400, "CSeq method mismatch");
			goto error;
		}
	}

	hfb.s=get_hfblock(outbound.len ? &outbound : &ruri, 
					faked_msg.headers, &hfb.len, PROTO_UDP);
	if (!hfb.s) {
		fifo_uac_error(response_file, 500, "no mem for hf block");
		goto error;
	}


	DBG("DEBUG: fifo_uac: EoL -- proceeding to transaction creation\n");
	/* we got it all, initiate transaction now! */
	if (response_file) {
		fn_len=strlen(response_file)+1;
		shmem_file=shm_malloc(fn_len);
		if (shmem_file==0) {
			fifo_uac_error(response_file, 500, "no shmem");
			goto error01;
		}
		memcpy(shmem_file, response_file, fn_len );
	} else {
		shmem_file=0;
	}
	/* HACK: there is yet a shortcoming -- if t_uac fails, callback
	   will not be triggered and no feedback will be printed
	   to shmem_file
	*/
	dummy_empty.s=0; dummy_empty.len=0;
	ret=t_uac_dlg( &method, 
		outbound.len ? &outbound: 0,
		PROTO_UDP,
		&ruri, 
		&faked_msg.to->body,	/* possibly w/to-tag in it */
		&faked_msg.from->body,
		&dummy_empty,			/* if present, to-tag passed in to */
		fromtag ? 				/* if fromtag present, ... */
			&dummy_empty: 		/* ... pass it in from ... */
			0,					/* use ephemeral otherwise */
		cseq ? &cseq : 0,
		faked_msg.callid ?
			&faked_msg.callid->body:
			0,
		&hfb, 						/* headers -- TBD */
		&body,
		fifo_callback, shmem_file );


	if (ret<=0) {
		err_ret=err2reason_phrase(ret, &sip_error, err_buf,
				sizeof(err_buf), "FIFO/UAC" ) ;
		if (err_ret > 0 )
		{

			fifo_uac_error(response_file, sip_error, err_buf);
		} else {
			fifo_uac_error(response_file, 500, "FIFO/UAC error" );
		}
	}

error01:
	pkg_free(hfb.s);

error:
	/* free_sip_msg(&faked_msg); */
	if (faked_msg.headers) free_hdr_field_lst(faked_msg.headers);
	return 1;
}
