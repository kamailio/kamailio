/*
 * $Id$
 *
 * simple UAC for things such as SUBSCRIBE or SMS gateway;
 * no authentication and other UAC features -- just send
 * a message, retransmit and await a reply; forking is not
 * supported during client generation, in all other places
 * it is -- adding it should be simple
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
#include "../../dprint.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "../../md5utils.h"
#include "../../mem/mem.h"
#include "../../fifo_server.h"
#include "../../error.h"
#include "t_funcs.h"
#include "config.h"
#include "sip_msg.h"
#include "ut.h"
#include "t_msgbuilder.h"
#include "uac.h"

/* Call-ID has the following form: call_id_rand-pid-seq */

char call_id[RAND_DIGITS+1+MAX_PID_LEN+1+MAX_SEQ_LEN+1];
static unsigned long callid_seq;

char *uac_from="\"UAC Account\" <sip:uac@dev.null:9>";

char from_tag[ MD5_LEN +1];

void uac_init() {
	unsigned long init_nr;
	char *c;
	int len;

	str src[3];

	init_nr=random() % (1<<(RAND_DIGITS*4));
	c=call_id;
	len=RAND_DIGITS;
	int2reverse_hex( &c, &len, init_nr );
	while (len) { *c='z'; len--; c++; }
	*c='-';

	src[0].s="Long live SER server";
	src[0].len=strlen(src[0].s);
	src[1].s=sock_info[0].address_str.s;
	src[1].len=strlen(src[1].s);
	src[2].s=sock_info[0].port_no_str.s;
	src[2].len=strlen(src[2].s);

	MDStringArray( from_tag, src, 3 );
	from_tag[MD5_LEN]=0;
}


void uac_child_init( int rank ) {
	int pid_nr;
	char *c;
	int len;

	pid_nr=getpid() % (1<<(MAX_PID_LEN*4));
	c=call_id+RAND_DIGITS+1;
	len=MAX_PID_LEN;
	int2reverse_hex( &c, &len, pid_nr );
	while (len) { *c='z'; len--; c++; }
	*c='-';

	callid_seq=random() % TABLE_ENTRIES;

}

void generate_callid() {
	char *c;
	int len;

	/* HACK: not long enough */
	callid_seq = (callid_seq+1) % TABLE_ENTRIES;
	c=call_id+RAND_DIGITS+1+MAX_PID_LEN+1;
	len=MAX_SEQ_LEN;
	int2reverse_hex( &c, &len, callid_seq );
	while (len) { *c='z'; len--; c++; }
}



int t_uac( str *msg_type, str *dst, 
	str *headers, str *body, transaction_cb completion_cb,
	void *cbp, struct dialog *dlg)
{

	struct cell *new_cell;
	struct proxy_l *proxy;
	int branch;
	int ret;
	int req_len;
	char *buf;
	union sockaddr_union to;
	struct socket_info* send_sock;
	struct retr_buf *request;

	/* be optimist -- assume success for return value */
	ret=1;

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
	generate_callid();

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
	LOCK_HASH(new_cell->hash_index);
	insert_into_hash_table_unsafe( hash_table , new_cell );
	UNLOCK_HASH(new_cell->hash_index);

	request=&new_cell->uac[branch].request;
	request->to=to;
	request->send_sock=send_sock;

	buf=build_uac_request(  *msg_type, *dst, *headers, *body, branch,
		new_cell /* t carries hash_index, label, md5, uac[].send_sock and
		     other pieces of information needed to print a message*/
		, &req_len );
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
		ser_error=ret=E_SEND;
		goto error01;
	}
	start_retr( request );

	/* success */
	goto done;

error01: 
	/* this is the safest way (though not the cheapest) to
	   make a transaction disappear; we may appreciate the
	   safety later when we add more complexity
	*/
	cleanup_uac_timers(new_cell);
	put_on_wait(new_cell);
error00:
	free_proxy( proxy );
	free( proxy );
done: 
	/* if we did not install cbp, release it now */
	if (cbp) shm_free(cbp);
	return ser_error=ret;
}

static void fifo_callback( struct cell *t, struct sip_msg *msg,
	int code, void *param)
{

	char *filename;
	int file;
	int r;
	str text;

	DBG("DEBUG: fifo UAC completed with status %d\n", code);
	if (t->cbp) {
		filename=(char *)(t->cbp);
		file=open(filename, O_WRONLY);
		if (file<0) {
			LOG(L_ERR, "ERROR: fifo_callback: can't open file %s: %s\n",
				filename, strerror(errno));
			return;
		}
		get_reply_status(&text,msg,code);
		if (text.s==0) {
			LOG(L_ERR, "ERROR: fifo_callback: get_reply_status failed\n");
			return;
		}
		r=write(file, text.s , text.len );
		close(file);
		pkg_free(text.s);
		if (r<0) {
			LOG(L_ERR, "ERROR: fifo_callback: write error: %s\n",
				strerror(errno));
			return;	
		}
	} else {
		LOG(L_INFO, "INFO: fifo UAC completed with status %d\n", code);
	}
}	

int fifo_uac( FILE *stream, char *response_file ) 
{
	char method[MAX_METHOD];
	char header[MAX_HEADER];
	char body[MAX_BODY];
	char dst[MAX_DST];
	str sm, sh, sb, sd;
	char *shmem_file;
	int fn_len;

	sm.s=method; sh.s=header; sb.s=body; sd.s=dst;
	while(1) {
		if (!read_line(method, MAX_METHOD, stream,&sm.len)||sm.len==0) {
			/* line breaking must have failed -- consume the rest
			   and proceed to a new request
			*/
			LOG(L_ERR, "ERROR: fifo_uac: method expected\n");
			return -1;
		}
		DBG("DEBUG: fifo_uac: method: %.*s\n", sm.len, method );
		if (!read_line(dst, MAX_DST, stream, &sd.len)||sd.len==0) {
			LOG(L_ERR, "ERROR: fifo_uac: destination expected\n");
			return -1;
		}
		DBG("DEBUG: fifo_uac:  dst: %.*s\n", sd.len, dst );
		/* now read header fields line by line */
		if (!read_line_set(header, MAX_HEADER, stream, &sh.len)) {
			LOG(L_ERR, "ERROR: fifo_uac: header fields expected\n");
			return -1;
		}
		DBG("DEBUG: fifo_uac: header: %.*s\n", sh.len, header );
		/* and eventually body */
		if (!read_line_set(body, MAX_BODY, stream, &sb.len)) {
			LOG(L_ERR, "ERROR: fifo_uac: body expected\n");
			return -1;
		}
		DBG("DEBUG: fifo_uac: body: %.*s\n", sb.len, body );
		DBG("DEBUG: fifo_uac: EoL -- proceeding to transaction creation\n");
		/* we got it all, initiate transaction now! */
		if (response_file) {
			fn_len=strlen(response_file)+1;
			shmem_file=shm_malloc(fn_len);
			if (shmem_file==0) {
				LOG(L_ERR, "ERROR: fifo_uac: no shmem\n");
				return -1;
			}
			memcpy(shmem_file, response_file, fn_len );
		} else {
			shmem_file=0;
		}
		/* HACK: there is yet a shortcoming -- if t_uac fails, callback
		   will not be triggered and no feedback will be printed
		   to shmem_file
		*/
		t_uac(&sm,&sd,&sh,&sb,fifo_callback,shmem_file,0 /* no dialog */);
		return 1;

	}
}

