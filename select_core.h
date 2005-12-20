#ifndef _SELECT_CORE_H
#define _SELECT_CORE_H

#include "str.h"
#include "parser/msg_parser.h"
#include "select.h"

enum {
	SEL_PARAM_TAG, 
	SEL_PARAM_Q, SEL_PARAM_EXPIRES, SEL_PARAM_METHOD, SEL_PARAM_RECEIVED, SEL_PARAM_INSTANCE, 
	SEL_PARAM_BRANCH, SEL_PARAM_RPORT, SEL_PARAM_I, SEL_PARAM_ALIAS
       };

SELECT_F(select_from)
SELECT_F(select_from_uri)
SELECT_F(select_from_tag)
SELECT_F(select_from_name)
SELECT_F(select_from_params)
SELECT_F(select_to)
SELECT_F(select_to_uri)
SELECT_F(select_to_tag)
SELECT_F(select_to_name)
SELECT_F(select_to_params)
SELECT_F(select_contact)
SELECT_F(select_contact_uri)
SELECT_F(select_contact_name)
SELECT_F(select_contact_params)
SELECT_F(select_contact_params_spec)
SELECT_F(select_via)
SELECT_F(select_via_name)
SELECT_F(select_via_version)
SELECT_F(select_via_transport)
SELECT_F(select_via_host)
SELECT_F(select_via_port)
SELECT_F(select_via_comment)
SELECT_F(select_via_params)
SELECT_F(select_via_params_spec)

SELECT_F(select_msgheader)
SELECT_F(select_anyheader)

SELECT_F(select_any_uri)
SELECT_F(select_uri_type)
SELECT_F(select_uri_user)
SELECT_F(select_uri_pwd)
SELECT_F(select_uri_host)
SELECT_F(select_uri_port)
SELECT_F(select_uri_params)

