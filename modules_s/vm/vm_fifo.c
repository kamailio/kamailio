/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
#include "vm.h"

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

    struct cell *trans;

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

#ifdef OBSO
    struct tm_binds  tmb;
#endif
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
		return -1;
    }

    icode = str2s(sc.s,sc.len,&ret);
    if(ret){
		LOG(L_ERR, "ERROR: fifo_t_reply: code(int) has wrong format\n");
		fifo_reply(response_file, "400 fifo_t_reply: code(int) has wrong format");
		return -1;
    }

    if(!read_line(sr.s, 128, stream, &sr.len)||sr.len==0){
		LOG(L_ERR, "ERROR: fifo_t_reply: reason expected\n");
		fifo_reply(response_file, "400 fifo_t_reply: reason expected");
		return -1;
    }
    sr.s[sr.len]='\0';

    if (!read_line(sti.s, 128, stream, &sti.len)||sti.len==0) {
		LOG(L_ERR, "ERROR: fifo_t_reply: trans_id expected\n");
		fifo_reply(response_file, "400 fifo_t_reply: trans_id expected");
		return -1;
    }
    sti.s[sti.len]='\0';
    DBG("DEBUG: fifo_t_reply: trans_id=%.*s\n",sti.len,sti.s);
    
    if(sscanf(sti.s,"%u:%u", &hash_index, &label) != 2){
		LOG(L_ERR, "ERROR: fifo_t_reply: invalid trans_id (%s)\n",sti.s);
		fifo_reply(response_file, "400 fifo_t_reply: invalid trans_id");
		return -1;
    }
    DBG("DEBUG: fifo_t_reply: hash_index=%u label=%u\n",hash_index,label);

    if( !read_line(sttag.s,64,stream,&sttag.len) || sttag.len==0 ){
		LOG(L_ERR, "ERROR: fifo_t_reply: to-tag expected\n");
		fifo_reply(response_file, "400 fifo_t_reply: to-ta expected");
		return -1;
    }
    sttag.s[sttag.len]='\0';
    DBG("DEBUG: fifo_t_reply: to-tag: %.*s\n",sttag.len,sttag.s);

    /*  parse the new headers */
    if (!read_line_set(snh.s, MAX_HEADER, stream, &snh.len)) {
		LOG(L_ERR, "ERROR: fifo_t_reply: while reading new headers\n");
		fifo_reply(response_file, "400 fifo_t_reply: while reading new headers");
		return -1;
    }
    snh.s[snh.len]='\0';
    DBG("DEBUG: fifo_t_reply: new headers: %.*s\n", snh.len, snh.s);

    /*  body can be empty ... */
    read_body(sb.s, MAX_BODY, stream, &sb.len);
    if (sb.len != 0) {
		DBG("DEBUG: fifo_t_reply: body: %.*s\n", sb.len, sb.s);
    }
    sb.s[sb.len]='\0';
    
    if( ((*_tmb.t_lookup_ident)(&trans,hash_index,label)) < 0 ) {
		LOG(L_ERR,"ERROR: fifo_t_reply: lookup failed\n");
		fifo_reply(response_file, "481 fifo_t_reply: no such transaction");
		return -1;
    }

    /* it's refcounted now, t_reply_with body unrefs for me -- I can 
     * continue but may not use T anymore  */
    ret = (*_tmb.t_reply_with_body)(trans,icode,reason,body,new_headers,to_tag);

    if (ret<0) {
	LOG(L_ERR, "ERROR: fifo_t_reply: reply failed\n");
	fifo_reply(response_file, "500 fifo_t_reply: reply failed");
	return -1;
    }
    
    fifo_reply(response_file, "200 fifo_t_reply succeeded\n");
    DBG("DEBUG: fifo_t_reply: ################ end ##############\n");
    return 1;

}



