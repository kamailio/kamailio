/*
 * $Id$
 */

#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../fifo_server.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../ip_addr.h"
#include "config.h"
#include "ut.h"
#include "uac.h"
#include "dlg.h"
#include "callid.h"
#include "h_table.h"
#include "uac_fifo.h"


/*
 * Callback data structure
 */
struct cb_data {
	dlg_t* dialog;
	char filename[1];
};


struct str_list {
        str s;
        struct str_list *next;
};


#define skip_hf(_hf) (             \
    ((_hf)->type == HDR_FROM)   || \
    ((_hf)->type == HDR_TO)     || \
    ((_hf)->type == HDR_CALLID) || \
    ((_hf)->type == HDR_CSEQ)      \
)


/*
 * Report an error to syslog and FIFO output file
 */
static inline void fifo_uac_error(char *reply_fifo, int code, char *msg)
{
	LOG(L_ERR, "ERROR: fifo_uac: %s\n", msg ); 
	fifo_reply(reply_fifo, "%d fifo_uac: %s", code, msg);
}


/*
 * Get the Request URI from the FIFO stream and parse it
 */
static inline int fifo_get_ruri(FILE* stream, char* response_file, str* ruri, struct sip_uri* puri)
{
	static char ruri_buf[MAX_URI_SIZE];

	if (!read_line(ruri_buf, MAX_URI_SIZE, stream, &ruri->len) || !ruri->len) {
		fifo_uac_error(response_file, 400, "ruri expected");
		return -1;
	}
	
	if (parse_uri(ruri_buf, ruri->len, puri) < 0 ) {
		fifo_uac_error(response_file, 400, "ruri invalid\n");
		return -2;
	}
	ruri->s = ruri_buf;
	DBG("DEBUG: fifo_get_ruri: '%.*s'\n", ruri->len, ruri->s);
	return 0;
}


/*
 * Get and parse next hop URI
 */
static inline int fifo_get_nexthop(FILE* stream, char* response_file, str* nexthop, struct sip_uri* pnexthop)
{
	static char nexthop_buf[MAX_URI_SIZE];

	if (!read_line(nexthop_buf, MAX_URI_SIZE, stream, &nexthop->len) || !nexthop->len) {
		fifo_uac_error(response_file, 400, "next hop address expected\n");
		return -1;
	}

	if (nexthop->len == 1 && nexthop_buf[0] == '.' ) {
		DBG("DEBUG: fifo_get_nexthop: next hop empty\n");
		nexthop->s = 0; 
		nexthop->len = 0;
	} else if (parse_uri(nexthop_buf, nexthop->len, pnexthop) < 0 ) {
		fifo_uac_error(response_file, 400, "next hop uri invalid\n");
		return -2;
	} else {
		nexthop->s = nexthop_buf;
		DBG("DEBUG: fifo_get_nexthop: hop: '%.*s'\n", nexthop->len, nexthop->s);
	}

	return 0;
}


/*
 * Get method name from FIFO stream
 */
static inline int fifo_get_method(FILE* stream, char* response_file, str* method)
{
	static char method_buf[MAX_METHOD];

	if (!read_line(method_buf, MAX_METHOD, stream, &method->len) || !method->len) {
		     /* line breaking must have failed -- consume the rest
			and proceed to a new request
		     */
		fifo_uac_error(response_file, 400, "method expected");
		return -1;
	}
	method->s = method_buf;
	DBG("fifo_get_method: method: '%.*s'\n", method->len, method->s);
	return 0;
}


/*
 * Get message body from FIFO stream
 */
static inline int fifo_get_body(FILE* stream, char* response_file, str* body)
{
	static char body_buf[MAX_BODY];

	if (!read_body(body_buf, MAX_BODY, stream, &body->len)) {
		fifo_uac_error(response_file, 400, "body expected");
		return -1;
	}
	body->s = body_buf;
	DBG("fifo_get_body: body: %.*s\n", body->len,  body->s);
	return 0;
}


/*
 * Get message headers from FIFO stream
 */
static inline int fifo_get_headers(FILE* stream, char* response_file, str* headers)
{
	static char headers_buf[MAX_HEADER];

	     /* now read and parse header fields */
	if (!read_line_set(headers_buf, MAX_HEADER, stream, &headers->len) || !headers->len) {
		fifo_uac_error(response_file, 400, "HFs expected");
		return -1;
	}
	headers->s = headers_buf;
	DBG("fifo_get_headers: headers: %.*s\n", headers->len, headers->s);
	return 0;
}


/*
 * Create shm_copy of filename
 */
static inline int fifo_cbp(char** shm_file, char* response_file)
{
	int fn_len;

	if (response_file) {
		fn_len = strlen(response_file) + 1;
		*shm_file = shm_malloc(fn_len);
		if (!*shm_file) {
			fifo_uac_error(response_file, 500, "no shmem");
			return -1;
		}
		memcpy(*shm_file, response_file, fn_len);
	} else {
		shm_file = 0;
	}
	return 0;
}


