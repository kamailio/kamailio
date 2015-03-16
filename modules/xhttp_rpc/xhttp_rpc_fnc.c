/*
 * Copyright (C) 2011 VoIP Embedded, Inc.
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "../../trim.h"
#include "../../ver.h"
#include "../../rpc_lookup.h"
#include "xhttp_rpc.h"

extern str xhttp_rpc_root;
extern int xhttp_rpc_mod_cmds_size;
extern xhttp_rpc_mod_cmds_t *xhttp_rpc_mod_cmds;

extern int ver_name_len;
extern int full_version_len;

#define XHTTP_RPC_COPY(p,str)	\
do{	\
	if ((int)((p)-buf)+(str).len>max_page_len) {	\
		goto error;	\
	}	\
	memcpy((p), (str).s, (str).len); (p) += (str).len;	\
}while(0)

#define XHTTP_RPC_COPY_2(p,str1,str2)	\
do{	\
	if ((int)((p)-buf)+(str1).len+(str2).len>max_page_len) {	\
		goto error;	\
	}	\
	memcpy((p), (str1).s, (str1).len); (p) += (str1).len;	\
	memcpy((p), (str2).s, (str2).len); (p) += (str2).len;	\
}while(0)

#define XHTTP_RPC_COPY_3(p,str1,str2,str3)	\
do{	\
	if ((int)((p)-buf)+(str1).len+(str2).len+(str3).len>max_page_len) {	\
		goto error;	\
	}	\
	memcpy((p), (str1).s, (str1).len); (p) += (str1).len;	\
	memcpy((p), (str2).s, (str2).len); (p) += (str2).len;	\
	memcpy((p), (str3).s, (str3).len); (p) += (str3).len;	\
}while(0)

#define XHTTP_RPC_COPY_4(p,str1,str2,str3,str4)	\
do{	\
	if ((int)((p)-buf)+(str1).len+(str2).len+(str3).len+(str4).len>max_page_len) {	\
		goto error;	\
	}	\
	memcpy((p), (str1).s, (str1).len); (p) += (str1).len;	\
	memcpy((p), (str2).s, (str2).len); (p) += (str2).len;	\
	memcpy((p), (str3).s, (str3).len); (p) += (str3).len;	\
	memcpy((p), (str4).s, (str4).len); (p) += (str4).len;	\
}while(0)

#define XHTTP_RPC_COPY_5(p,s1,s2,s3,s4,s5)	\
do{	\
	if ((int)((p)-buf)+(s1).len+(s2).len+(s3).len+(s4).len+(s5).len>max_page_len) {	\
		goto error;	\
	}	\
	memcpy((p), (s1).s, (s1).len); (p) += (s1).len;	\
	memcpy((p), (s2).s, (s2).len); (p) += (s2).len;	\
	memcpy((p), (s3).s, (s3).len); (p) += (s3).len;	\
	memcpy((p), (s4).s, (s4).len); (p) += (s4).len;	\
	memcpy((p), (s5).s, (s5).len); (p) += (s5).len;	\
}while(0)

#define XHTTP_RPC_COPY_6(p,s1,s2,s3,s4,s5,s6)	\
do{	\
	if ((int)((p)-buf)+(s1).len+(s2).len+(s3).len+(s4).len+(s5).len+(s6).len>max_page_len) {	\
		goto error;	\
	}	\
	memcpy((p), (s1).s, (s1).len); (p) += (s1).len;	\
	memcpy((p), (s2).s, (s2).len); (p) += (s2).len;	\
	memcpy((p), (s3).s, (s3).len); (p) += (s3).len;	\
	memcpy((p), (s4).s, (s4).len); (p) += (s4).len;	\
	memcpy((p), (s5).s, (s5).len); (p) += (s5).len;	\
	memcpy((p), (s6).s, (s6).len); (p) += (s6).len;	\
}while(0)

#define XHTTP_RPC_COPY_10(p,s1,s2,s3,s4,s5,s6,s7,s8,s9,s10)	\
do{	\
	if ((int)((p)-buf)+(s1).len+(s2).len+(s3).len+(s4).len+(s5).len+(s6).len+(s7).len+(s8).len+(s9).len+(s10).len>max_page_len) {	\
		goto error;	\
	}	\
	memcpy((p), (s1).s, (s1).len); (p) += (s1).len;	\
	memcpy((p), (s2).s, (s2).len); (p) += (s2).len;	\
	memcpy((p), (s3).s, (s3).len); (p) += (s3).len;	\
	memcpy((p), (s4).s, (s4).len); (p) += (s4).len;	\
	memcpy((p), (s5).s, (s5).len); (p) += (s5).len;	\
	memcpy((p), (s6).s, (s6).len); (p) += (s6).len;	\
	memcpy((p), (s7).s, (s7).len); (p) += (s7).len;	\
	memcpy((p), (s8).s, (s8).len); (p) += (s8).len;	\
	memcpy((p), (s9).s, (s9).len); (p) += (s9).len;	\
	memcpy((p), (s10).s, (s10).len); (p) += (s10).len;	\
}while(0)


static const str XHTTP_RPC_Response_Head_1 = str_init("<html><head><title>");
static const str XHTTP_RPC_Response_Head_2 = str_init(" RPC Management Interface</title>"\
        "<style type=\"text/css\">"\
                "body{margin:0;}body,p,div,td,th,tr,form,ol,ul,li,input,textarea,select,"\
                "a{font-family:\"lucida grande\",verdana,geneva,arial,helvetica,sans-serif;font-size:14px;}"\
                "a:hover{text-decoration:none;}a{text-decoration:underline;}"\
                ".foot{padding-top:40px;font-size:10px;color:#333333;}"\
                ".foot a{font-size:10px;color:#000000;}"
                "table.center{margin-left:auto;margin-right:auto;}\n"\
        "</style>"\
        "<meta http-equiv=\"Expires\" content=\"0\">"\
        "<meta http-equiv=\"Pragma\" content=\"no-cache\">");

static const str XHTTP_RPC_Response_Head_3 = str_init(\
"</head>"\
"<body alink=\"#000000\" bgcolor=\"#ffffff\" link=\"#000000\" text=\"#000000\" vlink=\"#000000\">");

static const str XHTTP_RPC_Response_Title_Table_1 = str_init(\
"<table cellspacing=\"0\" cellpadding=\"5\" width=\"100%%\" border=\"0\">"\
        "<tr bgcolor=\"#BBDDFF\">"\
        "<td colspan=2 valign=\"top\" align=\"left\" bgcolor=\"#EFF7FF\" width=\"100%%\">"\
        "<br/><h2 align=\"center\">");
static const str XHTTP_RPC_Response_Title_Table_2 = str_init(": RPC Interface</h2>"\
        "<p align=\"center\">");
static const str XHTTP_RPC_Response_Title_Table_4 = str_init("</p><br/></td></tr></table>\n<center>\n");

static const str XHTTP_RPC_Response_Menu_Table_1 = str_init("<table border=\"0\" cellpadding=\"3\" cellspacing=\"0\"><tbody><tr>\n");
static const str XHTTP_RPC_Response_Menu_Table_2 = str_init("<td><a href='");
static const str XHTTP_RPC_Response_Menu_Table_2b = str_init("<td><b><a href='");
static const str XHTTP_RPC_Response_Menu_Table_3 = str_init("'>");
static const str XHTTP_RPC_Response_Menu_Table_4 = str_init("</a><td>\n");
static const str XHTTP_RPC_Response_Menu_Table_4b = str_init("</a></b><td>\n");
static const str XHTTP_RPC_Response_Menu_Table_5 = str_init("</tr></tbody></table>\n");

static const str XHTTP_RPC_Response_Menu_Cmd_Table_1 = str_init("<table border=\"0\" cellpadding=\"3\" cellspacing=\"0\" width=\"90%\"><tbody>\n");
static const str XHTTP_RPC_Response_Menu_Cmd_tr_1 = str_init("<tr>\n");
static const str XHTTP_RPC_Response_Menu_Cmd_td_1a = str_init("   <td width=\"10%\"><a href='");
static const str XHTTP_RPC_Response_Menu_Cmd_td_3a = str_init("'>");
static const str XHTTP_RPC_Response_Menu_Cmd_td_4a = str_init("</a></td>\n");
static const str XHTTP_RPC_Response_Menu_Cmd_td_1b = str_init("   <td align=\"left\"><b>");
static const str XHTTP_RPC_Response_Menu_Cmd_td_1c = str_init("   <td valign=\"top\" align=\"left\" rowspan=\"");
static const str XHTTP_RPC_Response_Menu_Cmd_td_1d = str_init("   <td>");
static const str XHTTP_RPC_Response_Menu_Cmd_td_3c = str_init("\">");
static const str XHTTP_RPC_Response_Menu_Cmd_td_4b = str_init("</b></td>\n");
static const str XHTTP_RPC_Response_Menu_Cmd_td_4c = str_init("   </td>\n");
static const str XHTTP_RPC_Response_Menu_Cmd_td_4d = str_init("</td>\n");
static const str XHTTP_RPC_Response_Menu_Cmd_tr_2 = str_init("</tr>\n");
static const str XHTTP_RPC_Response_Menu_Cmd_Table_2 = str_init("</tbody></table>\n");

static const str XHTTP_RPC_NBSP = str_init("&nbsp;");
static const str XHTTP_RPC_SLASH = str_init("/");
static const str XHTTP_RPC_SEMICOLON = str_init(": ");

static const str XHTTP_RPC_NODE_INDENT = str_init("\t");
static const str XHTTP_RPC_NODE_SEPARATOR = str_init(":: ");
static const str XHTTP_RPC_ATTR_SEPARATOR = str_init(" ");
static const str XHTTP_RPC_ATTR_VAL_SEPARATOR = str_init("=");

const str XHTTP_RPC_BREAK = str_init("<br/>");
static const str XHTTP_RPC_CODE_1 = str_init("<pre>");
static const str XHTTP_RPC_CODE_2 = str_init("</pre>");

static const str XHTTP_RPC_Post_1 = str_init("\n"\
"               <form name=\"input\" method=\"get\">\n"\
"                       <input type=\"text\" name=\"arg\"/>\n"\
"                       <input type=\"submit\" value=\"Submit\"/>\n"\
"               </form>\n");

static const str XHTTP_RPC_Post_1a = str_init("\n"\
"               <form name=\"input\" method=\"get\">\n"\
"                       <textarea name=\"arg\" rows=\"2\" cols=\"60\"></textarea>\n"\
"                       <input type=\"submit\" value=\"Submit\"/>\n"\
"               </form>\n");

static const str XHTTP_RPC_Response_Foot = str_init(\
"\n</center>\n<div align=\"center\" class=\"foot\" style=\"margin:20px auto\">"\
        "<span style='margin-left:5px;'></span>"\
        "<a href=\"http://sip-router.org\">SIP Router web site</a> .:. "\
        "<a href=\"http://www.kamailio.org\">Kamailio web site</a><br/>"\
        "Copyright &copy; 2011-2013 <a href=\"http://www.voipembedded.com/\">VoIP Embedded</a>"\
                                                                ". All rights reserved."\
"</div></body></html>");

#define XHTTP_RPC_ROWSPAN 5
static const str XHTTP_RPC_CMD_ROWSPAN = str_init("5");



static const str XHTTP_RPC_ARG = str_init("?arg=");
str XHTTP_RPC_NULL_ARG = str_init("");

int xhttp_rpc_parse_url(str *http_url, int* mod, int* cmd, str *arg)
{
	int index = 0;
	int i;
	int mod_len, cmd_len;
	int url_len = http_url->len;
	char *url = http_url->s;

	if (url_len==0) {
		LM_ERR("No URL\n");
		return -1;
	}
	if (url[0] != '/') {
		LM_ERR("URL starting with [%c] instead of'/'\n", url[0]);
		return -1;
	}
	index++;
	if (url_len - index < xhttp_rpc_root.len) {
		LM_ERR("root path 2 short [%.*s]\n", url_len, url);
		return -1;
	}
	if (strncmp(xhttp_rpc_root.s, &url[index], xhttp_rpc_root.len) != 0) {
		LM_ERR("wrong root path [%.*s]\n", url_len, url);
		return -1;
	}
	if (xhttp_rpc_root.len) {
		index += xhttp_rpc_root.len;
		if (url_len - index <= 0)
			return 0;
		if (url[index] != '/') {
			LM_ERR("invalid root path [%s]\n", url);
			return -1;
		}
		index++;
	}
	if (index>=url_len)
		return 0;

	for(i=index;i<url_len && url[i]!='/';i++);
	mod_len = i - index;
	for(i=0; i<xhttp_rpc_mod_cmds_size &&
			!(xhttp_rpc_mod_cmds[i].mod.s[mod_len]=='.' &&
			strncmp(&url[index],
				xhttp_rpc_mod_cmds[i].mod.s,
				mod_len)==0);
			i++);
	if (i==xhttp_rpc_mod_cmds_size) {
		LM_ERR("Invalid mod [%.*s] in url [%s]\n",
			mod_len, &url[index], url);
			return -1;
	}
	*mod = i;

	index += mod_len;
	if (index>=url_len)
		return 0;

	/* skip over '/' */
	index++;

	/* Looking for "cmd" */
	if (index>=url_len)
		return 0;
	for(i=index;i<url_len && url[i]!='?';i++);
	cmd_len = i - index;
	for(i=0;i<xhttp_rpc_mod_cmds[*mod].size &&
		!(strncmp(&url[index],
			rpc_sarray[xhttp_rpc_mod_cmds[*mod].rpc_e_index+i]->name,
			cmd_len) == 0 &&
		cmd_len==
		strlen(rpc_sarray[xhttp_rpc_mod_cmds[*mod].rpc_e_index+i]->name));
		i++);
	if (i==xhttp_rpc_mod_cmds[*mod].size) {
		LM_ERR("Invalid cmd [%.*s] in url [%.*s]\n",
			cmd_len, &url[index], url_len, url);
		return -1;
	}
	*cmd = i;
	index += cmd_len;
	if (index>=url_len) return 0;
	i = url_len - index;
	if (i<XHTTP_RPC_ARG.len &&
		(0!=strncmp(&url[index], XHTTP_RPC_ARG.s, XHTTP_RPC_ARG.len))){
		LM_ERR("Invalid arg string [%.*s]\n", i, &url[index]);
		return -1;
	}
	index += XHTTP_RPC_ARG.len;
	arg->s = &url[index];
	arg->len = url_len - index;

	return 0;
}


