#include <sys/socket.h>

#include "msg_translator.h"
#include "mem.h"
#include "dprint.h"
#include "config.h"
#include <netdb.h>


#define MAX_VIA_LINE_SIZE      240
#define MAX_RECEIVED_SIZE  57




/* faster than inet_ntoa */
static inline char* q_inet_itoa(unsigned long ip)
{
	static char q_inet_itoa_buf[16]; /* 123.567.901.345\0 */
	unsigned char* p;
	unsigned char a,b,c;  /* abc.def.ghi.jkl */
	int offset;
	int r;
	p=(unsigned char*)&ip;

	offset=0;
	/* unrolled loops (faster)*/
	for(r=0;r<3;r++){
		a=p[r]/100;
		c=p[r]%10;
		b=p[r]%100/10;
		if (a){
			q_inet_itoa_buf[offset]=a+'0';
			q_inet_itoa_buf[offset+1]=b+'0';
			q_inet_itoa_buf[offset+2]=c+'0';
			q_inet_itoa_buf[offset+3]='.';
			offset+=4;
		}else if (b){
			q_inet_itoa_buf[offset]=b+'0';
			q_inet_itoa_buf[offset+1]=c+'0';
			q_inet_itoa_buf[offset+2]='.';
			offset+=3;
		}else{
			q_inet_itoa_buf[offset]=c+'0';
			q_inet_itoa_buf[offset+1]='.';
			offset+=2;
		}
	}
	/* last number */
	a=p[r]/100;
	c=p[r]%10;
	b=p[r]%100/10;
	if (a){
		q_inet_itoa_buf[offset]=a+'0';
		q_inet_itoa_buf[offset+1]=b+'0';
		q_inet_itoa_buf[offset+2]=c+'0';
		q_inet_itoa_buf[offset+3]=0;
	}else if (b){
		q_inet_itoa_buf[offset]=b+'0';
		q_inet_itoa_buf[offset+1]=c+'0';
		q_inet_itoa_buf[offset+2]=0;
	}else{
		q_inet_itoa_buf[offset]=c+'0';
		q_inet_itoa_buf[offset+1]=0;
	}

	return q_inet_itoa_buf;
}




/* checks if ip is in host(name) and ?host(ip)=name?
 * ip must be in network byte order!
 *  resolver = DO_DNS | DO_REV_DNS; if 0 no dns check is made
 * return 0 if equal */
int check_address(unsigned long ip, char *name, int resolver)
{
	struct hostent* he;
	int i;

	/* maybe we are lucky and name it's an ip */
	if (strcmp(name, q_inet_itoa( /* *(struct in_addr *)&*/ip ))==0)
		return 0;
	if (resolver&DO_DNS){
		DBG("check_address: doing dns lookup\n");
		/* try all names ips */
		he=gethostbyname(name);
		for(i=0;he && he->h_addr_list[i];i++){
			if (*(unsigned long*)he->h_addr_list[i]==ip)
				return 0;
		}
	}
	if (resolver&DO_REV_DNS){
		DBG("check_address: doing rev. dns lookup\n");
		print_ip(ip);
		/* try reverse dns */
		he=gethostbyaddr((char*)&ip, sizeof(ip), AF_INET);
		if (he && (strcmp(he->h_name, name)==0))
			return 0;
		for (i=0; he && he->h_aliases[i];i++){
			if (strcmp(he->h_aliases[i],name)==0)
				return 0;
		}
	}
	return -1;
}