static inline struct str_list *new_str(char *s, int len, struct str_list **last, int *total)
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
						/* inefficient - FIXME --andrei*/
						new=new_str(":", 1, &last, &total_len);
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


/* syntax:

	:t_uac:[file] EOL
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


/*
 * Make sure that the FIFO user created the message
 * correctly
 */
static inline int fifo_check_msg(struct sip_msg* msg, str* method, char* resp, str* body, 
				 int* fromtag, int *cseq_is, int* cseq, str* callid)
{
	struct to_body* parsed_from;
	struct cseq_body *parsed_cseq;
	int i;
	char c;

	if (body->len && !msg->content_type) {
		fifo_uac_error(resp, 400, "Content-Type missing");
		return -1;
	}

	if (body->len && msg->content_length) {
		fifo_uac_error(resp, 400, "Content-Length disallowed");
		return -2;
	}

	if (!msg->to) {
		fifo_uac_error(resp, 400, "To missing");
		return -3;
	}

	if (!msg->from) {
		fifo_uac_error(resp, 400, "From missing");
		return -4;
	}

	     /* we also need to know if there is from-tag and add it otherwise */
	if (parse_from_header(msg) < 0) {
		fifo_uac_error(resp, 400, "Error in From");
		return -5;
	}

	parsed_from = (struct to_body*)msg->from->parsed;
	*fromtag = parsed_from->tag_value.s && parsed_from->tag_value.len;

	*cseq = 0;
	if (msg->cseq && (parsed_cseq = get_cseq(msg))) {
		*cseq_is = 1;
		for (i = 0; i < parsed_cseq->number.len; i++) {
			c = parsed_cseq->number.s[i];
			if (c >= '0' && c <= '9' ) *cseq = (*cseq) * 10 + c - '0';
			else {
			        DBG("found non-numerical in CSeq: <%i>='%c'\n",(unsigned int)c,c);
				fifo_uac_error(resp, 400, "non-nummerical CSeq");
				return -6;
			}
		}
		
		if (parsed_cseq->method.len != method->len 
		    || memcmp(parsed_cseq->method.s, method->s, method->len) !=0 ) {
			fifo_uac_error(resp, 400, "CSeq method mismatch");
			return -7;
		}
	} else {
		*cseq_is = 0;
	}

	if (msg->callid) {
		callid->s = msg->callid->body.s;
		callid->len = msg->callid->body.len;
	} else {
		callid->s = 0;
		callid->len = 0;
	}

	return 0;
}


#define FIFO_ROUTE_PREFIX "Route: "
#define FIFO_ROUTE_SEPARATOR ", "

static inline void print_routes(FILE* out, dlg_t* _d)
{
	rr_t* ptr;

	ptr = _d->hooks.first_route;

	if (ptr) {
		fprintf(out, FIFO_ROUTE_PREFIX);
	} else {
		fprintf(out, ".\n");
		return;
	}

	while(ptr) {
		fprintf(out, "%.*s", ptr->len, ptr->nameaddr.name.s);

		ptr = ptr->next;
		if (ptr) {
			fprintf(out, FIFO_ROUTE_SEPARATOR);
		}
	} 

	if (_d->hooks.last_route) {
		fprintf(out, FIFO_ROUTE_SEPARATOR "<");
		fprintf(out, "%.*s", _d->hooks.last_route->len, _d->hooks.last_route->s);
		fprintf(out, ">");
	}

	if (_d->hooks.first_route) {
		fprintf(out, CRLF);
	}
}


static inline int print_uris(FILE* out, struct sip_msg* reply)
{
	dlg_t* dlg;
	
	dlg = (dlg_t*)shm_malloc(sizeof(dlg_t));
	if (!dlg) {
		LOG(L_ERR, "print_routes(): No memory left\n");
		return -1;
	}

	memset(dlg, 0, sizeof(dlg_t));
	if (dlg_response_uac(dlg, reply) < 0) {
		LOG(L_ERR, "print_routes(): Error while creating dialog structure\n");
		return -2;
	}

	if (dlg->state != DLG_CONFIRMED) {
		fprintf(out, ".\n.\n.\n");
		free_dlg(dlg);
		return 0;
	}

	if (dlg->hooks.request_uri->s) {	
		fprintf(out, "%.*s\n", dlg->hooks.request_uri->len, dlg->hooks.request_uri->s);
	} else {
		fprintf(out, ".\n");
	}
	if (dlg->hooks.next_hop->s) {
		fprintf(out, "%.*s\n", dlg->hooks.next_hop->len, dlg->hooks.next_hop->s);
	} else {
		fprintf(out, ".\n");
	}
	print_routes(out, dlg);
	free_dlg(dlg);
	return 0;
}


