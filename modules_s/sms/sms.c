/*
 * $Id$
 *
 * MAXFWD module
 *
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
#include "../im/im_funcs.h"
#include "sms_funcs.h"
#include "sms_report.h"
#include "libsms_modem.h"



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
char *domain_str      = 0;

/*global vaiables*/
int  default_net    = 0;
int  max_sms_parts  = MAX_SMS_PARTS;
str  domain;
int  *queued_msgs   = 0;
int  use_contact    = 0;
int  use_sms_report = 0;


struct module_exports exports= {
	"sms",
	(char*[]){
				"sms_send_msg_to_net",
				"sms_send_msg"
			},
	(cmd_function[]){
					w_sms_send_msg_to_net,
					w_sms_send_msg
					},
	(int[]){
				1,
				0
			},
	(fixup_function[]){
				fixup_sms_send_msg_to_net,
				0
		},
	2,

	(char*[]) {   /* Module parameter names */
		"networks",
		"modems",
		"links",
		"default_net",
		"max_sms_parts",
		"domain",
		"use_contact",
		"use_sms_report"
	},
	(modparam_t[]) {   /* Module parameter types */
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		INT_PARAM,
		STR_PARAM,
		INT_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&networks_config,
		&modems_config,
		&links_config,
		&default_net_str,
		&max_sms_parts,
		&domain_str,
		&use_contact,
		&use_sms_report
	},
	8,      /* Number of module paramers */

	sms_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) sms_exit,   /* module exit function */
	0,
	(child_init_function) sms_child_init  /* per-child init function */
};




static int fixup_sms_send_msg_to_net(void** param, int param_no)
{
	int net_nr,i;

	if (param_no==1) {
		for(net_nr=-1,i=0;i<nr_of_networks&&net_nr==-1;i++)
			if (!strcasecmp(networks[i].name,*param))
				net_nr = i;
		if (net_nr==-1) {
			LOG(L_ERR,"ERROR:fixup_sms_send_msg_to_net: network \"%s\""
				" not found in net list!\n",(char*)*param);
			return E_UNSPEC;
		} else {
			free(*param);
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
		LOG(L_ERR,"ERROR: invalid parameter syntax near [=]\n");
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
				LOG(L_ERR,"ERROR: invalid value \"%.*s\" for param [m]\n",
					arg_end-arg-2,arg+2);
				goto error;
			}
			break;
		case 'r':  /* retry time */
			foo=str2s(arg+2,arg_end-arg-2,&err);
			if (err) {
				LOG(L_ERR,"ERROR:set_modem_arg: cannot convert [r] arg to"
					" integer!\n");
				goto error;
			}
			mdm->retry = foo;
			break;
		case 'l':  /* looping interval */
			foo=str2s(arg+2,arg_end-arg-2,&err);
			if (err) {
				LOG(L_ERR,"ERROR:set_modem_arg: cannot convert [l] arg to"
					" integer!\n");
				goto error;
			}
			mdm->looping_interval = foo;
			break;
		case 'b':  /* baudrate */
			foo=str2s(arg+2,arg_end-arg-2,&err);
			if (err) {
				LOG(L_ERR,"ERROR:set_modem_arg: cannot convert [b] arg to"
					" integer!\n");
				goto error;
			}
			switch (foo) {
				case   300: foo=B300; break;
				case  1200: foo=B1200; break;
				case  2400: foo=B2400; break;
				case  9600: foo=B9600; break;
				case 19200: foo=B19200; break;
				case 38400: foo=B38400; break;
				default:
					LOG(L_ERR,"ERROR:set_modem_arg: unsupported value %d "
						"for [b] arg!\n",foo);
					goto error;
			}
			mdm->baudrate = foo;
			break;
		default:
			LOG(L_ERR,"ERROR:set_modem_arg: unknow param name [%c]\n",*arg);
	}

	return 1;
error:
	return -1;
}




