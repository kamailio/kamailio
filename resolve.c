/* $Id$*/

/* #include <arpa/nameser.h> -- included from resolve.h*/
#include <netinet/in.h>
#include <resolv.h>
#include <string.h>

#include "resolve.h"
#include "dprint.h"
#include "mem/mem.h"



/* mallocs for local stuff */
#define local_malloc pkg_malloc
#define local_free   pkg_free



 /*  skips over a domain name in a dns message
 *  (it can be  a sequence of labels ending in \0, a pointer or
 *   a sequence of labels ending in a pointer -- see rfc1035
 *   returns pointer after the domain name or null on error*/
unsigned char* dns_skipname(unsigned char* p, unsigned char* end)
{
	while(p<end){
		/* check if \0 (root label length) */
		if (*p==0){
			p+=1;
			break;
		}
		/* check if we found a pointer */
		if (((*p)&0xc0)==0xc0){
			/* if pointer skip over it (2 bytes) & we found the end */
			p+=2;
			break;
		}
		/* normal label */
		p+=*p+1;
	}
	return (p>end)?0:p;
}



/* parses the srv record into a srv_rdata structure
 *   msg   - pointer to the dns message
 *   end   - pointer to the end of the message
 *   rdata - pointer  to the rdata part of the srv answer
 * returns 0 on error, or a dyn. alloc'ed srv_rdata structure */
/* SRV rdata format:
 *            111111
 *  0123456789012345
 * +----------------+
 * |     priority   |
 * |----------------|
 * |     weight     |
 * |----------------|
 * |   port number  |
 * |----------------|
 * |                |
 * ~      name      ~
 * |                |
 * +----------------+
 */
struct srv_rdata* dns_srv_parser( unsigned char* msg, unsigned char* end,
								  unsigned char* rdata)
{
	struct srv_rdata* srv;
	int len;
	
	srv=0;
	if ((rdata+6)>end) goto error;
	srv=(struct srv_rdata*)local_malloc(sizeof(struct srv_rdata));
	if (srv==0){
		LOG(L_ERR, "ERROR: dns_srv_parser: outof memory\n");
		goto error;
	}
	
	memcpy((void*)&srv->priority, rdata, 2);
	memcpy((void*)&srv->weight,   rdata+2, 2);
	memcpy((void*)&srv->port,     rdata+4, 2);
	rdata+=6;
	srv->priority=ntohs(srv->priority);
	srv->weight=ntohs(srv->weight);
	srv->port=ntohs(srv->port);
	if ((len=dn_expand(msg, end, rdata, srv->name, MAX_DNS_NAME))==-1)
		goto error;
	/* add terminatng 0 ? */
	return srv;
error:
	if (srv) local_free(srv);
	return 0;
}



/* parses an A record rdata into an a_rdata structure
 * returns 0 on error or a dyn. alloc'ed a_rdata struct
 */
struct a_rdata* dns_a_parser(unsigned char* rdata, unsigned char* end)
{
	struct a_rdata* a;
	
	if (rdata+4>end) goto error;
	a=(struct a_rdata*)local_malloc(sizeof(struct a_rdata));
	if (a==0){
		LOG(L_ERR, "ERROR: dns_a_parser: out of memory\n");
		goto error;
	}
	memcpy(a->ip, rdata, 4);
	return a;
error:
	return 0;
}



/* parses an AAAA (ipv6) record rdata into an aaaa_rdata structure
 * returns 0 on error or a dyn. alloc'ed aaaa_rdata struct */
struct aaaa_rdata* dns_aaaa_parser(unsigned char* rdata, unsigned char* end)
{
	struct aaaa_rdata* aaaa;
	
	if (rdata+16>end) goto error;
	aaaa=(struct aaaa_rdata*)local_malloc(sizeof(struct aaaa_rdata));
	if (aaaa==0){
		LOG(L_ERR, "ERROR: dns_aaaa_parser: out of memory\n");
		goto error;
	}
	memcpy(aaaa->ip6, rdata, 16);
	return aaaa;
error:
	return 0;
}



/* frees completely a struct rdata list */
void free_rdata_list(struct rdata* head)
{
	struct rdata* l;
	for(l=head; l; l=l->next){
		/* free the parsed rdata*/
		if (l->rdata) local_free(l->rdata);
		local_free(l);
	}
}



/* gets the DNS records for name:type
 * returns a dyn. alloc'ed struct rdata linked list with the parsed responses
 * or 0 on error
 * see rfc1035 for the query/response format */
