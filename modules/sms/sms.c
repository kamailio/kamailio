/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../socket_info.h"
#include "../../cfg/cfg_struct.h"
#include "../../modules/tm/tm_load.h"
#include "sms_funcs.h"
#include "sms_report.h"
#include "libsms_modem.h"


MODULE_VERSION


static int sms_init(void);
static int sms_exit(void);
static int sms_child_init(int);
static int w_sms_send_msg(struct sip_msg*, char*, char* );
static int w_sms_send_msg_to_net(struct sip_msg*, char*, char*);
static int fixup_sms_send_msg_to_net(void** param, int param_no);



/* parameters */
char *networks_config = 0;
char *modems_config   = 0;
char *links_config    = 0;
char *default_net_str = 0;

/*global variables*/
int    default_net    = 0;
int    max_sms_parts  = MAX_SMS_PARTS;
str    domain = {0,0};
int    *queued_msgs    = 0;
int    use_contact     = 0;
int    sms_report_type = NO_REPORT;


static cmd_export_t cmds[]={
	{"sms_send_msg_to_net", w_sms_send_msg_to_net, 1,
	     fixup_sms_send_msg_to_net, REQUEST_ROUTE},
	{"sms_send_msg",        w_sms_send_msg,        0,
	     0,                         REQUEST_ROUTE},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"networks",        PARAM_STRING, &networks_config },
	{"modems",          PARAM_STRING, &modems_config   },
	{"links",           PARAM_STRING, &links_config    },
	{"default_net",     PARAM_STRING, &default_net_str },
	{"max_sms_parts",   INT_PARAM, &max_sms_parts   },
	{"domain",          PARAM_STR, &domain      },
	{"use_contact",     INT_PARAM, &use_contact     },
	{"sms_report_type", INT_PARAM, &sms_report_type },
	{0,0,0}
};


struct module_exports exports= {
	"sms",
	cmds,
	0,        /* RPC methods */
	params,

	sms_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) sms_exit,   /* module exit function */
	0,
	(child_init_function) sms_child_init  /* per-child init function */
};




static int fixup_sms_send_msg_to_net(void** param, int param_no)
{
	long net_nr,i;

	if (param_no==1) {
		for(net_nr=-1,i=0;i<nr_of_networks&&net_nr==-1;i++)
			if (!strcasecmp(networks[i].name,*param))
				net_nr = i;
		if (net_nr==-1) {
			LM_ERR("network \"%s\" not found in net list!\n",(char*)*param);
			return E_UNSPEC;
		} else {
			pkg_free(*param);
			*param=(void*)net_nr;
			return 0;
		}
	}
	return 0;
}





#define eat_spaces(_p) \
	while( *(_p)==' ' || *(_p)=='\t' ){\
	(_p)++;}