int set_network_arg(struct network *net, char *arg, char *arg_end)
{
	int err,foo;

	if (*(arg+1)!='=') {
		LOG(L_ERR,"ERROR:set_network_arg:invalid parameter syntax near [=]\n");
		goto error;
	}
	switch (*arg)
	{
		case 'c':  /* sms center number */
			memcpy(net->smsc,arg+2,arg_end-arg-2);
			net->smsc[arg_end-arg-2] = 0;
			break;
		case 'm':  /* maximum sms per one call */
			foo=str2s(arg+2,arg_end-arg-2,&err);
			if (err) {
				LOG(L_ERR,"ERROR:set_network_arg: cannot convert [m] arg to"
					" integer!\n");
				goto error;
			}
			net->max_sms_per_call = foo;
			break;
		default:
			LOG(L_ERR,"ERROR:set_network_arg: unknow param name [%c]\n",*arg);
	}

	return 1;
error:
	return -1;
}




int parse_config_lines()
{
	char *p,*start;
	int  i, k, step = 1;
	int  mdm_nr, net_nr;

	nr_of_networks = 0;
	nr_of_modems = 0;

	step = 0;
	/* parsing modems configuration string */
	if ( (p = modems_config)==0) {
		LOG(L_ERR,"ERROR:SMS parse_config_lines: param \"modems\" not"
			" found\n");
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
		modems[nr_of_modems].device[0] = 0;
		modems[nr_of_modems].pin[0] = 0;
		modems[nr_of_modems].mode = MODE_NEW;
		modems[nr_of_modems].retry = 10;
		modems[nr_of_modems].looping_interval = 20;
		modems[nr_of_modems].baudrate = B9600;
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
			LOG(L_ERR,"ERROR:SMS parse config modems: modem %s has no device"
				" associated\n",modems[nr_of_modems].name);
			goto error;
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
		LOG(L_ERR,"ERROR:SMS parse config modems - no modem found!\n");
		goto error;
	}

	step++;
	/* parsing networks configuration string */
	if ( (p = networks_config)==0) {
		LOG(L_ERR,"ERROR:SMS parse_config_lines: param \"networks\" not "
			"found\n");
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
		networks[nr_of_networks].smsc[0] = 0;
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
		if (networks[nr_of_networks].smsc[0]==0) {
			LOG(L_ERR,"ERROR:SMS parse config networks: network %s has no sms"
				" center associated\n",networks[nr_of_networks].name);
			goto error;
		}
		nr_of_networks++;
		eat_spaces(p);
		if (*p==';')
			p++;
		eat_spaces(p);
	}
	if (nr_of_networks==0)
	{
		LOG(L_ERR,"ERROR:SMS parse config networks - no network found!\n");
		goto error;
	}

	step++;
	/* parsing links configuration string */
	if ( (p = links_config)==0) {
		LOG(L_ERR,"ERROR:SMS parse_config_lines: param \"links\" not "
			"found\n");
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
			LOG(L_ERR,"ERROR:sms_parse_conf_line: unknown modem %.*s \n,",
				p-start, start);
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
				LOG(L_ERR,"ERROR:SMS parse modem config - associated"
					" net <%.*s> not found in net list\n",p-start,start);
				goto error;
			}
			DBG("DEBUG:sms startup: linking net \"%s\" to modem \"%s\" on "
				"pos %d.\n",networks[net_nr].name,modems[mdm_nr].name,k);
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

	/* resloving default setwork name - if any*/
	if (default_net_str) {
		for(net_nr=-1,i=0;i<nr_of_networks&&net_nr==-1;i++)
			if (!strcasecmp(networks[i].name,default_net_str))
				net_nr = i;
		if (net_nr==-1) {
			LOG(L_ERR,"ERROR:SMS setting default net: network \"%s\""
				" not found in net list!\n",default_net_str);
			goto error;
		}
		default_net = net_nr;
	}

	return 0;
parse_error:
	LOG(L_ERR,"ERROR: SMS %s config: parse error before  chr %d [%.*s]\n",
		(step==1)?"modems":(step==2?"netwoks":"links"),
		p - ((step==1)?modems_config:(step==2?networks_config:links_config)),
		(*p==0)?4:1,(*p==0)?"NULL":p );
error:
	return -1;
}




