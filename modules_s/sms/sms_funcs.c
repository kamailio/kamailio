#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "../../dprint.h"
#include "../im/im_funcs.h"
#include "sms_funcs.h"
#include "libsms_modem.h"
#include "libsms_put_sms.h"



struct modem modems[MAX_MODEMS];
struct network networks[MAX_NETWORKS];
int net_pipes_in[MAX_NETWORKS];
int nr_of_networks;
int nr_of_modems;




int push_on_network(struct sip_msg *msg, int net)
{
	str body,user,host;
	struct sms_msg sms_messg;
	struct to_body from_parsed;
	int foo;

	if ( im_extract_body(msg,&body)==-1 )
	{
		LOG(L_ERR,"ERROR:sms_push_on_net:cannot extract body from msg!\n");
		goto error;
	}

	if (im_get_user(msg,&user,&host)==-1 )
	{
		LOG(L_ERR,"ERROR:sms_push_on_net:cannot get user from msg!\n");
		goto error;
	}

	if (body.len<=MAX_SMS_LENGTH)
		foo = body.len;
	else {
		LOG(L_WARN,"WARNING:sms_push_on_net: message longer than %d->"
			"truncated!\n",MAX_SMS_LENGTH);
		foo = MAX_SMS_LENGTH;
	}
	memcpy(sms_messg.text, body.s, foo);
	if (msg->from) {
		parse_to(msg->from->body.s,msg->from->body.s+msg->from->body.len+1,
			&from_parsed);
		if (from_parsed.error!=PARSE_OK && foo+SMS_FROM_LEN+
		from_parsed.body.len>MAX_SMS_LENGTH) {
			LOG(L_WARN,"WARNING:sms_push_on_net: cannot apend FROM tag!!"
				"maximum length (%d) exceded!\n",MAX_SMS_LENGTH);
		} else {
			memcpy(sms_messg.text+foo,SMS_FROM,SMS_FROM_LEN);
			foo += SMS_FROM_LEN;
			memcpy(sms_messg.text+foo,from_parsed.body.s,from_parsed.body.len);
			foo += from_parsed.body.len;
		}
	}
	sms_messg.text[foo] = 0;

	if (user.len>MAX_CHAR_BUF) {
		LOG(L_ERR,"ERROR:sms_push_on_net: user (number) longer than %d\n",
			MAX_CHAR_BUF);
		goto error;
	}
	strncpy(sms_messg.to, user.s, user.len);
	sms_messg.to[user.len] = 0;

	sms_messg.is_binary = 0;
	sms_messg.udh = 1;
	sms_messg.cs_convert = 1;

	if (write(net_pipes_in[net], &sms_messg, sizeof(sms_messg))!=
	sizeof(sms_messg) )
	{
		LOG(L_ERR,"ERROR:sms_push_on_net: error when writting to pipe\n");
		goto error;
	}

	return 1;
error:
	return -1;
}




void modem_process(struct modem *mdm)
{
	struct sms_msg sms_messg;
	struct network *net;
	int i,len;
	int counter;
	int dont_wait;
	int empty_pipe;
	int last_smsc_index;

	/* let's open/init the modem */
	DBG("DEBUG:modem_process: openning modem\n");
	if (openmodem(mdm)==-1) {
		LOG(L_ERR,"ERROR:modem_process: cannot open modem %s!"
			" %s \n",mdm->name,strerror(errno));
		return;
	}
	setmodemparams(mdm);
	initmodem(mdm);
	last_smsc_index = -1;

	sleep(1);

	while(1)
	{
		dont_wait = 0;
		for (i=0;i<nr_of_networks && mdm->net_list[i]!=-1;i++)
		{
			counter = 0;
			empty_pipe = 0;
			net = &(networks[mdm->net_list[i]]);
			DBG("DEBUG:modem_process: %s processing sms for net %s \n",
				mdm->device, net->name);
			/*getting msgs from pipe*/
			while( counter<net->max_sms_per_call && !empty_pipe )
			{
				/* let's read a sms from pipe */
				len = read(net->pipe_out, &sms_messg,
					sizeof(sms_messg));
				if (len!=sizeof(sms_messg)) {
					if (len>=0)
						LOG(L_ERR,"ERROR:modem_process: truncated message"
						" read from pipe! -> discarted\n");
					else if (errno==EAGAIN) {
						DBG("DEBUG:modem_process: pipe emptied!! \n");
						empty_pipe = 1;
					}
					counter++;
					continue;
				}

				/*sets the apropriat sms center*/
				if (last_smsc_index!=mdm->net_list[i]) {
					setsmsc(mdm,net->smsc);
					last_smsc_index = mdm->net_list[i];
				}

				/* compute and send the sms */
				DBG("DEBUG:modem_process: processing sms: to:[%s] "
					"body=[%s]\n",sms_messg.to,sms_messg.text);
				putsms( &sms_messg , mdm);

				counter++;
				/* if I reached the limit -> set not to wait */
				if (counter==net->max_sms_per_call)
					dont_wait = 1;
			}/*while*/
		}/*for*/
		if (!dont_wait)
			sleep(mdm->looping_interval);
	}/*while*/
}