char * build_buf_from_sip_request(struct sip_msg* msg, unsigned int *returned_len)
{
	unsigned int len, new_len, via_len, received_len, uri_len;
	char* line_buf;
	char* received_buf;
	char* tmp;
	int tmp_len;
	char* new_buf;
	char* orig;
	char* buf;
	unsigned int offset, s_offset, size;
	unsigned long source_ip;
	struct lump *t,*r;
	struct lump* anchor;

	orig=msg->orig;
	buf=msg->buf;
	len=msg->len;
	source_ip=msg->src_ip;
	received_len=0;
	new_buf=0;
	line_buf=0;
	received_buf=0;

	line_buf=pkg_malloc(sizeof(char)*MAX_VIA_LINE_SIZE);
	if (line_buf==0){
		LOG(L_ERR, "ERROR: forward_request: out of memory\n");
		goto error1;
	}
/*
	via_len=snprintf(line_buf, MAX_VIA_LINE_SIZE, "Via: SIP/2.0/UDP %s:%d\r\n",
						names[0], port_no);
*/
	via_len=MY_VIA_LEN+names_len[0]; /* space included in MY_VIA*/
	if ((via_len+port_no_str_len+CRLF_LEN)<MAX_VIA_LINE_SIZE){
		memcpy(line_buf, MY_VIA, MY_VIA_LEN);
		memcpy(line_buf+MY_VIA_LEN, names[0], names_len[0]);
		if (port_no!=SIP_PORT){
			memcpy(line_buf+via_len, port_no_str, port_no_str_len);
			via_len+=port_no_str_len;
		}
		memcpy(line_buf+via_len, CRLF, CRLF_LEN);
		via_len+=CRLF_LEN;
		line_buf[via_len]=0; /* null terminate the string*/
	}else{
		LOG(L_ERR, "forward_request: ERROR: via too long (%d)\n",
				via_len);
		goto error1;
	}



	/* check if received needs to be added */
	if (check_address(source_ip, msg->via1->host.s, received_dns)!=0){
		received_buf=pkg_malloc(sizeof(char)*MAX_RECEIVED_SIZE);
		if (received_buf==0){
			LOG(L_ERR, "ERROR: forward_request: out of memory\n");
			goto error1;
		}
		/*
		received_len=snprintf(received_buf, MAX_RECEIVED_SIZE,
								";received=%s",
								inet_ntoa(*(struct in_addr *)&source_ip));
		*/
		memcpy(received_buf, RECEIVED, RECEIVED_LEN);
		tmp=q_inet_itoa( /* *(struct in_addr *)& */source_ip);
		tmp_len=strlen(tmp);
		received_len=RECEIVED_LEN+tmp_len;
		memcpy(received_buf+RECEIVED_LEN, tmp, tmp_len);
		received_buf[received_len]=0; /*null terminate it */
	}

	/* add via header to the list */
	/* try to add it before msg. 1st via */
	/*add first via, as an anchor for second via*/
	anchor=anchor_lump(&(msg->add_rm), msg->via1->hdr.s-buf, 0, HDR_VIA);
	if (anchor==0) goto error;
	if (insert_new_lump_before(anchor, line_buf, via_len, HDR_VIA)==0)
		goto error;
	/* if received needs to be added, add anchor after host and add it */
	if (received_len){
		if (msg->via1->params.s){
				size= msg->via1->params.s-msg->via1->hdr.s-1; /*compensate
															  for ';' */
		}else{
				size= msg->via1->host.s-msg->via1->hdr.s+msg->via1->host.len;
				if (msg->via1->port!=0){
					size+=strlen(msg->via1->hdr.s+size+1)+1; /* +1 for ':'*/
				}
		}
		anchor=anchor_lump(&(msg->add_rm),msg->via1->hdr.s-buf+size,0,
				HDR_VIA);
		if (anchor==0) goto error;
		if (insert_new_lump_after(anchor, received_buf, received_len, HDR_VIA)
				==0 ) goto error;
	}


	/* compute new msg len and fix overlapping zones*/
	new_len=len;
	s_offset=0;
	for(t=msg->add_rm;t;t=t->next){
		for(r=t->before;r;r=r->before){
			switch(r->op){
				case LUMP_ADD:
					new_len+=r->len;
					break;
				default:
					/* only ADD allowed for before/after */
					LOG(L_CRIT, "BUG:forward_request: invalid op for"
								" data lump (%x)\n", r->op);
			}
		}
		switch(t->op){
			case LUMP_ADD:
				new_len+=t->len;
				break;
			case LUMP_DEL:
				/* fix overlapping deleted zones */
				if (t->u.offset < s_offset){
					/* change len */
					if (t->len>s_offset-t->u.offset)
							t->len-=s_offset-t->u.offset;
					else t->len=0;
					t->u.offset=s_offset;
				}
				s_offset=t->u.offset+t->len;
				new_len-=t->len;
				break;
			case LUMP_NOP:
				/* fix offset if overlapping on a deleted zone */
				if (t->u.offset < s_offset){
					t->u.offset=s_offset;
				}else
					s_offset=t->u.offset;
				/* do nothing */
				break;
			debug:
				LOG(L_CRIT,"BUG:forward_request: invalid"
							" op for data lump (%x)\n", r->op);
		}
		for (r=t->after;r;r=r->after){
			switch(r->op){
				case LUMP_ADD:
					new_len+=r->len;
					break;
				default:
					/* only ADD allowed for before/after */
					LOG(L_CRIT, "BUG:forward_request: invalid"
								" op for data lump (%x)\n", r->op);
			}
		}
	}


	if (msg->new_uri.s){
		uri_len=msg->new_uri.len;
		new_len=new_len-msg->first_line.u.request.uri.len+uri_len;
	}
	new_buf=(char*)malloc(new_len+1);
	if (new_buf==0){
		LOG(L_ERR, "ERROR: forward_request: out of memory\n");
		goto error;
	}

	offset=s_offset=0;
	if (msg->new_uri.s){
		/* copy message up to uri */
		size=msg->first_line.u.request.uri.s-buf;
		memcpy(new_buf, orig, size);
		offset+=size;
		s_offset+=size;
		/* add our uri */
		memcpy(new_buf+offset, msg->new_uri.s, uri_len);
		offset+=uri_len;
		s_offset+=msg->first_line.u.request.uri.len; /* skip original uri */
	}
/* copy msg adding/removing lumps */
	for (t=msg->add_rm;t;t=t->next){
		switch(t->op){
			case LUMP_ADD:
				/* just add it here! */
				/* process before  */
				for(r=t->before;r;r=r->before){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:forward_request: invalid op for"
									" data lump (%x)\n", r->op);

					}
				}
				/* copy "main" part */
				memcpy(new_buf+offset, t->u.value, t->len);
				offset+=t->len;
				/* process after */
				for(r=t->after;r;r=r->after){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:forward_request: invalid op for"
									" data lump (%x)\n", r->op);
					}
				}
				break;
			case LUMP_NOP:
			case LUMP_DEL:
				/* copy till offset */
				if (s_offset>t->u.offset){
					DBG("Warning: (%d) overlapped lumps offsets,"
						" ignoring(%x, %x)\n", t->op, s_offset,t->u.offset);
					/* this should've been fixed above (when computing len) */
					/* just ignore it*/
					break;
				}
				size=t->u.offset-s_offset;
				if (size){
					memcpy(new_buf+offset, orig+s_offset,size);
					offset+=size;
					s_offset+=size;
				}
				/* process before  */
				for(r=t->before;r;r=r->before){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:forward_request: invalid op for"
									" data lump (%x)\n", r->op);

					}
				}
				/* process main (del only) */
				if (t->op==LUMP_DEL){
					/* skip len bytes from orig msg */
					s_offset+=t->len;
				}
				/* process after */
				for(r=t->after;r;r=r->after){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:forward_request: invalid op for"
									" data lump (%x)\n", r->op);
					}
				}
				break;
			default:
					LOG(L_CRIT, "BUG: forward_request: unknown op (%x)\n",
							t->op);
		}
	}
	/* copy the rest of the message */
	memcpy(new_buf+offset, orig+s_offset, len-s_offset);
	new_buf[new_len]=0;

	*returned_len=new_len;
	return new_buf;