int set_modem_arg(struct modem *mdm, char *arg, char *arg_end)
{
	int err, foo;

	if (*(arg+1)!='=') {
		LM_ERR("invalid parameter syntax near [=]\n");
		goto error;
	}
	switch (*arg)
	{
		case 'd':  /* device */
			memcpy(mdm->device,arg+2,arg_end-arg-2);
			mdm->device[arg_end-arg-2] = 0;
			break;
		case 'p':  /* pin */
			memcpy(mdm->pin,arg+2,arg_end-arg-2);
			mdm->pin[arg_end-arg-2] = 0;
			break;
		case 'm':  /* mode */
			if (!strncasecmp(arg+2,"OLD",3)
			&& arg_end-arg-2==3) {
				mdm->mode = MODE_OLD;
			} else if (!strncasecmp(arg+2,"DIGICOM",7)
			&& arg_end-arg-2==7) {
				mdm->mode = MODE_DIGICOM;
			} else if (!strncasecmp(arg+2,"ASCII",5)
			&& arg_end-arg-2==5) {
				mdm->mode = MODE_ASCII;
			} else if (!strncasecmp(arg+2,"NEW",3)
			&& arg_end-arg-2==3) {
				mdm->mode = MODE_NEW;
			} else {
				LM_ERR("invalid value \"%.*s\" for param [m]\n",
					(int)(arg_end-arg-2),arg+2);
				goto error;
			}
			break;
		case 'c':  /* sms center number */
			memcpy(mdm->smsc,arg+2,arg_end-arg-2);
			mdm->smsc[arg_end-arg-2] = 0;
			break;
		case 'r':  /* retry time */
			foo=str2s(arg+2,arg_end-arg-2,&err);
			if (err) {
				LM_ERR("failed to convert [r] arg to integer!\n");
				goto error;
			}
			mdm->retry = foo;
			break;
		case 'l':  /* looping interval */
			foo=str2s(arg+2,arg_end-arg-2,&err);
			if (err) {
				LM_ERR("failed to convert [l] arg to integer!\n");
				goto error;
			}
			mdm->looping_interval = foo;
			break;
		case 'b':  /* baudrate */
			foo=str2s(arg+2,arg_end-arg-2,&err);
			if (err) {
				LM_ERR("failed to convert [b] arg to integer!\n");
				goto error;
			}
			switch (foo) {
				case   300: foo=B300; break;
				case  1200: foo=B1200; break;
				case  2400: foo=B2400; break;
				case  9600: foo=B9600; break;
				case 19200: foo=B19200; break;
				case 38400: foo=B38400; break;
				case 57600: foo=B57600; break;
				default:
					LM_ERR("unsupported value %d for [b] arg!\n",foo);
					goto error;
			}
			mdm->baudrate = foo;
			break;
		case 's':  /* scan */
			foo=str2s(arg+2,arg_end-arg-2,&err);
			if (err) {
				LM_WARN("cannot convert [s] arg to integer!, assume default mode s=%d (SCAN)\n", 
					SMS_BODY_SCAN);
				foo = SMS_BODY_SCAN;
			}
			switch (foo) {
				case   SMS_BODY_SCAN: 
				case   SMS_BODY_SCAN_NO: 
				case   SMS_BODY_SCAN_MIX: 
					break;
				default:
					LM_WARN("unsupported value s=%d for [s] arg!, assume default mode s=%d (SCAN)\n",
						  foo,SMS_BODY_SCAN);
					foo = SMS_BODY_SCAN;
			}
			mdm->scan = foo;
			break;
		case 't':  /* to */
			memcpy(mdm->to,arg+2,arg_end-arg-2);
			mdm->to[arg_end-arg-2] = 0;
			break;
		default:
			LM_ERR("unknown param name [%c]\n",*arg);
			goto error;
	}

	return 1;
error:
	return -1;
}




int set_network_arg(struct network *net, char *arg, char *arg_end)
{
	int err,foo;

	if (*(arg+1)!='=') {
		LM_ERR("invalid parameter syntax near [=]\n");
		goto error;
	}
	switch (*arg)
	{
		case 'm':  /* maximum sms per one call */
			foo=str2s(arg+2,arg_end-arg-2,&err);
			if (err) {
				LM_ERR("cannot convert [m] arg to integer!\n");
				goto error;
			}
			net->max_sms_per_call = foo;
			break;
		default:
			LM_ERR("unknown param name [%c]\n",*arg);
			goto error;
	}

	return 1;
error:
	return -1;
}




