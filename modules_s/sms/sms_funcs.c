#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "../../dprint.h"
#include "../../config.h"
#include "../../globals.h"
#include "../../mem/mem.h"
#include "../im/im_funcs.h"
#include "sms_funcs.h"
#include "libsms_modem.h"
#include "libsms_sms.h"




struct modem modems[MAX_MODEMS];
struct network networks[MAX_NETWORKS];
int net_pipes_in[MAX_NETWORKS];
int nr_of_networks;
int nr_of_modems;




#define ERR_REPLY_TEXT " is an invalid number! Please resend your SMS using "\
	"a number in (contry code)(area code)(local number) format. Thanks for "\
	" using our service!"
#define ERR_REPLY_TEXT_LEN sizeof(ERR_REPLY_TEXT)-1

#define SMS_FROM_TAG ";tag=qwer-4321-m9n8-r6y2"
#define SMS_FROM_TAG_LEN sizeof(SMS_FROM_TAG)-1

#define append_str(_p,_s,_l) \
	memcpy((_p),(_s),(_l));\
	(_p) += (_l);




int push_on_network(struct sip_msg *msg, int net)
{
	str    body;
	struct sip_uri uri;
	struct sms_msg sms_messg;
	struct to_body from_parsed;
	int    foo;
	char   *p;

	if ( im_extract_body(msg,&body)==-1 )
	{
		LOG(L_ERR,"ERROR:sms_push_on_net:cannot extract body from msg!\n");
		goto error;
	}

	if (parse_uri(msg->first_line.u.request.uri.s,
		msg->first_line.u.request.uri.len, &uri) <0 )
	{
		LOG(L_ERR,"ERROR:sms_push_on_net:unable to parse uri\n");
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
		memset(&from_parsed,0,sizeof(from_parsed));
		parse_to(msg->from->body.s,msg->from->body.s+msg->from->body.len+1,
			&from_parsed);
		if (from_parsed.error==PARSE_OK ) {
			memcpy(sms_messg.from,from_parsed.uri.s,from_parsed.uri.len);
			sms_messg.from_len = from_parsed.uri.len;
			if (foo+SMS_FROM_LEN+from_parsed.body.len>MAX_SMS_LENGTH) {
				LOG(L_WARN,"WARNING:sms_push_on_net: cannot append FROM tag!!"
					"maximum length (%d) exceded!\n",MAX_SMS_LENGTH);
			} else {
				memcpy(sms_messg.text+foo,SMS_FROM,SMS_FROM_LEN);
				foo += SMS_FROM_LEN;
				memcpy(sms_messg.text+foo,from_parsed.body.s,
					from_parsed.body.len);
				foo += from_parsed.body.len;
			}
		} else {
			LOG(L_ERR,"ERROR:sms_push_on_net: cannot parse from!\n");
			goto error;
		}
	}
	sms_messg.text_len = foo;

	sms_messg.to_user_len = uri.user.len;
	sms_messg.to_len = uri.user.len + 1 + uri.host.len
		+ ((uri.port.s)?(1+uri.port.len):0);
	if (sms_messg.to_len>MAX_CHAR_BUF) {
		LOG(L_ERR,"ERROR:sms_push_on_net: to address (user@host:port)"
			" longer than %d\n",MAX_CHAR_BUF);
		goto error;
	}
	p = sms_messg.to;
	append_str(p,uri.user.s,uri.user.len);
	*(p++) = '@';
	append_str(p,uri.host.s,uri.host.len);
	if (uri.port.s) {
		*(p++) = ':';
		append_str(p,uri.port.s,uri.port.len);
	}

	sms_messg.is_binary = 0;
	sms_messg.udh = 1;
	sms_messg.cs_convert = 1;

	if (write(net_pipes_in[net], &sms_messg, sizeof(sms_messg))!=
	sizeof(sms_messg) )
	{
		LOG(L_ERR,"ERROR:sms_push_on_net: error when writting to pipe\n");
		goto error;
	}

	free_uri(&uri);
	return 1;
error:
	free_uri(&uri);
	return -1;
}