void xhttp_rpc_get_next_arg(rpc_ctx_t* ctx, str *arg)
{
	int i;

	trim_leading(&ctx->arg2scan);

	if (ctx->arg2scan.len<=0) {
		*arg = XHTTP_RPC_NULL_ARG;
		return;
	}
	if (ctx->arg2scan.len==1 && ctx->arg2scan.s[0]=='\0') {
		*arg = XHTTP_RPC_NULL_ARG;
		return;
	}
	else {
		*arg = ctx->arg2scan;
		for(i=1; i<arg->len-1; i++) {
			if(arg->s[i]==' '||arg->s[i]=='\t'||
				arg->s[i]=='\r'||arg->s[i]=='\n')
				break;
		}
		arg->len = i;
		arg->s[i] = '\0';
		i++;
		ctx->arg2scan.s += i;
		ctx->arg2scan.len -= i;
	}
	return;
}

int xhttp_rpc_build_header(rpc_ctx_t *ctx)
{
	int i, j;
	char *p = ctx->reply.body.s;
	char *buf = ctx->reply.buf.s;
	str code;
	int max_page_len = ctx->reply.buf.len;
	int mod = ctx->mod;
	int cmd = ctx->cmd;

	str name;

	str exec_name = {(char*)ver_name, ver_name_len};
	str server_hdr = {(char*)full_version, full_version_len};


	XHTTP_RPC_COPY_10(p,XHTTP_RPC_Response_Head_1,
			exec_name,
			XHTTP_RPC_Response_Head_2,
			XHTTP_RPC_Response_Head_3,
			XHTTP_RPC_Response_Title_Table_1,
			exec_name,
			XHTTP_RPC_Response_Title_Table_2,
			server_hdr,
			XHTTP_RPC_Response_Title_Table_4,
			/* Building module menu */
			XHTTP_RPC_Response_Menu_Table_1);
	for(i=0;i<xhttp_rpc_mod_cmds_size;i++) {
		if(i!=mod) {
			XHTTP_RPC_COPY(p,XHTTP_RPC_Response_Menu_Table_2);
		} else {
			XHTTP_RPC_COPY(p,XHTTP_RPC_Response_Menu_Table_2b);
		}
		XHTTP_RPC_COPY(p,XHTTP_RPC_SLASH);
		if (xhttp_rpc_root.len) {
			XHTTP_RPC_COPY_2(p,xhttp_rpc_root,XHTTP_RPC_SLASH);
		}
		XHTTP_RPC_COPY_3(p,xhttp_rpc_mod_cmds[i].mod,
				XHTTP_RPC_Response_Menu_Table_3,
				xhttp_rpc_mod_cmds[i].mod);
		if(i!=mod) {
			XHTTP_RPC_COPY(p,XHTTP_RPC_Response_Menu_Table_4);
		} else {
			XHTTP_RPC_COPY(p,XHTTP_RPC_Response_Menu_Table_4b);
		}
	}
	XHTTP_RPC_COPY(p,XHTTP_RPC_Response_Menu_Table_5);

	if (ctx->arg_received) { /* Build an rpc reply */
		name.s =
		(char*)rpc_sarray[xhttp_rpc_mod_cmds[mod].rpc_e_index+cmd]->name;
		name.len = strlen(name.s);
		/* Print comand name */
		XHTTP_RPC_COPY_4(p,XHTTP_RPC_Response_Menu_Cmd_Table_1,
				XHTTP_RPC_Response_Menu_Cmd_tr_1,
				XHTTP_RPC_Response_Menu_Cmd_td_1a,
				XHTTP_RPC_SLASH);
		if (xhttp_rpc_root.len) {
			XHTTP_RPC_COPY_2(p,xhttp_rpc_root, XHTTP_RPC_SLASH);
		}
		XHTTP_RPC_COPY_6(p,xhttp_rpc_mod_cmds[mod].mod,
				XHTTP_RPC_SLASH,
				name,
				XHTTP_RPC_Response_Menu_Cmd_td_3a,
				name,
				XHTTP_RPC_Response_Menu_Cmd_td_4a);
		/* Print response code */
		XHTTP_RPC_COPY(p,XHTTP_RPC_Response_Menu_Cmd_td_1d);
		code.s = int2str((unsigned long)ctx->reply.code, &code.len);
		XHTTP_RPC_COPY_10(p,code,
				XHTTP_RPC_SEMICOLON,
				ctx->reply.reason,
				XHTTP_RPC_Response_Menu_Cmd_td_4d,
				XHTTP_RPC_Response_Menu_Cmd_tr_2,
				XHTTP_RPC_Response_Menu_Cmd_tr_1,
				XHTTP_RPC_Response_Menu_Cmd_td_1d,
				XHTTP_RPC_Response_Menu_Cmd_td_4d,
				XHTTP_RPC_Response_Menu_Cmd_td_1d,
				XHTTP_RPC_CODE_1);
	} else if (mod>=0) { /* Building command menu */
		if (ctx->reply.body.len==0 && ctx->reply.code!=200) {
			code.s = int2str((unsigned long)ctx->reply.code, &code.len);
			XHTTP_RPC_COPY_5(p,XHTTP_RPC_CODE_1,
					code,
					XHTTP_RPC_SEMICOLON,
					ctx->reply.reason,
					XHTTP_RPC_CODE_2);
		} else {
			name.s =
			(char*)rpc_sarray[xhttp_rpc_mod_cmds[mod].rpc_e_index]->name;
			name.len = strlen(name.s);
			/* Build the list of comands for the selected module */
			XHTTP_RPC_COPY_4(p,XHTTP_RPC_Response_Menu_Cmd_Table_1,
					XHTTP_RPC_Response_Menu_Cmd_tr_1,
					XHTTP_RPC_Response_Menu_Cmd_td_1a,
					XHTTP_RPC_SLASH);
			if (xhttp_rpc_root.len) {
				XHTTP_RPC_COPY_2(p,xhttp_rpc_root,XHTTP_RPC_SLASH);
			}
			XHTTP_RPC_COPY_6(p,xhttp_rpc_mod_cmds[mod].mod,
					XHTTP_RPC_SLASH,
					name,
					XHTTP_RPC_Response_Menu_Cmd_td_3a,
					name,
					XHTTP_RPC_Response_Menu_Cmd_td_4a);
			if (cmd>=0) {
				name.s =
			(char*)rpc_sarray[xhttp_rpc_mod_cmds[mod].rpc_e_index+cmd]->name;
				name.len = strlen(name.s);
				XHTTP_RPC_COPY_3(p,XHTTP_RPC_Response_Menu_Cmd_td_1b,
						name,
						XHTTP_RPC_Response_Menu_Cmd_td_4b);
			}
			XHTTP_RPC_COPY(p,XHTTP_RPC_Response_Menu_Cmd_tr_2);
			for(j=1;j<xhttp_rpc_mod_cmds[mod].size;j++) {
				name.s =
			(char*)rpc_sarray[xhttp_rpc_mod_cmds[mod].rpc_e_index+j]->name;
				name.len = strlen(name.s);
				XHTTP_RPC_COPY_3(p,XHTTP_RPC_Response_Menu_Cmd_tr_1,
						XHTTP_RPC_Response_Menu_Cmd_td_1a,
						XHTTP_RPC_SLASH);
				if (xhttp_rpc_root.len) {
					XHTTP_RPC_COPY_2(p,xhttp_rpc_root, XHTTP_RPC_SLASH);
				}
				XHTTP_RPC_COPY_6(p,xhttp_rpc_mod_cmds[mod].mod,
						XHTTP_RPC_SLASH,
						name,
						XHTTP_RPC_Response_Menu_Cmd_td_3a,
						name,
						XHTTP_RPC_Response_Menu_Cmd_td_4a);
				if (cmd>=0){
					if (j==1) {
						XHTTP_RPC_COPY_5(p,
							XHTTP_RPC_Response_Menu_Cmd_td_1c,
							XHTTP_RPC_CMD_ROWSPAN,
							XHTTP_RPC_Response_Menu_Cmd_td_3c,
							XHTTP_RPC_Post_1,
							XHTTP_RPC_Response_Menu_Cmd_td_4c);
					} else if (j>XHTTP_RPC_ROWSPAN) {
						XHTTP_RPC_COPY_3(p,
							XHTTP_RPC_Response_Menu_Cmd_td_1d,
							XHTTP_RPC_NBSP,
							XHTTP_RPC_Response_Menu_Cmd_td_4d);
					}
				}
				XHTTP_RPC_COPY(p,XHTTP_RPC_Response_Menu_Cmd_tr_2);
			}
			if (cmd>=0){
				if (j==1) {
					XHTTP_RPC_COPY_10(p,XHTTP_RPC_Response_Menu_Cmd_tr_1,
							XHTTP_RPC_Response_Menu_Cmd_td_1d,
							XHTTP_RPC_NBSP,
							XHTTP_RPC_Response_Menu_Cmd_td_4d,
							XHTTP_RPC_Response_Menu_Cmd_td_1c,
							XHTTP_RPC_CMD_ROWSPAN,
							XHTTP_RPC_Response_Menu_Cmd_td_3c,
							XHTTP_RPC_Post_1,
							XHTTP_RPC_Response_Menu_Cmd_td_4c,
							XHTTP_RPC_Response_Menu_Cmd_tr_2);
					j++;
				}
				for(;j<=XHTTP_RPC_ROWSPAN;j++) {
					XHTTP_RPC_COPY_5(p,XHTTP_RPC_Response_Menu_Cmd_tr_1,
							XHTTP_RPC_Response_Menu_Cmd_td_1d,
							XHTTP_RPC_NBSP,
							XHTTP_RPC_Response_Menu_Cmd_td_4d,
							XHTTP_RPC_Response_Menu_Cmd_tr_2);
				}
			}
		}
		XHTTP_RPC_COPY_2(p,XHTTP_RPC_Response_Menu_Cmd_Table_2,
				XHTTP_RPC_Response_Foot);
	} else {
		if (ctx->reply.body.len==0 && ctx->reply.code!=200) {
			code.s = int2str((unsigned long)ctx->reply.code, &code.len);
			XHTTP_RPC_COPY_5(p,XHTTP_RPC_CODE_1,
					code,
					XHTTP_RPC_SEMICOLON,
					ctx->reply.reason,
					XHTTP_RPC_CODE_2);
		}
		XHTTP_RPC_COPY(p,XHTTP_RPC_Response_Foot);
	}

	ctx->reply.body.len = p - ctx->reply.body.s;
	return 0;


error:
	LM_ERR("buffer 2 small\n");
	ctx->reply.body.len = p - ctx->reply.body.s;
	return -1;
}