static void fifo_callback(struct cell *t, struct sip_msg *reply,
			  int code, void *param)
{
	
	char *filename;
	FILE* f;
	str text;

	DBG("!!!!! ref_counter: %d\n", t->ref_count);

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
		fifo_reply(filename, "%.*s\n", text.len, text.s );
		pkg_free(text.s);
	} else {
		text.s=reply->first_line.u.reply.reason.s;
		text.len=reply->first_line.u.reply.reason.len;

		f = fopen(filename, "wt");
		if (!f) return;
		fprintf(f, "%d %.*s\n", reply->first_line.u.reply.statuscode, text.len, text.s);
		print_uris(f, reply);
		fprintf(f, "%s\n", reply->headers->name.s);
		fclose(f);
	}
	DBG("DEBUG: fifo_callback sucesssfuly completed\n");
}	


int fifo_uac(FILE *stream, char *response_file)
{
	str method, ruri, nexthop, headers, body, hfb, callid;
	struct sip_uri puri, pnexthop;
	struct sip_msg faked_msg;
	int ret, sip_error, err_ret;
	int fromtag, cseq_is, cseq;
	struct cb_data;
	char err_buf[MAX_REASON_LEN];
	char* shm_file;
	dlg_t dlg;

	if (fifo_get_method(stream, response_file, &method) < 0) return 1;
	if (fifo_get_ruri(stream, response_file, &ruri, &puri) < 0) return 1;
	if (fifo_get_nexthop(stream, response_file, &nexthop, &pnexthop) < 0) return 1;
	if (fifo_get_headers(stream, response_file, &headers) < 0) return 1;

	/* use SIP parser to look at what is in the FIFO request */
	memset(&faked_msg, 0, sizeof(struct sip_msg));
	faked_msg.len = headers.len; 
	faked_msg.buf = faked_msg.unparsed = headers.s;
	if (parse_headers(&faked_msg, HDR_EOH, 0) == -1 ) {
		DBG("DEBUG: fifo_uac: parse_headers failed\n");
		fifo_uac_error(response_file, 400, "HFs unparseable");
		goto error;
	}
	DBG("DEBUG: fifo_uac: parse_headers succeeded\n");

	if (fifo_get_body(stream, response_file, &body) < 0) goto error;
	
	     /* at this moment, we collected all the things we got, let's
	      * verify user has not forgotten something */
	if (fifo_check_msg(&faked_msg, &method, response_file, &body, &fromtag, 
			   &cseq_is, &cseq, &callid) < 0) goto error;

	hfb.s = get_hfblock(nexthop.len ? &nexthop : &ruri, 
			    faked_msg.headers, &hfb.len, PROTO_UDP);
	if (!hfb.s) {
		fifo_uac_error(response_file, 500, "no mem for hf block");
		goto error;
	}

	DBG("DEBUG: fifo_uac: EoL -- proceeding to transaction creation\n");

	memset(&dlg, 0, sizeof(dlg_t));
	     /* Fill in Call-ID, use given Call-ID if
	      * present and generate it if not present
	      */
	if (callid.s && callid.len) dlg.id.call_id = callid;
	else generate_callid(&dlg.id.call_id);
	
	     /* We will not fill in dlg->id.rem_tag because
	      * if present it will be printed within To HF
	      */
	
	     /* Generate fromtag if not present */
	if (!fromtag) {
		generate_fromtag(&dlg.id.loc_tag, &dlg.id.call_id);
	}

	     /* Fill in CSeq */
	if (cseq_is) dlg.loc_seq.value = cseq;
	else dlg.loc_seq.value = DEFAULT_CSEQ;
	dlg.loc_seq.is_set = 1;

	dlg.loc_uri = faked_msg.from->body;
	dlg.rem_uri = faked_msg.to->body;
	dlg.hooks.request_uri = &ruri;
	dlg.hooks.next_hop = (nexthop.len ? &nexthop : &ruri);

#ifdef XL_DEBUG
	print_dlg(stderr, &dlg);
#endif

	/* we got it all, initiate transaction now! */
	if (fifo_cbp(&shm_file, response_file) < 0) goto error01;

	ret = t_uac(&method, &hfb, &body, &dlg, fifo_callback, shm_file);

	if (ret <= 0) {
		err_ret = err2reason_phrase(ret, &sip_error, err_buf,
					    sizeof(err_buf), "FIFO/UAC") ;
		if (err_ret > 0 )
		{
			fifo_uac_error(response_file, sip_error, err_buf);
		} else {
			fifo_uac_error(response_file, 500, "FIFO/UAC error");
		}
	}
	
 error01:
	pkg_free(hfb.s);
	
 error:
	     /* free_sip_msg(&faked_msg); */
	if (faked_msg.headers) free_hdr_field_lst(faked_msg.headers);
	return 1;
}