error1:
	if (line_buf) pkg_free(line_buf);
	if (received_buf) pkg_free(received_buf);
error:
	if (new_buf) free(new_buf);
	*returned_len=0;
	return 0;
}




char * build_buf_from_sip_response(struct sip_msg* msg, unsigned int *returned_len)
{
	unsigned int new_len, via_len,r;
	char* new_buf;
	unsigned offset, s_offset, size;
	struct hostent* he;
	char* orig;
	char* buf;
	unsigned int len;
#ifdef DNS_IP_HACK
	int err;
#endif


	orig=msg->orig;
	buf=msg->buf;
	len=msg->len;
	new_buf=0;
	/* we must remove the first via */
	via_len=msg->via1->bsize;
	size=msg->via1->hdr.s-buf;
	DBG("via len: %d, initial size: %d\n", via_len, size);
	if (msg->via1->next){
		/* add hdr size*/
		size+=msg->via1->hdr.len+1;
	    DBG(" adjusted via len: %d, initial size: %d\n",
				via_len, size);
	}else{
		/* add hdr size ("Via:")*/
		via_len+=msg->via1->hdr.len+1;
	}
	new_len=len-via_len;

	DBG(" old size: %d, new size: %d\n", len, new_len);
	new_buf=(char*)malloc(new_len+1);/* +1 is for debugging
											(\0 to print it )*/
	if (new_buf==0){
		LOG(L_ERR, "ERROR: forward_reply: out of memory\n");
		goto error;
	}
	new_buf[new_len]=0; /* debug: print the message */
	memcpy(new_buf, orig, size);
	offset=size;
	s_offset=size+via_len;
	memcpy(new_buf+offset,orig+s_offset, len-s_offset);
	 /* send it! */
	DBG(" copied size: orig:%d, new: %d, rest: %d\n",
			s_offset, offset,
			len-s_offset );

	*returned_len=new_len;
	return new_buf;
error:
	if (new_buf) free(new_buf);
	*returned_len=0;
	return 0;
}



