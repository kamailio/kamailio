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
#include "../im/im_funcs.h"
#include "sms_funcs.h"



static int sms_init(void);
static int sms_exit(void);
static int w_sms_send_msg(struct sip_msg*, char*, char* );
static int w_sms_send_msg_to_net(struct sip_msg*, char*, char*);



/* parameters */
char *networks_config;
char *modems_config;
int  looping_interval;
int  max_sms_per_call;



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
				0,
				0
		},
	2,

	(char*[]) {   /* Module parameter names */
		"networks",
		"modems",
		"looping_interval",
		"max_sms_per_call",
		"default_net"
	},
	(modparam_t[]) {   /* Module parameter types */
		STR_PARAM,
		STR_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&networks_config,
		&modems_config,
		&looping_interval,
		&max_sms_per_call,
		&default_net
	},
	5,      /* Number of module paramers */

	sms_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) sms_exit,   /* module exit function */
	0,
	0  /* per-child init function */
};



#define eat_spaces(_p) \
	while( *(_p)==' ' || *(_p)=='\t' ){\
	(_p)++;}



int parse_config_lines()
{
	char *p,*start;
	int  i, k, step = 1;

	nr_of_networks = 0;
	nr_of_modems = 0;

	/* parsing networks configuration string */
	p = networks_config;
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
		/*get the sms center number*/
		eat_spaces(p);
		if (*p!='[')
			goto parse_error;
		p++;
		eat_spaces(p);
		start = p;
		while(*p!=' ' && *p!='\t' && *p!=']' && *p!=0)
			p++;
		if ( p==start || *p==0 )
			goto parse_error;
		memcpy(networks[nr_of_networks].smsc, start, p-start);
		networks[nr_of_networks].smsc[p-start] = 0;
		DBG("DEBUG: sms startup: network found <%s> smsc=<%s>\n",
			networks[nr_of_networks].name, networks[nr_of_networks].smsc);
		eat_spaces(p);
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
		LOG(L_ERR,"ERROR:SMS parse config networks - no network found!\n");
		goto error;
	}

	step++;
	/* parsing modems configuration string */
	p = modems_config;
	while (*p)
	{
		eat_spaces(p);
		/*get modem's device*/
		start = p;
		while (*p!=' ' && *p!='\t' && *p!='[' && *p!=0)
			p++;
		if ( p==start || *p==0 )
			goto parse_error;
		memcpy(modems[nr_of_modems].device, start, p-start);
		modems[nr_of_modems].device[p-start] = 0;
		memset(modems[nr_of_modems].net_list,0XFF,
			sizeof(modems[nr_of_modems].net_list) );
		DBG("DEBUG: sms startup: modem on <%.*s> found \n",p-start,start);
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
			while(*p!=' ' && *p!='\t' && *p!=']' && *p!=',' && *p!=0)
				p++;
			if ( p==start || *p==0 )
				goto parse_error;
			DBG("DEBUG:sms startup: associated net found <%.*s>\n",
				p-start,start);
			/* lookup for the network -> get its index */
			for(i=0;i<nr_of_networks;i++) {
				if (!strncasecmp(networks[i].name,start,p-start)
				&& networks[i].name[p-start]==0)
				{
					modems[nr_of_modems].net_list[k++]=i;
					i = -1;
					break;
				}
			}
			if (i!=-1) {
				LOG(L_ERR,"ERROR:SMS parse modem config - associated"
					" net <%.*s> not found in net list\n",p-start,start);
				goto error;
			}
			eat_spaces(p);
			if (*p==',') {
				p++;
				eat_spaces(p);
			}
		}
		if (*p!=']')
			goto parse_error;
		p++;
		/* end of element */
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

	return 0;
parse_error:
	LOG(L_ERR,"ERROR: SMS %s config: parse error before  chr %d [%.*s]\n",
		(step==1)?"networks":"modems",
		p - ((step==1)?networks_config:modems_config),
		(*p==0)?4:1,(*p==0)?"NULL":p );
error:
	return -1;
}




int start_device_processes()
{
	int i, net_pipe[2], foo;

	/* creats pipes for networks */
	for(i=0;i<nr_of_networks;i++)
	{
		/* create the pipe*/
		if (pipe(net_pipe)==-1) {
			LOG(L_ERR,"ERROR: sms_init: cannot create pipe!\n");
			goto error;
		}
		networks[i].pipe_out = net_pipe[0];
		net_pipes_in[i] = net_pipe[1];
		/* sets reading from pipe to non blocking */
		if ((foo=fcntl(net_pipe[0],F_GETFL,0))<0) {
			LOG(L_ERR,"ERROR: sms_init: cannot get flag for pipe - fcntl\n");
			goto error;
		}
		foo |= O_NONBLOCK;
		if (fcntl(net_pipe[0],F_SETFL,foo)<0) {
			LOG(L_ERR,"ERROR: sms_init: cannot set flag for pipe - fcntl\n");
			goto error;
		}
	}

	/* creats processes for each modem */
	for(i=0;i<nr_of_modems;i++)
	{
		if ( (foo=fork())<0 ) {
			LOG(L_ERR,"ERROR: sms_init: cannot fork \n");
			goto error;
		}
		if (!foo) {
			modem_process(&(modems[i]));
			exit(0);
		}
	}

	return 0;
error:
	return-1;
}




static int sms_init(void)
{
	printf("sms - initializing\n");

	if (parse_config_lines()==-1)
		goto error;

	if (start_device_processes()==-1)
		goto error;

	return 0;
error:
	return -1;
}




static int sms_exit(void)
{
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

