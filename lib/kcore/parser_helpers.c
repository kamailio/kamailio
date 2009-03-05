#include "parser_helpers.h"
#include "errinfo.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../dprint.h"

#include <string.h>

struct sip_uri *parse_to_uri(struct sip_msg *msg)
{
	struct to_body *tb = NULL;
	
	if(msg==NULL || msg->to==NULL || msg->to->parsed==NULL)
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