int global_init()
{
	int  i, net_pipe[2], foo;
	char *p;

	/*fix domain lenght*/
	if (domain_str) {
		domain.s = domain_str;
		domain.len = strlen(domain_str);
	} else {
		/*do I have to add port?*/
		i = (sock_info[0].port_no_str.len && sock_info[0].port_no!=5060);
		domain.len = sock_info[0].name.len + i*sock_info[0].port_no_str.len;
		domain.s = (char*)pkg_malloc(domain.len);
		if (!domain.s) {
			LOG(L_ERR,"ERROR:sms_init_child: no free pkg memory!\n");
			goto error;
		}
		p = domain.s;
		memcpy(p,sock_info[0].name.s,sock_info[0].name.len);
		p += sock_info[0].name.len;
		if (i) {
			memcpy(p,sock_info[0].port_no_str.s,sock_info[0].port_no_str.len);
			p += sock_info[0].port_no_str.len;
		}
	}

	/* creats pipes for networks */
	for(i=0;i<nr_of_networks;i++)
	{
		/* create the pipe*/
		if (pipe(net_pipe)==-1) {
			LOG(L_ERR,"ERROR: sms_global_init: cannot create pipe!\n");
			goto error;
		}
		networks[i].pipe_out = net_pipe[0];
		net_pipes_in[i] = net_pipe[1];
		/* sets reading from pipe to non blocking */
		if ((foo=fcntl(net_pipe[0],F_GETFL,0))<0) {
			LOG(L_ERR,"ERROR: sms_global_init: cannot get flag for pipe"
				" - fcntl\n");
			goto error;
		}
		foo |= O_NONBLOCK;
		if (fcntl(net_pipe[0],F_SETFL,foo)<0) {
			LOG(L_ERR,"ERROR: sms_global_init: cannot set flag for pipe"
				" - fcntl\n");
			goto error;
		}
	}

	/* if report will be used, init the report queue */
	if (use_sms_report && !init_report_queue()) {
		LOG(L_ERR,"ERROR: sms_global_init: cannot get shm memory!\n");
		goto error;
	}

	/* alloc in shm for queued_msgs */
	queued_msgs = (int*)shm_malloc(sizeof(int));
	if (!queued_msgs) {
		LOG(L_ERR,"ERROR: sms_global_init: cannot get shm memory!\n");
		goto error;
	}
	*queued_msgs = 0;

	return 1;
error:
	return -1;
}




int sms_child_init(int rank)
{
	int  i, foo;

	/* only the child 0 will execut this */
	if (rank)
		goto done;

	/* creats processes for each modem */
	for(i=0;i<nr_of_modems;i++)
	{
		if ( (foo=fork())<0 ) {
			LOG(L_ERR,"ERROR: sms_child_init: cannot fork \n");
			goto error;
		}
		if (!foo) {
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
	printf("sms - initializing\n");

	if (parse_config_lines()==-1)
		return -1;
	if (global_init()==-1)
		return -1;
	return 0;
}




static int sms_exit(void)
{
	if (!domain_str)
		pkg_free(domain.s);

	if (queued_msgs)
		shm_free(queued_msgs);

	if (use_sms_report)
		destroy_report_queue();

	return 0;
}




static int w_sms_send_msg(struct sip_msg *msg, char *foo, char *bar)
{
	return push_on_network(msg, default_net);
}




static int w_sms_send_msg_to_net(struct sip_msg *msg, char *net_nr, char *foo)
{
	return push_on_network(msg,(unsigned int)net_nr);
}

