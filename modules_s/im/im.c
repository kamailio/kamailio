/*
 * $Id$
 *
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"
#include "../../data_lump_rpl.h"
#include "im_funcs.h"
#include "im_load.h"


#define  IM_HEADER        "BEGIN_MESSAGE"
#define  IM_HEADER_LEN    (strlen(IM_HEADER))
#define  IM_FOOTER        "END_MESSAGE"
#define  IM_FOOTER_LEN    (strlen(IM_FOOTER))

#define append_str(_p,_s,_l) \
	{memcpy((_p),(_s),(_l));\
	(_p) += (_l);}


static int im_init(void);
static int im_dump_msg_to_fifo(struct sip_msg*, char*, char* );



/* parameters */
char *fifo_name = 0;



struct module_exports exports= {
	"im",
	(char*[]){
				"im_dump_msg_to_fifo",
				/* not applicable from script - only from other modules */
				"im_extract_body",
				"im_check_content_type",
				"im_get_body_len",
				"load_im"
			},
	(cmd_function[]){
					im_dump_msg_to_fifo,
					/* useless from script */
					(cmd_function)im_extract_body,
					(cmd_function)im_check_content_type,
					(cmd_function)im_get_body_len,
					(cmd_function)load_im
					},
	(int[]){
				0, /* im_dump_msg_to_fifo */
				/* useless from script */
				2, /* im_extract_body */
				1, /* im_check_content_type */
				1, /* im_get_body_len */
				1  /* im_load */
			},
	(fixup_function[]){
				0,
				0,
				0,
				0,
				0
		},
	5,

	(char*[]) {   /* Module parameter names */
		"dump_fifo_name"
	},
	(modparam_t[]) {   /* Module parameter types */
		STR_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&fifo_name
	},
	1,      /* Number of module paramers */

	im_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function)  0,   /* module exit function */
	0,
	(child_init_function) 0  /* per-child init function */
};




static int im_init(void)
{
	int fd;

	DBG("IM - initializing\n");

	if (!fifo_name) {
		LOG(L_WARN,"WARNING:im_init:no dump fifo name! desabling dumping!!\n");
		return 0;
	}

	/* open fifo */
	fd = open( fifo_name, O_TRUNC|O_WRONLY|O_NONBLOCK );
	if (fd==-1 && errno!=ENXIO ) {
		LOG(L_ERR,"ERROR:im_init: cannot open fifo %s : %s\n",
			fifo_name,strerror(errno));
		return -1;
	}

	if (fd>=0)
		close(fd);

	return 0;
}




inline int im_add_contact(struct sip_msg* msg , str* to_uri)
{
	struct lump_rpl *lump;
	char *buf, *p;
	int len;

	len = 9 /*"Contact: "*/ + to_uri->len + 2/*"<>"*/ + CRLF_LEN;

	buf = pkg_malloc( len );
	if(!buf) {
		LOG(L_ERR,"ERROR:im_add_contact: out of memory! \n");
		return -1;
	}

	p = buf;
	append_str( p, "Contact: " , 9);
	*(p++) = '<';
	append_str( p, to_uri->s, to_uri->len);
	*(p++) = '>';
	append_str( p, CRLF, CRLF_LEN);

	lump = build_lump_rpl( buf , len );
	if(!lump) {
		LOG(L_ERR,"ERROR:sms_add_contact: unable to build lump_rpl! \n");
		pkg_free( buf );
		return -1;
	}
	add_lump_rpl( msg , lump );

	pkg_free(buf);
	return 1;
}




static int im_dump_msg_to_fifo(struct sip_msg *msg, char *para1, char *para2)
{
	str    body;
	struct to_body  from_parsed;
	struct to_param *foo,*bar;
	char   *p, *buf=0;
	int    len;
	int    fd;

	if (!fifo_name) {
		LOG(L_ERR,"ERROR:im_dump_msg_to_file:dump fifo not configured!\n");
		goto error1;
	}

	if ( im_extract_body(msg,&body)==-1 )
	{
		LOG(L_ERR,"ERROR:im_dump_msg_to_file:cannot extract body from msg!\n");
		goto error1;
	}

	if (!msg->from) {
		LOG(L_ERR,"ERROR:im_dump_msg_to_file: no FROM header found!\n");
		goto error1;
	}

	if (!msg->to) {
		LOG(L_ERR,"ERROR:im_dump_msg_to_file: no TO header found!\n");
		goto error1;
	}

	/* parsing from header */
	memset(&from_parsed,0,sizeof(from_parsed));
	p = translate_pointer(msg->orig,msg->buf,msg->from->body.s);
	buf = (char*)pkg_malloc(msg->from->body.len+1);
	if (!buf) {
		LOG(L_ERR,"ERROR:im_dump_msg_to_file: no free pkg memory\n");
		goto error;
	}
	memcpy(buf,p,msg->from->body.len+1);
	parse_to(buf,buf+msg->from->body.len+1,&from_parsed);
	if (from_parsed.error!=PARSE_OK ) {
		LOG(L_ERR,"ERROR:im_dump_msg_to_file: cannot parse from header\n");
		goto error;
	}
	/* we are not intrested in from param-> le's free them now*/
	for(foo=from_parsed.param_lst ; foo ; foo=bar){
		bar = foo->next;
		pkg_free(foo);
	}

	/* adds contact header into reply */
	if (im_add_contact(msg,&(get_to(msg)->uri))==-1) {
		LOG(L_ERR,"ERROR:im_dump_msg_to_file:can't build contact for reply\n");
		goto error;
	}

	/*-------------BUILD AND FILL THE BUFFER --------------------*/
	/* computes the amount of memory needed */
	len = IM_HEADER_LEN + IM_FOOTER_LEN +2
		+ body.len + 1 /*body + \n */
		+ from_parsed.uri.len +1 /* from */
		+ get_to(msg)->uri.len + 1; /* to + \n */
	/* allocs a new sms_msg structure in pkg memory */
	if (buf) pkg_free(buf);
	buf = (char*)pkg_malloc(len);
	if (!buf) {
		LOG(L_ERR,"ERROR:im_dump_msg_to_file: cannot get pkg memory!\n");
		goto error;
	}
	p = buf;

	/* copy  header */
	append_str( p, IM_HEADER, IM_HEADER_LEN);
	*(p++) = '\n';
	/* copy from */
	append_str(p,from_parsed.uri.s,from_parsed.uri.len);
	*(p++) = '\n';
	/* copy to */
	append_str(p,get_to(msg)->uri.s,get_to(msg)->uri.len);
	*(p++) = '\n';
	/* copy body */
	append_str(p, body.s, body.len);
	*(p++) = '\n';
	append_str(p, IM_FOOTER, IM_FOOTER_LEN);
	*(p++) = '\n';
	DBG("DEBUG:im_dump_msg_to_file: <%d><%d>\n[%.*s]\n",len,p-buf,len,buf);

	/* opens for writting the fifo */
	fd = open(fifo_name, O_WRONLY|O_NONBLOCK);
	if (fd==-1) {
		LOG(L_ERR,"ERROR:im_dump_msg_to_file: cannot open fifo fro writting"
			"[%s] : %s\n",fifo_name, strerror(errno) );
		goto error;
	}
	if (write(fd, buf, len)!=len ) {
		LOG(L_ERR,"ERROR:im_dump_msg_to_file: error when writting to file"
			" : %s\n",strerror(errno) );
		close(fd);
		goto error;
	}
	close(fd);

	if (buf) pkg_free(buf);
	return 1;
error:
	if (buf) pkg_free(buf);
error1:
	return -1;
}

