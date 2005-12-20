#include "select.h"
#include "select_core.h"
#include "dprint.h"
#include "trim.h"
#include "parser/hf.h"
#include "parser/parse_from.h"
#include "parser/parse_to.h"
#include "parser/contact/parse_contact.h"
#include "parser/contact/contact.h"
#include "parser/parse_via.h"
#include "parser/parse_uri.h"

#define RETURN0_res(x) {*res=x;return 0;}
#define TRIM_RET0_res(x) {*res=x;trim(res);return 0;} 
#define TEST_RET_res_body(x) if (x){*res=x->body;return 0;}else return 1;
#define TEST_RET_res_value(x) if (x){*res=x->value;return 0;}else return 1;

int select_from(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_from_header(msg)<0)
		return -1;
	RETURN0_res(msg->from->body);
}

int select_from_uri(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_from_header(msg)<0)
		return -1;
	RETURN0_res(get_from(msg)->uri);
}

int select_from_tag(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_from_header(msg)<0)
		return -1;
	RETURN0_res(get_from(msg)->tag_value);
}

int select_from_name(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_from_header(msg)<0)
		return -1;
	RETURN0_res(get_from(msg)->display);
}

int select_from_params(str* res, select_t* s, struct sip_msg* msg)
{
	struct to_param* p;
	if (parse_from_header(msg)<0)
		return -1;
	
	p = get_from(msg)->param_lst;
	while (p) {
		if ((p->name.len==s->params[s->n-1].v.s.len)
		    && !strncasecmp(p->name.s, s->params[s->n-1].v.s.s,p->name.len)) {
			RETURN0_res(p->value);
		}
		p = p->next;
	}
	return 1;
}

int parse_to_header(struct sip_msg *msg)
{
        if ( !msg->to && ( parse_headers(msg,HDR_TO_F,0)==-1 || !msg->to)) {
                ERR("bad msg or missing TO header\n");
                return -1;
        }

        // HDR_TO_T is automatically parsed (get_hdr_field in parser/msg_parser.c)
        // so check only ptr validity
        if (msg->to->parsed)
                return 0;
	else
		return -1;
}

int select_to(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_to_header(msg)<0)
		return -1; 
	RETURN0_res(msg->to->body);
}

int select_to_uri(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_to_header(msg)<0)
		return -1;
	RETURN0_res(get_to(msg)->uri);
}

int select_to_tag(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_to_header(msg)<0)
		return -1;
	RETURN0_res(get_to(msg)->tag_value);
}

int select_to_name(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_to_header(msg)<0)
		return -1;
	RETURN0_res(get_to(msg)->display);
}

int select_to_params(str* res, select_t* s, struct sip_msg* msg)
{
	struct to_param* p;

	if (parse_to_header(msg)<0)
		return -1;
	
	p = get_to(msg)->param_lst;
	while (p) {
		if ((p->name.len==s->params[s->n-1].v.s.len)
		    && !strncasecmp(p->name.s, s->params[s->n-1].v.s.s,p->name.len)) {
			RETURN0_res(p->value);
		}
		p = p->next;
	}
	return 1;
}

int parse_contact_header( struct sip_msg *msg)
{
        if ( !msg->contact && ( parse_headers(msg,HDR_CONTACT_F,0)==-1 || !msg->contact)) {
                ERR("bad msg or missing CONTACT header\n");
                return -1;
        }

        if (msg->contact->parsed)
                return 0;

	return parse_contact(msg->contact);
}

#define get_contact(msg) ((contact_body_t*)(msg->contact->parsed))

int select_contact(str* res, select_t* s, struct sip_msg* msg)
{
	if (parse_contact_header(msg)<0)
		return -1;
	RETURN0_res(msg->contact->body);
}

int select_contact_uri(str* res, select_t* s, struct sip_msg* msg)
{
	contact_t* c;
	if (parse_contact_header(msg)<0)
		return -1;
	
	c = get_contact(msg)->contacts;
	if (!c)
		return 1;
	RETURN0_res(c->uri);
}

int select_contact_name(str* res, select_t* s, struct sip_msg* msg)
{
	contact_t* c;
	if (parse_contact_header(msg)<0)
		return -1;
	
	c = get_contact(msg)->contacts;
	if (!c)
		return 1;
	RETURN0_res(c->name);
}