int parse_config_lines(void)
{
	char *p,*start;
	int  i, k, step;
	int  mdm_nr, net_nr;

	nr_of_networks = 0;
	nr_of_modems = 0;

	step = 1;
	/* parsing modems configuration string */
	if ( (p = modems_config)==0) {
		LM_ERR("param \"modems\" not found\n");
		goto error;
	}
	while (*p)
	{
		eat_spaces(p);
		/*get modem's name*/
		start = p;
		while (*p!=' ' && *p!='\t' && *p!='[' && *p!=0)
			p++;
		if ( p==start || *p==0 )
			goto parse_error;
		memcpy(modems[nr_of_modems].name, start, p-start);
		modems[nr_of_modems].name[p-start] = 0;
		modems[nr_of_modems].smsc[0] = 0;
		modems[nr_of_modems].device[0] = 0;
		modems[nr_of_modems].pin[0] = 0;
		modems[nr_of_modems].mode = MODE_NEW;
		modems[nr_of_modems].retry = 4;
		modems[nr_of_modems].looping_interval = 20;
		modems[nr_of_modems].baudrate = B9600;
		modems[nr_of_modems].scan = SMS_BODY_SCAN;
		modems[nr_of_modems].to[0] = 0;
		memset(modems[nr_of_modems].net_list,0XFF,
			sizeof(modems[nr_of_modems].net_list) );
		/*get modem parameters*/
		eat_spaces(p);
		if (*p!='[')
			goto parse_error;
		p++;
		while (*p!=']')
		{
			eat_spaces(p);
			start = p;
			while(*p!=' ' && *p!='\t' && *p!=']' && *p!=';' && *p!=0)
				p++;
			if ( p==start || *p==0 )
				goto parse_error;
			if (set_modem_arg( &(modems[nr_of_modems]), start, p)==-1)
				goto error;
			eat_spaces(p);
			if (*p==';') {
				p++;
				eat_spaces(p);
			}
		}
		if (*p!=']')
			goto parse_error;
		p++;
		/* end of element */
		if (modems[nr_of_modems].device[0]==0) {
			LM_ERR("modem %s has no device associated\n",
					modems[nr_of_modems].name);
			goto error;
		}
		if (modems[nr_of_modems].smsc[0]==0) {
			LM_WARN("modem %s has no sms center associated -> using"
				" the default one from modem\n",modems[nr_of_modems].name);
		}
		nr_of_modems++;
		eat_spaces(p);
		if (*p==';') {
			p++;
			eat_spaces(p);
		}
	}
	if (nr_of_modems==0)
	{
		LM_ERR("failed to parse config modems - no modem found!\n");
		goto error;
	}

	step++;
	/* parsing networks configuration string */
	if ( (p = networks_config)==0) {
		LM_ERR("param \"networks\" not found\n");
		goto error;
	}
	while (*p)
	{
		eat_spaces(p);
		/*get network name*/
		start = p;
		while (*p!=' ' && *p!='\t' && *p!='[' && *p!=0)
			p++;
		if ( p==start || *p==0 )
			goto parse_error;
		memcpy(networks[nr_of_networks].name, start, p-start);
		networks[nr_of_networks].name[p-start] = 0;
		networks[nr_of_networks].max_sms_per_call = 10;
		/*get network parameters*/
		eat_spaces(p);
		if (*p!='[')
			goto parse_error;
		p++;
		while (*p!=']')
		{
			eat_spaces(p);
			start = p;
			while(*p!=' ' && *p!='\t' && *p!=']' && *p!=';' && *p!=0)
				p++;
			if ( p==start || *p==0 )
				goto parse_error;
			if (set_network_arg( &(networks[nr_of_networks]), start, p)==-1)
				goto error;
			eat_spaces(p);
			if (*p==';') {
				p++;
				eat_spaces(p);
			}
		}
		if (*p!=']')
			goto parse_error;
		p++;
		/* end of element */
		nr_of_networks++;
		eat_spaces(p);
		if (*p==';')
			p++;
		eat_spaces(p);
	}
	if (nr_of_networks==0)
	{
		LM_ERR("no network found!\n");
		goto error;
	}

	step++;
	/* parsing links configuration string */
	if ( (p = links_config)==0) {
		LM_ERR("param \"links\" not found\n");
		goto error;
	}
	while (*p)
	{
		eat_spaces(p);
		/*get modem's device*/
		start = p;
		while (*p!=' ' && *p!='\t' && *p!='[' && *p!=0)
			p++;
		if ( p==start || *p==0 )
			goto parse_error;
		/*looks for modem index*/
		for(mdm_nr=-1,i=0;i<nr_of_modems && mdm_nr==-1;i++)
			if (!strncasecmp(modems[i].name,start,p-start)&&
			modems[i].name[p-start]==0)
				mdm_nr = i;
		if (mdm_nr==-1) {
			LM_ERR("unknown modem %.*s \n,",(int)(p-start), start);
			goto error;
		}
		/*get associated networks list*/
		eat_spaces(p);
		if (*p!='[')
			goto parse_error;
		p++;
		k=0;
		while (*p!=']')
		{
			eat_spaces(p);
			start = p;
			while(*p!=' ' && *p!='\t' && *p!=']' && *p!=';' && *p!=0)
				p++;
			if ( p==start || *p==0 )
				goto parse_error;
			/* lookup for the network -> get its index */
			for(net_nr=-1,i=0;i<nr_of_networks&&net_nr==-1;i++)
				if (!strncasecmp(networks[i].name,start,p-start)
				&& networks[i].name[p-start]==0)
					net_nr = i;
			if (net_nr==-1) {
				LM_ERR("associated net <%.*s> not found in net list\n",
					(int)(p-start), start);
				goto error;
			}
			LM_DBG("linking net \"%s\" to modem \"%s\" on pos %d.\n",
					networks[net_nr].name,modems[mdm_nr].name,k);
			modems[mdm_nr].net_list[k++]=net_nr;
			eat_spaces(p);
			if (*p==';') {
				p++;
				eat_spaces(p);
			}
		}
		if (*p!=']')
			goto parse_error;
		p++;
		/* end of element */
		eat_spaces(p);
		if (*p==';') {
			p++;
			eat_spaces(p);
		}
	}

	/* resolving default network name - if any*/
	if (default_net_str) {
		for(net_nr=-1,i=0;i<nr_of_networks&&net_nr==-1;i++)
			if (!strcasecmp(networks[i].name,default_net_str))
				net_nr = i;
		if (net_nr==-1) {
			LM_ERR("network \"%s\" not found in net list!\n",default_net_str);
			goto error;
		}
		default_net = net_nr;
	}

	return 0;
parse_error:
	LM_ERR("SMS %s config: parse error before  chr %d [%.*s]\n",
		(step==1)?"modems":(step==2?"networks":"links"),
		(int)(p - ((step==1)?modems_config:
				   (step==2?networks_config:links_config))),
		(*p==0)?4:1,(*p==0)?"NULL":p );
error:
	return -1;
}