int xhttp_rpc_build_content(rpc_ctx_t *ctx, str *val, str *id)
{
	char *p;
	char *buf = ctx->reply.buf.s;
	int max_page_len = ctx->reply.buf.len;
	int i;

	if (ctx->reply.body.len==0)
		if (0!=xhttp_rpc_build_header(ctx))
			return -1;

	p = ctx->reply.body.s + ctx->reply.body.len;

	if (val && val->s && val->len) {
		if (id && id->s && id->len) {
			for(i=0;i<ctx->struc_depth;i++)
				XHTTP_RPC_COPY(p,XHTTP_RPC_NODE_INDENT);
			if ((int)(p-buf)+id->len>max_page_len) {
				goto error;
			}
			memcpy(p, id->s, id->len);
			p += id->len;
			XHTTP_RPC_COPY(p,XHTTP_RPC_SEMICOLON);
		}
		if ((int)(p-buf)+val->len>max_page_len) {
			goto error;
		}
		memcpy(p, val->s, val->len);
		p += val->len;
		XHTTP_RPC_COPY(p,XHTTP_RPC_BREAK);
	} else {
		if (id && id->s && id->len) {
			for(i=0;i<ctx->struc_depth;i++)
				XHTTP_RPC_COPY(p,XHTTP_RPC_NODE_INDENT);
			if ((int)(p-buf)+id->len>max_page_len) {
				goto error;
			}
			memcpy(p, id->s, id->len);
			p += id->len;
			XHTTP_RPC_COPY(p,XHTTP_RPC_SEMICOLON);
			XHTTP_RPC_COPY(p,XHTTP_RPC_BREAK);
		}
	}
	ctx->reply.body.len = p - ctx->reply.body.s;

	return 0;
error:
	LM_ERR("buffer 2 small\n");
	ctx->reply.body.len = p - ctx->reply.body.s;
	return -1;
}