int select_contact_params_spec(str* res, select_t* s, struct sip_msg* msg)
{
	contact_t* c;
	
	if (s->params[s->n-1].type != PARAM_DIV) {
		BUG("Last parameter should have type DIV (converted)\n");
		return -1;
	}
	
	if (parse_contact_header(msg)<0)
		return -1;
	
	c = get_contact(msg)->contacts;
	if (!c)
		return 1;
	
	switch (s->params[s->n-1].v.i) {
	case SEL_PARAM_Q:
		TEST_RET_res_body(c->q);
	case SEL_PARAM_EXPIRES:
		TEST_RET_res_body(c->expires);
	case SEL_PARAM_METHOD:
		TEST_RET_res_body(c->method);
	case SEL_PARAM_RECEIVED:
		TEST_RET_res_body(c->received);
	case SEL_PARAM_INSTANCE:
		TEST_RET_res_body(c->instance);
	default:
		BUG("Unexpected parameter value \"%d\"\n", s->params[s->n-1].v.i);
		return -1;
	}
	return -1;
}

int select_contact_params(str* res, select_t* s, struct sip_msg* msg)
{
	contact_t* c;
	param_t* p;
	if (parse_contact_header(msg)<0)
		return -1;
	
	c = get_contact(msg)->contacts;
	if (!c)
		return 1;
	p = c->params;
	while (p) {
		if ((p->name.len==s->params[s->n-1].v.s.len)
		    && !strncasecmp(p->name.s, s->params[s->n-1].v.s.s,p->name.len)) {
			RETURN0_res(p->body)
		}
		p = p->next;
	}
	return 1;
}

int parse_via_header( struct sip_msg *msg, int n, struct via_body** q)
{
	struct hdr_field *p;
	struct via_body *pp = NULL;
	int i;
	
	switch (n) {
	case 1:
	case 2:
		if (!msg->h_via1 && (parse_headers(msg,HDR_VIA_F,0)==-1 || !msg->h_via1)) {
                        DBG("bad msg or missing VIA1 header \n");
                        return -1;
                }
		pp = msg->h_via1->parsed;
		if (n==1) break;
		pp = pp->next;
		if (pp) break;
		
                if (!msg->h_via2 && (parse_headers(msg,HDR_VIA2_F,0)==-1 || !msg->h_via2)) {
                        DBG("bad msg or missing VIA2 header \n");
                        return -1;
                }
                pp = msg->h_via2->parsed;
                break;
	default:	
	        if (!msg->eoh && (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->eoh)) {
        	        ERR("bad msg while parsing to EOH \n");
	                return -1;
		}
		p = msg->h_via1;
		i = n;
		while (i && p) {
		        if (p->type == HDR_VIA_T) {
		        	i--;
		        	pp = p->parsed;
		        	while (i && (pp->next)) {
		        		i--;
		        		pp = pp->next;
		        	}
		        }
			p = p->next;
		}
		if (i > 0) {
			DBG("missing VIA[%d] header\n", n);
			return -1;
		}
	}
	if (pp) {
		*q = pp;
		return 0;
	} else
		return -1;
}

int select_via(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	if (((s->n == 1) || (s->params[1].type == PARAM_STR)) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	res->s=p->name.s;
	res->len=p->bsize;
	trim(res);
	return 0;
}

int select_via_name(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->name);
}

int select_via_version(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->version);
}

int select_via_transport(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->transport);
}

int select_via_host(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->host);
}

int select_via_port(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->port_str);
}

int select_via_comment(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	RETURN0_res(p->comment);
}

int select_via_params(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;
	struct via_param *q;

	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	
	for (q = p->param_lst;q;q=q->next) {
		if ((q->name.len==s->params[s->n-1].v.s.len)
		    && !strncasecmp(q->name.s, s->params[s->n-1].v.s.s,q->name.len)) {
			RETURN0_res(q->value);
		}
	}
	return 1;
}

int select_via_params_spec(str* res, select_t* s, struct sip_msg* msg)
{
	struct via_body *p = NULL;

	if (s->params[s->n-1].type != PARAM_DIV) {
		BUG("Last parameter should have type DIV (converted)\n");
		return -1;
	}
	
	// it's not neccessary to test if (s->n > 1)
	if ((s->params[1].type == PARAM_STR) && (parse_via_header(msg, 1, &p)<0)) return -1;
	else if (parse_via_header(msg, s->params[1].v.i, &p)<0) return -1;
	if (!p) return -1;
	
	switch (s->params[s->n-1].v.i) {
	case SEL_PARAM_BRANCH:
		TEST_RET_res_value(p->branch);
	case SEL_PARAM_RECEIVED:
		TEST_RET_res_value(p->received);
	case SEL_PARAM_RPORT:
		TEST_RET_res_value(p->rport);
	case SEL_PARAM_I:
		TEST_RET_res_value(p->i);
	case SEL_PARAM_ALIAS:
		TEST_RET_res_value(p->alias);
	default:
		BUG("Unexpected parameter value \"%d\"\n", s->params[s->n-1].v.i);
		return -1;
	}
	return -1;
}

