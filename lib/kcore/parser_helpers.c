#include "parser_helpers.h"
#include "errinfo.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../dprint.h"
#include "../../mem/mem.h"

#include <string.h>

struct sip_uri *parse_to_uri(struct sip_msg *msg)
{
	struct to_body *tb = NULL;
	
	if(msg==NULL)
		return NULL;

	if(parse_to_header(msg)<0)
	{
		LM_ERR("cannot parse TO header\n");
		return NULL;
	}

	if(msg->to==NULL || get_to(msg)==NULL)
		return NULL;

	tb = get_to(msg);
	
	if(tb->parsed_uri.user.s!=NULL || tb->parsed_uri.host.s!=NULL)
		return &tb->parsed_uri;

	if (parse_uri(tb->uri.s, tb->uri.len , &tb->parsed_uri)<0)
	{
		LM_ERR("failed to parse To uri\n");
		memset(&tb->parsed_uri, 0, sizeof(struct sip_uri));
		set_err_info(OSER_EC_PARSER, OSER_EL_MEDIUM, "error parsing To uri");
		set_err_reply(400, "bad To uri");
		return NULL;
	}

	return &tb->parsed_uri;
}


struct sip_uri *parse_from_uri(struct sip_msg *msg)
{
	struct to_body *tb = NULL;
        
	if(msg==NULL)
		return NULL;

	if(parse_from_header(msg)<0)
	{
		LM_ERR("cannot parse FROM header\n");
		return NULL;
	}
	
	if(msg->from==NULL || get_from(msg)==NULL)
		return NULL;

	tb = get_from(msg);
	
	if(tb->parsed_uri.user.s!=NULL || tb->parsed_uri.host.s!=NULL)
		return &tb->parsed_uri;
	
	if (parse_uri(tb->uri.s, tb->uri.len , &tb->parsed_uri)<0)
	{
		LM_ERR("failed to parse From uri\n");
		memset(&tb->parsed_uri, 0, sizeof(struct sip_uri));
		set_err_info(OSER_EC_PARSER, OSER_EL_MEDIUM, "error parsing From uri");
		set_err_reply(400, "bad From uri");
		return NULL;
	}
	return &tb->parsed_uri;
}


/*!
 * get first RR header and print comma separated bodies in oroute
 * - order = 0 normal; order = 1 reverse
 * - nb_recs - input=skip number of rr; output=number of printed rrs
 */
int print_rr_body(struct hdr_field *iroute, str *oroute, int order,
												unsigned int * nb_recs)
{
	rr_t *p;
	int n = 0, nr=0;
	int i = 0;
	int route_len;
#define MAX_RR_HDRS	64
	static str route[MAX_RR_HDRS];
	char *cp, *start;

	if(iroute==NULL)
		return 0;

	route_len= 0;
	memset(route, 0, MAX_RR_HDRS*sizeof(str));

	while (iroute!=NULL) 
	{
		if (parse_rr(iroute) < 0) 
		{
			LM_ERR("failed to parse RR\n");
			goto error;
		}

		p =(rr_t*)iroute->parsed;
		while (p)
		{
			route[n].s = p->nameaddr.name.s;
			route[n].len = p->len;
			LM_DBG("current rr is %.*s\n", route[n].len, route[n].s);

			n++;
			if(n==MAX_RR_HDRS)
			{
				LM_ERR("too many RR\n");
				goto error;
			}
			p = p->next;
		}

		iroute = next_sibling_hdr(iroute);
	}

	for(i=0;i<n;i++){
		if(!nb_recs || (nb_recs && 
		 ( (!order&& (i>=*nb_recs)) || (order && (i<=(n-*nb_recs)) )) ) )
		{
			route_len+= route[i].len;
			nr++;
		}
	
	}

	if(nb_recs)
		LM_DBG("skipping %i route records\n", *nb_recs);
	
	route_len += --nr; /* for commas */

	oroute->s=(char*)pkg_malloc(route_len);


	if(oroute->s==0)
	{
		LM_ERR("no more pkg mem\n");
		goto error;
	}
	cp = start = oroute->s;
	if(order==0)
	{
		i= (nb_recs == NULL) ? 0:*nb_recs;

		while (i<n)
		{
			memcpy(cp, route[i].s, route[i].len);
			cp += route[i].len;
			if (++i<n)
				*(cp++) = ',';
		}
	} else {
		
		i = (nb_recs == NULL) ? n-1 : (n-*nb_recs-1);
			
		while (i>=0)
		{
			memcpy(cp, route[i].s, route[i].len);
			cp += route[i].len;
			if (i-->0)
				*(cp++) = ',';
		}
	}
	oroute->len=cp - start;

	LM_DBG("out rr [%.*s]\n", oroute->len, oroute->s);
	LM_DBG("we have %i records\n", n);
	if(nb_recs != NULL)
		*nb_recs = (unsigned int)n; 

	return 0;

error:
	return -1;
}


/*!
 * Path must be available. Function returns the first uri 
 * from Path without any duplication.
 */
int get_path_dst_uri(str *_p, str *_dst)
{
	rr_t *route = 0;

	LM_DBG("path for branch: '%.*s'\n", _p->len, _p->s);
	if(parse_rr_body(_p->s, _p->len, &route) < 0) {	
		LM_ERR("failed to parse Path body\n");
		return -1;
	}

	if(!route) {
		LM_ERR("failed to parse Path body no head found\n");
		return -1;
	}
	*_dst = route->nameaddr.uri;

	free_rr(&route);
	
	return 0;
}