static select_row_t select_core[] = {
	{ NULL, PARAM_STR, STR_STATIC_INIT("from"), select_from, 0},
	{ NULL, PARAM_STR, STR_STATIC_INIT("f"), select_from, 0},
	{ select_from, PARAM_STR, STR_STATIC_INIT("uri"), select_from_uri, 0},
	{ select_from, PARAM_STR, STR_STATIC_INIT("tag"), select_from_tag, 0},
	{ select_from, PARAM_STR, STR_STATIC_INIT("name"), select_from_name, 0},
	{ select_from, PARAM_STR, STR_STATIC_INIT("params"), select_from_params, CONSUME_NEXT_STR},
	{ NULL, PARAM_STR, STR_STATIC_INIT("to"), select_to, 0},
	{ NULL, PARAM_STR, STR_STATIC_INIT("t"), select_to, 0},
	{ select_to, PARAM_STR, STR_STATIC_INIT("uri"), select_to_uri, 0},
	{ select_to, PARAM_STR, STR_STATIC_INIT("tag"), select_to_tag, 0},
	{ select_to, PARAM_STR, STR_STATIC_INIT("name"), select_to_name, 0},
	{ select_to, PARAM_STR, STR_STATIC_INIT("params"), select_to_params, CONSUME_NEXT_STR},
	{ NULL, PARAM_STR, STR_STATIC_INIT("contact"), select_contact, 0},
	{ NULL, PARAM_STR, STR_STATIC_INIT("m"), select_contact, 0},
	{ select_contact, PARAM_STR, STR_STATIC_INIT("uri"), select_contact_uri, 0},
	{ select_contact, PARAM_STR, STR_STATIC_INIT("name"), select_contact_name, 0}, 
	{ select_contact, PARAM_STR, STR_STATIC_INIT("q"), select_contact_params_spec, DIVERSION | SEL_PARAM_Q}, 
	{ select_contact, PARAM_STR, STR_STATIC_INIT("expires"), select_contact_params_spec, DIVERSION | SEL_PARAM_EXPIRES}, 
	{ select_contact, PARAM_STR, STR_STATIC_INIT("method"), select_contact_params_spec, DIVERSION | SEL_PARAM_METHOD}, 
	{ select_contact, PARAM_STR, STR_STATIC_INIT("received"), select_contact_params_spec, DIVERSION | SEL_PARAM_RECEIVED}, 
	{ select_contact, PARAM_STR, STR_STATIC_INIT("instance"), select_contact_params_spec, DIVERSION | SEL_PARAM_INSTANCE}, 	
	{ select_contact, PARAM_STR, STR_STATIC_INIT("params"), select_contact_params, CONSUME_NEXT_STR},
	{ NULL, PARAM_STR, STR_STATIC_INIT("via"), select_via, OPTIONAL | CONSUME_NEXT_INT},
	{ NULL, PARAM_STR, STR_STATIC_INIT("v"), select_via, OPTIONAL | CONSUME_NEXT_INT},
	{ select_via, PARAM_STR, STR_STATIC_INIT("name"), select_via_name, 0},
	{ select_via, PARAM_STR, STR_STATIC_INIT("version"), select_via_version, 0},
	{ select_via, PARAM_STR, STR_STATIC_INIT("transport"), select_via_transport, 0},
	{ select_via, PARAM_STR, STR_STATIC_INIT("host"), select_via_host, 0},
	{ select_via, PARAM_STR, STR_STATIC_INIT("port"), select_via_port, 0},
	{ select_via, PARAM_STR, STR_STATIC_INIT("comment"), select_via_comment, 0},
	{ select_via, PARAM_STR, STR_STATIC_INIT("branch"), select_via_params_spec, DIVERSION | SEL_PARAM_BRANCH},
	{ select_via, PARAM_STR, STR_STATIC_INIT("received"), select_via_params_spec, DIVERSION | SEL_PARAM_RECEIVED},
	{ select_via, PARAM_STR, STR_STATIC_INIT("rport"), select_via_params_spec, DIVERSION | SEL_PARAM_RPORT},
	{ select_via, PARAM_STR, STR_STATIC_INIT("i"), select_via_params_spec, DIVERSION | SEL_PARAM_I},
	{ select_via, PARAM_STR, STR_STATIC_INIT("alias"), select_via_params_spec, DIVERSION | SEL_PARAM_ALIAS},
	{ select_via, PARAM_STR, STR_STATIC_INIT("params"), select_via_params, CONSUME_NEXT_STR},
	
	{ select_from_uri, PARAM_INT, STR_NULL, select_any_uri, IS_ALIAS},
	{ select_to_uri, PARAM_INT, STR_NULL, select_any_uri, IS_ALIAS},
	{ select_contact_uri, PARAM_INT, STR_NULL, select_any_uri, IS_ALIAS},
	{ select_any_uri, PARAM_STR, STR_STATIC_INIT("type"), select_uri_type, 0},
	{ select_any_uri, PARAM_STR, STR_STATIC_INIT("user"), select_uri_user, 0},
	{ select_any_uri, PARAM_STR, STR_STATIC_INIT("pwd"), select_uri_pwd, 0},
	{ select_any_uri, PARAM_STR, STR_STATIC_INIT("host"), select_uri_host, 0},
	{ select_any_uri, PARAM_STR, STR_STATIC_INIT("port"), select_uri_port, 0},
	{ select_any_uri, PARAM_STR, STR_STATIC_INIT("params"), select_uri_params, 0},
	{ NULL, PARAM_STR, STR_STATIC_INIT("msg"), select_msgheader, PARAM_EXPECTED},
	{ select_msgheader, PARAM_STR, STR_NULL, select_anyheader, OPTIONAL | CONSUME_NEXT_INT},
	{ NULL, PARAM_INT, STR_NULL, NULL, 0}
};

static select_table_t select_core_table = {select_core, NULL};

#endif // _SELECT_CORE_H