struct rdata* get_record(char* name, int type)
{
	int size;
	int qno, answers_no;
	int r,i;
	int ans_len;
	static union dns_query buff;
	unsigned char* p;
	unsigned char* t;
	unsigned char* end;
	static unsigned char answer[ANS_SIZE];
	unsigned short rtype, class, rdlength;
	unsigned int ttl;
	struct rdata* head;
	struct rdata** crt;
	struct rdata* rd;
	struct srv_rdata* srv_rd;
	struct srv_rdata* crt_srv;
	
	
	
	head=rd=0;
	crt=&head;
	size=res_search(name, C_IN, type, buff.buff, sizeof(buff));
	if (size<0) goto error;
	else if (size > sizeof(buff)) size=sizeof(buff);
	
	p=buff.buff+DNS_HDR_SIZE;
	end=buff.buff+size;
	if (p>end) goto error_boundary;
	qno=ntohs((unsigned short)buff.hdr.qdcount);

	for (r=0; r<qno; r++){
		/* skip the name of the question */
		if ((p=dns_skipname(p, end))==0) goto error;
		p+=2+2; /* skip QCODE & QCLASS */
	#if 0
		for (;(p<end && (*p)); p++);
		p+=1+2+2; /* skip the ending  '\0, QCODE and QCLASS */
	#endif
		if (p>end) goto error;
	};
	answers_no=ntohs((unsigned short)buff.hdr.ancount);
	ans_len=ANS_SIZE;
	t=answer;
	for (r=0; (r<answers_no) && (p<end); r++){
		/*  ignore it the default domain name */
		if ((p=dns_skipname(p, end))==0) goto error;
		/*
		skip=dn_expand(buff.buff, end, p, t, ans_len);
		p+=skip;
		*/
		/* check if enough space is left fot type, class, ttl & size */
		if ((p+2+2+4+2)>end) goto error_boundary;
		/* get type */
		memcpy((void*) &rtype, (void*)p, 2);
		rtype=ntohs(rtype);
		p+=2;
		/* get  class */
		memcpy((void*) &class, (void*)p, 2);
		class=ntohs(class);
		p+=2;
		/* get ttl*/
		memcpy((void*) &ttl, (void*)p, 4);
		ttl=ntohl(ttl);
		p+=4;
		/* get size */
		memcpy((void*)&rdlength, (void*)p, 2);
		rdlength=ntohs(rdlength);
		p+=2;
		/* check for type */
		if (rtype!=type){
			LOG(L_WARN, "WARNING: get_record: wrong type in answer\n");
			p+=rdlength;
			continue;
		}
		/* expand the "type" record  (rdata)*/
		/* print it */
		/*
		printf("\ntype=%d class= %d, ttl= %d, rdlength= %d\n",
				rtype, class, ttl, rdlength);
		for (i=0;i<rdlength;i++){
			printf("%x ", *(p+i));
		}
		printf("\n");
		*/
		rd=(struct rdata*) local_malloc(sizeof(struct rdata));
		if (rd==0){
			LOG(L_ERR, "ERROR: get_record: out of memory\n");
			goto error;
		}
		rd->type=rtype;
		rd->class=class;
		rd->ttl=ttl;
		switch(type){
			case T_SRV:
				srv_rd= dns_srv_parser(buff.buff, end, p);
				rd->rdata=(void*)srv_rd;
				if (srv_rd==0) goto error_parse;
				
				/* insert sorted into the list
				 * crt reused */
				for (crt=&head; *crt; crt= &((*crt)->next)){
					crt_srv=(struct srv_rdata*)(*crt)->rdata;
					if ((srv_rd->priority <  crt_srv->priority) ||
					   ( (srv_rd->priority == crt_srv->priority) && 
							 (srv_rd->weight > crt_srv->weight) ) ){
						/* insert here */
						break;
					}
				}
				/* insert here */
				rd->next=*crt;
				*crt=rd;
				
				break;
			case T_A:
				rd->rdata=(void*) dns_a_parser(p,end);
				if (rd->rdata==0) goto error_parse;
				*crt=rd; /* crt points to the last "next" or the list head*/
				crt=&(rd->next);
				break;
			case T_AAAA:
				rd->rdata=(void*) dns_aaaa_parser(p,end);
				if (rd->rdata==0) goto error_parse;
				*crt=rd;
				crt=&(rd->next);
				break;
			default:
				LOG(L_ERR, "BUG: get_record: unknown type %d\n", type);
				rd->rdata=0;
				goto error;
		}
		
		p+=rdlength;
		
	}
	return head;
error_boundary:
		LOG(L_ERR, "ERROR: get_record: end of query buff reached\n");
		return 0;
error_parse:
		LOG(L_ERR, "ERROR: get_record: rdata parse error \n");
		if (rd) local_free(rd); /* rd->rdata=0 & rd is not linked yet into
								   the list */
error:
		LOG(L_ERR, "ERROR: get_record \n");
		if (head) free_rdata_list(head);
	return 0;
}