int build_and_send_sip(struct sms_msg *sms_messg)
{
	str to;
	str from;
	str contact;
	str msg;
	int foo;
	char *p;

	from.s = contact.s = msg.s = 0;

	to.s = sms_messg->from;
	to.len = sms_messg->from_len;

	from.len = 5 /*"<sip:"*/ +  sms_messg->to_len/*address*/ + 1 /*">"*/
		+ SMS_FROM_TAG_LEN;
	from.s = (char*)pkg_malloc(from.len);
	if (!from.s)
		goto error;
	p=from.s;
	append_str(p,"<sip:",5);
	append_str(p,sms_messg->to,sms_messg->to_len);
	*(p++)='>';
	append_str(p,SMS_FROM_TAG,SMS_FROM_TAG_LEN);

	contact.len = 5 /*"<sip:"*/ + sock_info[0].address_str.len + 3 /*":9>"*/;
	contact.s = (char*)pkg_malloc(contact.len);
	if (!contact.s)
		goto error;
	p=contact.s;
	append_str(p,"<sip:",5);
	append_str(p,sock_info[0].address_str.s,sock_info[0].address_str.len);
	append_str(p,":9>",3);

	msg.len = sms_messg->to_user_len /*number*/ + ERR_REPLY_TEXT_LEN;
	msg.s = (char*)pkg_malloc(msg.len);
	if (!msg.s)
		goto error;
	p=msg.s;
	append_str(p,sms_messg->to,sms_messg->to_user_len);
	append_str(p,ERR_REPLY_TEXT,ERR_REPLY_TEXT_LEN );

	foo = im_send_message(&to, &from, &contact, &msg);
	if (from.s) pkg_free(from.s);
	if (contact.s) pkg_free(contact.s);
	if (msg.s) pkg_free(msg.s);
	return foo;
error:
	LOG(L_ERR,"ERROR:sms_build_and_send_sip: no free pkg memory!\n");
	if (from.s) pkg_free(from.s);
	if (contact.s) pkg_free(contact.s);
	if (msg.s) pkg_free(msg.s);
	return -1;
}





void modem_process(struct modem *mdm)
{
	struct sms_msg sms_messg;
	struct incame_sms sms;
	struct network *net;
	int i,j,k,len;
	int counter;
	int dont_wait;
	int empty_pipe;
	int last_smsc_index;
	int cpms_unsuported;
	int max_mem=0, used_mem=0;
	int have_error=0;

	cpms_unsuported = 0;
	last_smsc_index = -1;

	/* let's open/init the modem */
	DBG("DEBUG:modem_process: openning modem\n");
	if (openmodem(mdm)==-1) {
		LOG(L_ERR,"ERROR:modem_process: cannot open modem %s!"
			" %s \n",mdm->name,strerror(errno));
		return;
	}

	setmodemparams(mdm);
	initmodem(mdm);

	if ( (max_mem=check_memory(mdm,MAX_MEM))==-1 ) {
		LOG(L_WARN,"WARNING:modem_process: CPMS command unsuported!"
			" using default values (10,10)\n");
		used_mem = max_mem = 10;
		cpms_unsuported = 1;
	}

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
						DBG("DEBUG:modem_process: out pipe emptied!! \n");
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
				DBG("DEBUG:modem_process: processing sms: to:[%.*s] "
					"body=[%.*s]\n",sms_messg.to_user_len,sms_messg.to,
					sms_messg.text_len,sms_messg.text);
				if ( putsms( &sms_messg , mdm)==-1 ) {
					build_and_send_sip(&sms_messg);
					last_smsc_index = -1;
				}

				counter++;
				/* if I reached the limit -> set not to wait */
				if (counter==net->max_sms_per_call)
					dont_wait = 1;
			}/*while*/
		}/*for*/

		/* let's see if we have incomming sms */
		if ( !cpms_unsuported )
			if ((used_mem = check_memory(mdm,USED_MEM))==-1) {
				LOG(L_ERR,"ERROR:modem_process: CPMS command failed!"
					" cannot get used mem -> using 10\n");
				used_mem = 10;
				last_smsc_index = -1;
			}
		/* if any, let's get them */
		if (used_mem)
			for(i=1,k=1;k<=used_mem && i<=max_mem;i++) {
				if (getsms(&sms,mdm,i)!=-1) {
					k++;
					DBG("SMS Get from location %d\n",i);
					/*for test ;-) ->  to be remove*/
					DBG("SMS RECEIVED:\n\rFrom: %s %s\n\r%s %s"
						"\n\r\"%s\"\n\r",sms.sender,sms.name,
						sms.date,sms.time,sms.ascii);
				}
			}

		if (!dont_wait)
			sleep(mdm->looping_interval);
	}/*while*/
}






