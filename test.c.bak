/*
 * tst  
 */

#include <stdio.h>
#include <string.h>

#include "msg_parser.h"
#include "dprint.h"

#define BSIZE 1024
char buf[BSIZE+1];

void main()
{
	char* rest;
	char* tmp;
	char* first_via;
	char* second_via;
	struct msg_start fl;
	struct hdr_field hf;
	struct via_body vb1, vb2;

	int len;
	int offset;
	int r;

	while(!feof(stdin)){
		len=fread(buf,1,BSIZE,stdin);
		buf[len+1]=0;
		printf("read <%s>(%d)\n",buf,strlen(buf));
		fflush(stdin);
	
		/* eat crlf from the beginning */
		for (tmp=buf; (*tmp=='\n' || *tmp=='\r')&&
				tmp-buf < len ; tmp++);
		offset=tmp-buf;
		rest=parse_first_line(tmp, len-offset, &fl);
		offset+=rest-tmp;
		tmp=rest;
		switch(fl.type){
			case SIP_INVALID:
				printf("invalid message\n");
				break;
			case SIP_REQUEST:
				printf("SIP Request:\n");
				printf(" method:  <%s>\n",fl.u.request.method);
				printf(" uri:     <%s>\n",fl.u.request.uri);
				printf(" version: <%s>\n",fl.u.request.version);
				break;
			case SIP_REPLY:
				printf("SIP Reply  (status):\n");
				printf(" version: <%s>\n",fl.u.reply.version);
				printf(" status:  <%s>\n",fl.u.reply.status);
				printf(" reason:  <%s>\n",fl.u.reply.reason);
				break;
			default:
				printf("unknown type %d\n",fl.type);
		}
		
		/*find first Via: */
		hf.type=HDR_ERROR;
		first_via=0;
		second_via=0;
		do{
			rest=get_hdr_field(tmp, len-offset, &hf);
			offset+=rest-tmp;
			tmp=rest;
			switch (hf.type){
				case HDR_ERROR:
					DPrint("ERROR: bad header  field\n");
					goto  error;
				case HDR_EOH: 
					goto eoh;
				case HDR_VIA:
					if (first_via==0) first_via=hf.body;
					else if (second_via==0) second_via=hf.body;
					break;
			}
			printf("header field type %d, name=<%s>, body=<%s>\n",
				hf.type, hf.name, hf.body);

		
		}while(hf.type!=HDR_EOH && rest-buf < len);

	eoh:
		/* replace cr/lf with space in first via */
		for (tmp=first_via;(first_via) && (*tmp);tmp++)
			if ((*tmp=='\r')||(*tmp=='\n'))	*tmp=' ';

		printf("first via: <%s>\n", first_via);
		tmp=parse_via_body(first_via, strlen(first_via), &vb1);
		if (vb1.error!=VIA_PARSE_OK){
			DPrint("ERROR: parsing via body: %s\n", first_via);
			goto error;
		}
		/* compact via */
		if (vb1.next) second_via=vb1.next;
		if (second_via) {
			tmp=parse_via_body(second_via, strlen(second_via), &vb2);
			if (vb2.error!=VIA_PARSE_OK){
				DPrint("ERROR: parsing via body: %s\n", second_via);
				goto error;
			}
		}
		
		/* dump parsed data */
		printf(" first  via: <%s/%s/%s> <%s:%d>",
				vb1.name, vb1.version, vb1.transport, vb1.host, vb1.port);
		if (vb1.params) printf(";<%s>", vb1.params);
		if (vb1.comment) printf(" <%s>", vb1.comment);
		printf ("\n");
		if (second_via){
			printf(" second via: <%s/%s/%s> <%s:%d>",
					vb2.name, vb2.version, vb2.transport, vb2.host, vb2.port);
			if (vb2.params) printf(";<%s>", vb2.params);
			if (vb2.comment) printf(" <%s>", vb2.comment);
			printf ("\n");
		}

		

	error:
		/* find endof msg */
		printf("rest:(buffer=%x, rest=%x)\n%s\n.\n",buf,rest,rest);

		for (r=0; r<len+1;r++)
			printf("%02x ", buf[r]);
		printf("\n*rest=%02x\n",*rest);
		
	}
}
				
			