int xhttp_rpc_insert_break(rpc_ctx_t *ctx)
{
	char *p = ctx->reply.body.s + ctx->reply.body.len;;
	char *buf = ctx->reply.buf.s;
	int max_page_len = ctx->reply.buf.len;

	XHTTP_RPC_COPY(p,XHTTP_RPC_BREAK);

	ctx->reply.body.len = p - ctx->reply.body.s;
	return 0;
error:
	LM_ERR("buffer 2 small\n");
	ctx->reply.body.len = p - ctx->reply.body.s;
	return -1;
}

int xhttp_rpc_build_page(rpc_ctx_t *ctx)
{
	char *p;
	char *buf = ctx->reply.buf.s;
	int max_page_len = ctx->reply.buf.len;

	if (ctx->reply.body.len==0)
		if (0!=xhttp_rpc_build_content(ctx, NULL, NULL))
			return -1;

	p = ctx->reply.body.s + ctx->reply.body.len;

	if (ctx->arg_received) {
		XHTTP_RPC_COPY_5(p,XHTTP_RPC_CODE_2,
				XHTTP_RPC_Response_Menu_Cmd_td_4d,
				XHTTP_RPC_Response_Menu_Cmd_tr_2,
				XHTTP_RPC_Response_Menu_Cmd_Table_2,
				XHTTP_RPC_Response_Foot);
		ctx->reply.body.len = p - ctx->reply.body.s;
	}

	return 0;
error:
	LM_ERR("buffer 2 small\n");
	ctx->reply.body.len = p - ctx->reply.body.s;
	return -1;
}