//ABSTRACT_F(select_msgheader)
// Instead of ABSTRACT_F(select_msgheader)
// use function which uses select_core_table
// to avoid gcc warning about not used 
int select_msgheader(str* res, select_t* s, struct sip_msg* msg)
{
	if (select_core_table.next)
		return -1;
	else
		return -1;
}

int select_anyheader(str* res, select_t* s, struct sip_msg* msg)
{
	struct hdr_field *hf, *hf0;
	int hi;
	
	if(msg==NULL)
		return -1;

	hf0 = NULL;

	/* extract header index if present */
	if (s->n == 3) {
		if (s->params[2].type == PARAM_INT) {
			hi = s->params[2].v.i;
		} else {
			hi = -1;
		}
	} else {
		hi = 1;
	}

	/* we need to be sure we have parsed all headers */
	if (!msg->eoh && (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->eoh)) {
		ERR("bad msg while parsing to EOH \n");
		return -1;
	}
	for (hf=msg->headers; hf; hf=hf->next) {
		if(s->params[1].type==PARAM_DIV) {
			if (s->params[1].v.i!=hf->type)	continue;
		} else if(s->params[1].type==PARAM_STR) {
			if (s->params[1].v.s.len!=hf->name.len)	continue;
			if (strncasecmp(s->params[1].v.s.s, hf->name.s, hf->name.len)!=0) continue;
		}
		hf0 = hf;
		hi--;
		if (!hi) break;
	}
	if(hf0==NULL || hi>0)
		return 1;
	res->s = hf0->body.s;
	res->len = hf0->body.len;
	trim(res);
	return 0;
}

ABSTRACT_F(select_any_uri)

int select_uri_type(str* res, select_t* s, struct sip_msg* msg)
{
	if (!s->parent_f) {
		ERR("BUG: no parent fuction defined\n");
		return -1;
	}

	int ret;
	ret = s->parent_f(res, s, msg);
	if (ret != 0)
		return ret;

	struct sip_uri uri;
	trim(res);
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	switch (uri.type) {
	case SIPS_URI_T:
	case TELS_URI_T:
		res->len=4;
		break;
	case SIP_URI_T:
	case TEL_URI_T:
		res->len=3;
		break;
	case ERROR_URI_T:
		return -1;
	}
	return 0;
}

int select_uri_user(str* res, select_t* s, struct sip_msg* msg)
{
	if (!s->parent_f) {
		ERR("BUG: no parent fuction defined\n");
		return -1;
	}

	int ret;
	ret = s->parent_f(res, s, msg);
	if (ret != 0)
		return ret;

	struct sip_uri uri;
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	RETURN0_res(uri.user);
}

int select_uri_pwd(str* res, select_t* s, struct sip_msg* msg)
{
	if (!s->parent_f) {
		ERR("BUG: no parent fuction defined\n");
		return -1;
	}

	int ret;
	ret = s->parent_f(res, s, msg);
	if (ret != 0)
		return ret;

	struct sip_uri uri;
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	RETURN0_res(uri.passwd);
}

int select_uri_host(str* res, select_t* s, struct sip_msg* msg)
{
	if (!s->parent_f) {
		ERR("BUG: no parent fuction defined\n");
		return -1;
	}

	int ret;
	ret = s->parent_f(res, s, msg);
	if (ret != 0)
		return ret;

	struct sip_uri uri;
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	RETURN0_res(uri.host);
}

int select_uri_port(str* res, select_t* s, struct sip_msg* msg)
{
	if (!s->parent_f) {
		ERR("BUG: no parent fuction defined\n");
		return -1;
	}

	int ret;
	ret = s->parent_f(res, s, msg);
	if (ret != 0)
		return ret;

	struct sip_uri uri;
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;

	RETURN0_res(uri.port);
}

int select_uri_params(str* res, select_t* s, struct sip_msg* msg)
{
	if (!s->parent_f) {
		ERR("BUG: no parent fuction defined\n");
		return -1;
	}

	int ret;
	ret = s->parent_f(res, s, msg);
	if (ret != 0)
		return ret;

	struct sip_uri uri;
	if (parse_uri(res->s, res->len, &uri)<0)
		return -1;
		
	RETURN0_res(uri.params);
}

