/*
 * $Id$
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
 * History:
 * ----------
 * 2002-03-28 t_uac_dlg protocolization completed
 */

#include "vm_fifo.h"

#include "../tm/config.h"
#include "../tm/tm_load.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"
#include "../../dprint.h"
#include "../../fifo_server.h"
#include "../../ut.h"

#include <string.h>
#include <assert.h>

#include <arpa/inet.h>

/*
  Syntax:

  ":vm_reply:[response file]\n
  code\n
  reason\n
  trans_id\n
  to_tag\n
  [new headers]\n
  \n
  [Body]\n
  .\n
  \n"
 */

int fifo_vm_reply( FILE* stream, char *response_file )
{
    int ret;

    struct sip_msg* msg;

    char code[16];
    char reason[128];
    char trans_id[128];
    char new_headers[MAX_HEADER];
    char body[MAX_BODY];
    char to_tag[128];

    str sc        /*  code */
	,sr       /*  reason */
	,sti      /*  trans_id */
	,snh      /*  new_headers */
	,sb       /*  body */
	,sttag;   /*  to-tag */

    load_tm_f        load_tm;
    struct tm_binds  tmb;
    unsigned int hash_index,label,icode;

    sc.s=code;
    sr.s=reason;
    sti.s=trans_id;
    snh.s=new_headers; sb.s=body;
    sttag.s=to_tag; sttag.len=0;


    /*  get the infos from FIFO server */

    DBG("DEBUG: fifo_t_reply: ############### begin ##############\n");

    if (!read_line(sc.s, 16, stream, &sc.len)||sc.len==0) {
	LOG(L_ERR, "ERROR: fifo_t_reply: code expected\n");
	fifo_reply(response_file, "400 fifo_t_reply: code expected");
	return 1;
    }

    icode = str2s(sc.s,sc.len,&ret);
    if(ret){
	LOG(L_ERR, "ERROR: fifo_t_reply: code(int) has wrong format\n");
	fifo_reply(response_file, "400 fifo_t_reply: code(int) has wrong format");
	return 1;
    }

    if(!read_line(sr.s, 128, stream, &sr.len)||sr.len==0){
	LOG(L_ERR, "ERROR: fifo_t_reply: reason expected\n");
	fifo_reply(response_file, "400 fifo_t_reply: reason expected");
	return 1;
    }
    sr.s[sr.len]='\0';

    if (!read_line(sti.s, 128, stream, &sti.len)||sti.len==0) {
	LOG(L_ERR, "ERROR: fifo_t_reply: trans_id expected\n");
	fifo_reply(response_file, "400 fifo_t_reply: trans_id expected");
	return 1;
    }
    sti.s[sti.len]='\0';
    DBG("DEBUG: fifo_t_reply: trans_id=%.*s\n",sti.len,sti.s);
    
    if(sscanf(sti.s,"%u:%u", &hash_index, &label) != 2){
	LOG(L_ERR, "ERROR: fifo_t_reply: invalid trans_id (%s)\n",sti.s);
	fifo_reply(response_file, "400 fifo_t_reply: invalid trans_id");
	return 1;
    }
    DBG("DEBUG: fifo_t_reply: hash_index=%u label=%u\n",hash_index,label);

    if( !read_line(sttag.s,64,stream,&sttag.len) || sttag.len==0 ){
	LOG(L_ERR, "ERROR: fifo_t_reply: to-tag expected\n");
	goto error01;
    }
    sttag.s[sttag.len]='\0';
    DBG("DEBUG: fifo_t_reply: to-tag: %.*s\n",sttag.len,sttag.s);

    /*  parse the new headers */
    if (!read_line_set(snh.s, MAX_HEADER, stream, &snh.len)/*||snh.len==0*/) {
	LOG(L_ERR, "ERROR: fifo_t_reply: while reading new headers\n");
	fifo_reply(response_file, "400 fifo_t_reply: while reading new headers");
	return 1;
    }
    trim_r(snh);
    snh.s[snh.len]='\0';
    DBG("DEBUG: fifo_t_reply: new headers: %.*s\n", snh.len, snh.s);

    /*  body can be empty ... */
    read_body(sb.s, MAX_BODY, stream, &sb.len);
    if (sb.len != 0) {
	DBG("DEBUG: fifo_t_reply: body: %.*s\n", sb.len, sb.s);
    }
    sb.s[sb.len]='\0';
    
    if (!(load_tm = (load_tm_f)find_export("load_tm",NO_SCRIPT))){
	LOG(L_ERR,"ERROR: fifo_t_reply: could not load 'load_tm'. module tm should be loaded !\n");
	goto error01;
    }

    if ((*load_tm)(&tmb)==-1) {
	LOG(L_ERR,"ERROR: fifo_t_reply: 'load_tm' failed\n");
	goto error01;
    }

    if( ((*tmb.t_lookup_ident)(&msg,hash_index,label)) != 1 ) {
	LOG(L_ERR,"ERROR: fifo_t_reply: \n");
	goto error01;
    }

    // Ugly fix to avoid crash at t_lookup:161 (tid_matching)
    if(!msg->via1->transport.s)
	msg->via1->transport.len = 0;

    ret = (*tmb.t_reply_with_body)(msg,icode,reason,body,new_headers,to_tag);
    DBG("DEBUG: fifo_t_reply: ################ end ##############\n");

    return ret;

 error01:
    return -1;
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


/* syntax:

	:t_uac_dlg:[file] EOL
	method EOL
	dst EOL
	[r-uri] EOL (if none, dst is taken)
	[to] EOL (if no 'to', dst is taken)
	[to_tag] EOL
	[from] EOL (if no 'from', server's default from is taken)
	[from_tag] EOL (if none, a new one will be created)
	[cseq] EOL
	[call_id] EOL
	[CR-LF separated HFs]* EOL
	EOL
	[body] EOL
	.EOL
	EOL
*/

int fifo_uac_dlg( FILE *stream, char *response_file ) 
{
	char method[MAX_METHOD];
	char dst[MAX_DST];
	char r_uri[MAX_DST];
	char to[MAX_FROM];
	char to_tag[MAX_FROM];
	char from[MAX_FROM];
	char from_tag[MAX_FROM];
	char cseq_buf[32]; //TODO: use a #define
	int cseq_i=0;
	char call_id[128]; //TODO: use a #define
	char header[MAX_HEADER];
	char body[MAX_BODY];
	str sm, sd, sr, st, stt, sf, sft, scseq, scid, sh, sb;
	char *shmem_file;
	int fn_len;
	int ret;
	int sip_error;
	char err_buf[MAX_REASON_LEN];
	int err_ret;
	load_tm_f        load_tm;
	struct tm_binds  tmb;

	sm.s=method; sd.s=dst; sr.s=r_uri; st.s=to; stt.s=to_tag; sf.s=from;
	sft.s=from_tag;	scseq.s=cseq_buf; scid.s=call_id; sh.s=header; sb.s=body;

	// method
	if (!read_line(method, MAX_METHOD, stream,&sm.len)||sm.len==0) {
		/* line breaking must have failed -- consume the rest
		   and proceed to a new request
		*/
		LOG(L_ERR, "ERROR: fifo_vm_uac: method expected\n");
		fifo_reply(response_file, 
			"400 fifo_vm_uac: method expected");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac: method: %.*s\n", sm.len, method );

	// dst
	if (!read_line(dst, MAX_DST, stream, &sd.len)||sd.len==0) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: destination expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: destination expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac:  dst: %.*s\n", sd.len, dst );

	// [r-uri]
	if (!read_line(sr.s, MAX_DST, stream, &sr.len)) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: r-uri expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: r-uri expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac:  r-uri: %.*s\n", sr.len, sr.s );

	// [to]
	if (!read_line(st.s, MAX_FROM, stream, &st.len)) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: to expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: to expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac:  to: %.*s\n", st.len, st.s );

	// [to_tag]
	if (!read_line(stt.s, MAX_FROM, stream, &stt.len)) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: to expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: to expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac:  to-tag: %.*s\n", stt.len, stt.s );

	// [from]
	if (!read_line(from, MAX_FROM, stream, &sf.len)) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: from expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: from expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac:  from: %.*s\n", sf.len, from);

	// [from-tag]
	if (!read_line(from_tag, MAX_FROM, stream, &sft.len)) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: from-tag expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: from-tag expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac:  from-tag: %.*s\n", sft.len, from_tag);

	// [cseq]
	if (!read_line(scseq.s, 32, stream, &scseq.len)) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: cseq expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: cseq expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac:  cseq: %.*s\n", scseq.len, scseq.s);

	// convert scseq to integer into cseq
	if(sscanf(scseq.s,"%i",&cseq_i) != 1){
		fifo_reply(response_file, 
			"400 fifo_vm_uac: cseq has bad format\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: cseq has bad format\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac:  cseq_i: %i\n", cseq_i);

	// [call id]
	if (!read_line(scid.s, 128, stream, &scid.len)) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: cseq expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: cseq expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac:  call id: %.*s\n", scid.len, scid.s);

	/* now read header fields line by line */
	if (!read_line_set(header, MAX_HEADER, stream, &sh.len)) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: HFs expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: header fields expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac: header: %.*s\n", sh.len, header );
	/* and eventually body */
	if (!read_body(body, MAX_BODY, stream, &sb.len)) {
		fifo_reply(response_file, 
			"400 fifo_vm_uac: body expected\n");
		LOG(L_ERR, "ERROR: fifo_vm_uac: body expected\n");
		return 1;
	}
	DBG("DEBUG: fifo_vm_uac: body: %.*s\n", sb.len, body );
	DBG("DEBUG: fifo_vm_uac: EoL -- proceeding to transaction creation\n");
	/* we got it all, initiate transaction now! */
	if (response_file) {
		fn_len=strlen(response_file)+1;
		shmem_file=shm_malloc(fn_len);
		if (shmem_file==0) {
			LOG(L_ERR, "ERROR: fifo_vm_uac: no shmem\n");
			fifo_reply(response_file, 
				"500 fifo_vm_uac: no memory for shmem_file\n");
			return 1;
		}
		memcpy(shmem_file, response_file, fn_len );
	} else {
		shmem_file=0;
	}


    if (!(load_tm = (load_tm_f)find_export("load_tm",NO_SCRIPT))){
	LOG(L_ERR,"ERROR: fifo_t_reply: could not load 'load_tm'. module tm should be loaded !\n");
	return 1;
    }

    if ((*load_tm)(&tmb)==-1) {
	LOG(L_ERR,"ERROR: fifo_t_reply: 'load_tm' failed\n");
	return 1;
    }

    /* HACK: there is yet a shortcoming -- if t_uac fails, callback
       will not be triggered and no feedback will be printed
       to shmem_file
    */
    ret=(*tmb.t_uac_dlg)(&sm,
			 sd.len==0 ? 0 : &sd,
			 PROTO_UDP,
			 sr.len==0 ? 0 : &sr,
			 st.len==0 ? 0 : &st,
			 sf.len==0 ? 0 : &sf,
			 /*stt.len==0 ? 0 :*/ &stt,
			 sft.len==0 ? 0 : &sft,
			 (int*)&cseq_i,
			 scid.len==0 ? 0 : &scid,
			 sh.len==0 ? 0 : &sh,
			 sb.len==0 ? 0 : &sb,
			 fifo_callback,shmem_file);
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