int global_init(void)
{
	load_tm_f  load_tm;
	int        i, net_pipe[2], foo;
	char       *p;
	struct socket_info* si;

	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		LM_ERR("cannot import load_tm\n");
		goto error;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm( &tmb )==-1)
		goto error;

	/*fix domain*/
	if (!domain.s){
		si=get_first_socket();
		if (si==0){
			LM_CRIT("null listen socket list\n");
			goto error;
		}
		/*do I have to add port?*/
		i = (si->port_no_str.len && si->port_no!=5060);
		domain.len = si->name.len + i*(si->port_no_str.len+1);
		domain.s = (char*)pkg_malloc(domain.len);
		if (!domain.s) {
			LM_ERR("no free pkg memory!\n");
			goto error;
		}
		p = domain.s;
		memcpy(p,si->name.s,si->name.len);
		p += si->name.len;
		if (i) {
			*p=':'; p++;
			memcpy(p,si->port_no_str.s, si->port_no_str.len);
			p += si->port_no_str.len;
		}
	}

	/* creates pipes for networks */
	for(i=0;i<nr_of_networks;i++)
	{
		/* create the pipe*/
		if (pipe(net_pipe)==-1) {
			LM_ERR("failed to create pipe!\n");
			goto error;
		}
		networks[i].pipe_out = net_pipe[0];
		net_pipes_in[i] = net_pipe[1];
		/* sets reading from pipe to non blocking */
		if ((foo=fcntl(net_pipe[0],F_GETFL,0))<0) {
			LM_ERR("failed to get flag for pipe - fcntl\n");
			goto error;
		}
		foo |= O_NONBLOCK;
		if (fcntl(net_pipe[0],F_SETFL,foo)<0) {
			LM_ERR("failed to set flag for pipe - fcntl\n");
			goto error;
		}
	}

	/* if report will be used, init the report queue */
	if (sms_report_type!=NO_REPORT && !init_report_queue()) {
		LM_ERR("cannot get shm memory!\n");
		goto error;
	}

	/* alloc in shm for queued_msgs */
	queued_msgs = (int*)shm_malloc(sizeof(int));
	if (!queued_msgs) {
		LM_ERR("cannot get shm memory!\n");
		goto error;
	}
	*queued_msgs = 0;
	
	/* register nr_of_modems number of child processes that will
	 * update their local configuration */
	cfg_register_child(nr_of_modems);

	return 1;
error:
	return -1;
}




int sms_child_init(int rank)
{
	int  i, foo;

	/* only the child 1 will execute this */
	if (rank != 1) goto done;

	/* creates processes for each modem */
	for(i=0;i<nr_of_modems;i++)
	{
		if ( (foo=fork())<0 ) {
			LM_ERR("cannot fork \n");
			goto error;
		}
		if (!foo) {
			/* initialize the config framework */
			if (cfg_child_init()) goto error;

			modem_process(&(modems[i]));
			goto done;
		}
	}

done:
	return 0;
error:
	return-1;
}




static int sms_init(void)
{
	LM_INFO("SMS - initializing\n");

	if (parse_config_lines()==-1)
		return -1;
	if (global_init()==-1)
		return -1;
	return 0;
}




static int sms_exit(void)
{
	if (queued_msgs)
		shm_free(queued_msgs);

	if (sms_report_type!=NO_REPORT)
		destroy_report_queue();

	return 0;
}




static int w_sms_send_msg(struct sip_msg *msg, char *foo, char *bar)
{
	return push_on_network(msg, default_net);
}




static int w_sms_send_msg_to_net(struct sip_msg *msg, char *net_nr, char *foo)
{
	return push_on_network(msg,(unsigned int)(unsigned long)net_nr);
}

