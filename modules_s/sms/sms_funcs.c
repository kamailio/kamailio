#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "../../dprint.h"
#include "../../config.h"
#include "../../globals.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../im/im_funcs.h"
#include "sms_funcs.h"
#include "libsms_modem.h"
#include "libsms_sms.h"




struct modem modems[MAX_MODEMS];
struct network networks[MAX_NETWORKS];
int net_pipes_in[MAX_NETWORKS];
int nr_of_networks;
int nr_of_modems;
int max_sms_parts;
int *queued_msgs;



#define ERR_NUMBER_TEXT " is an invalid number! Please resend your SMS "\
	"using a number in (contry code)(area code)(local number) format. Thanks"\
	" for using our service!"
#define ERR_NUMBER_TEXT_LEN (sizeof(ERR_NUMBER_TEXT)-1)

#define ERR_TRUNCATE_TEXT "We are sorry, but your message exceeded our "\
	"maximum allowed length. The following part of the message wasn't sent"\
	" : "
#define ERR_TRUNCATE_TEXT_LEN (sizeof(ERR_TRUNCATE_TEXT)-1)

#define ERR_MODEM_TEXT "Due to our modem temporary indisponibility, "\
	"the following message couldn't be sent : "
#define ERR_MODEM_TEXT_LEN (sizeof(ERR_MODEM_TEXT)-1)

#define SMS_FROM_TAG ";tag=qwer-4321-m9n8-r6y2"
#define SMS_FROM_TAG_LEN (sizeof(SMS_FROM_TAG)-1)

#define append_str(_p,_s,_l) \
	{memcpy((_p),(_s),(_l));\
	(_p) += (_l);}




int push_on_network(struct sip_msg *msg, int net)
{
	str    body;
	struct sip_uri to_uri;
	struct sms_msg sms_messg;
	struct to_body from_parsed;
	char   *p;

	if (*queued_msgs>MAX_QUEUED_MESSAGES)
		goto error1;
	(*queued_msgs)++;

	if ( im_extract_body(msg,&body)==-1 )
	{
		LOG(L_ERR,"ERROR:sms_push_on_net:cannot extract body from msg!\n");
		goto error1;
	}

	if (!msg->from) {
		LOG(L_ERR,"ERROR:sms_push_on_net: no FROM header found!\n");
		goto error1;
	}

	/* parsing to */
	if (!msg->to || !get_to(msg)
	|| parse_uri( get_to(msg)->uri.s, get_to(msg)->uri.len, &to_uri) )  {
		LOG(L_ERR,"ERROR:sms_push_on_net: unable to extract user name from"
			" To header!\n");
		goto error1;
	}

	/* parsing from header */
	memset(&from_parsed,0,sizeof(from_parsed));
	parse_to(msg->from->body.s,msg->from->body.s+msg->from->body.len+1,
		&from_parsed);
	if (from_parsed.error!=PARSE_OK ) {
		LOG(L_ERR,"ERROR:sms_push_on_net: cannot parse from header\n");
		goto error;
	}

	/* copy "from" into sms struct */
	memcpy(sms_messg.from,from_parsed.uri.s,from_parsed.uri.len);
	sms_messg.from_len = from_parsed.uri.len;

	/* compossing sms body */
	sms_messg.text_len = SMS_HDR_BF_ADDR_LEN + sms_messg.from_len
		+ SMS_HDR_AF_ADDR_LEN + body.len+SMS_FOOTER_LEN;
	sms_messg.text = (char*)shm_malloc(sms_messg.text_len);
	if (!sms_messg.text) {
		LOG(L_ERR,"ERROR:sms_push_on_net: cannot get shm memory!\n");
		goto error;
	}
	p = sms_messg.text;
	append_str(p, SMS_HDR_BF_ADDR, SMS_HDR_BF_ADDR_LEN);
	append_str(p, sms_messg.from, sms_messg.from_len);
	append_str(p, SMS_HDR_AF_ADDR, SMS_HDR_AF_ADDR_LEN);
	append_str(p, body.s, body.len);
	append_str(p, SMS_FOOTER, SMS_FOOTER_LEN);

	/* copy user from "to" */
	sms_messg.to_len = to_uri.user.len;
	if (sms_messg.to_len>MAX_CHAR_BUF) {
		LOG(L_ERR,"ERROR:sms_push_on_net: user tel number"
			" longer than %d\n",MAX_CHAR_BUF);
		goto error;
	}
	p = sms_messg.to;
	append_str(p,to_uri.user.s,to_uri.user.len);

	/* setting up sms characteristics */
	sms_messg.is_binary = 0;
	sms_messg.udh = 1;
	sms_messg.cs_convert = 1;

	if (write(net_pipes_in[net], &sms_messg, sizeof(sms_messg))!=
	sizeof(sms_messg) )
	{
		LOG(L_ERR,"ERROR:sms_push_on_net: error when writting to pipe\n");
		shm_free(sms_messg.text);
		goto error;
	}

	free_uri(&to_uri);
	return 1;
error:
	free_uri(&to_uri);
error1:
	return -1;
}





int send_sip_msg_request(str *to, str *from_user, str *body)
{
	str from;
	str contact;
	int foo;
	char *p;

	from.s = contact.s = 0;

	/* From header */
	from.len = 5 /*"<sip:"*/ +  from_user->len/*user*/ + 1/*"@"*/
		+ domain.len /*host*/ + 1 /*">"*/ + SMS_FROM_TAG_LEN;
	from.s = (char*)pkg_malloc(from.len);
	if (!from.s)
		goto error;
	p=from.s;
	append_str(p,"<sip:",5);
	append_str(p,from_user->s,from_user->len);
	*(p++)='@';
	append_str(p,domain.s,domain.len);
	*(p++)='>';
	append_str(p,SMS_FROM_TAG,SMS_FROM_TAG_LEN);

	/* Contact header */
	contact.len = 5 /*"<sip:"*/ + domain.len/*host*/ + 1 /*">"*/;
	contact.s = (char*)pkg_malloc(contact.len);
	if (!contact.s)
		goto error;
	p=contact.s;
	append_str(p,"<sip:",5);
	append_str(p,domain.s,domain.len);
	*(p++)='>';

	foo = im_send_message(to, &from, &contact, body);
	if (from.s) pkg_free(from.s);
	if (contact.s) pkg_free(contact.s);
	return foo;
error:
	LOG(L_ERR,"ERROR:sms_build_and_send_sip: no free pkg memory!\n");
	if (from.s) pkg_free(from.s);
	if (contact.s) pkg_free(contact.s);
	return -1;
}




inline int send_error(struct sms_msg *sms_messg, char *msg1_s, int msg1_len,
													char *msg2_s, int msg2_len)
{
	str  body;
	str  from;
	str  to;
	char *p;
	int  foo;

	/* from */
	from.s = sms_messg->from;
	from.len = sms_messg->from_len;
	/* to */
	to.len = sms_messg->to_len;
	to.s = sms_messg->to;
	/* body */
	body.len = msg1_len + msg2_len;
	body.s = (char*)pkg_malloc(body.len);
	if (!body.s)
		goto error;
	p=body.s;
	append_str(p, msg1_s, msg1_len );
	append_str(p, msg2_s, msg2_len);

	/* sending */
	foo = send_sip_msg_request( &from, &to, &body);
	pkg_free( body.s );
	return foo;
error:
	LOG(L_ERR,"ERROR:sms_send_error: no free pkg memory!\n");
	return -1;

}



inline unsigned short *split_text(char *text, int text_len, int *nr,int nice)
{
	static unsigned short lens[30];
	int  nr_chunks;
	int  k,k1,len;
	char c;

	nr_chunks = 0;
	len = 0;

	do{
		k = MAX_SMS_LENGTH-(nice&&nr_chunks?SMS_EDGE_PART_LEN:0);
		if ( len+k<text_len ) {
			/* is not the last piece :-( */
			if (nice && !nr_chunks) k -= SMS_EDGE_PART_LEN;
			if (text_len-len-k<=SMS_FOOTER_LEN+4)
				k = (text_len-len)/2;
			/* ->looks for a point to split */
			k1 = k;
			while( k>0 && (c=text[len+k-1])!='.' && c!=' ' && c!=';'
			&& c!='\r' && c!='\n' && c!='-' && c!='!' && c!='?' && c!='+'
			&& c!='=' && c!='\t' && c!='\'')
				k--;
			if (k<k1/2)
				/* wast of space !!!!*/
				k=k1;
			len += k;
			lens[nr_chunks] = k;
		}else {
			/*last chunk*/
			lens[nr_chunks] = text_len-len;
			len = text_len;
		}
		nr_chunks++;
	}while (len<text_len);

	if (nr) *nr = nr_chunks;
	return lens;
}




inline int send_as_sms(struct sms_msg *sms_messg, struct modem *mdm)
{
	static char  buf[MAX_SMS_LENGTH];
	unsigned int buf_len;
	unsigned short *len_array, *len_array_nice;
	unsigned int   nr_chunks,  nr_chunks_nice;
	unsigned int use_nice;
	char *text;
	int  text_len;
	char *p, *q;
	int  ret_code;
	int  i;

	use_nice = 0;
	text = sms_messg->text;
	text_len = sms_messg->text_len;

	len_array = split_text(text, text_len, &nr_chunks,0);
	len_array_nice = split_text(text, text_len, &nr_chunks_nice,1);
	if (nr_chunks_nice==nr_chunks) {
		len_array = len_array_nice;
		use_nice = 1;
	}

	for(i=0,p=text;i<nr_chunks&&i<max_sms_parts;p+=len_array[i++]) {
		if (use_nice) {
			q = buf;
			if (nr_chunks>1 && i)  {
				append_str(q,SMS_EDGE_PART,SMS_EDGE_PART_LEN);
				*(q-2)=nr_chunks+'0';
				*(q-4)=i+1+'0';
			}
			append_str(q,p,len_array[i]);
			if (nr_chunks>1 && !i)  {
				append_str(q,SMS_EDGE_PART,SMS_EDGE_PART_LEN);
				*(q-2)=nr_chunks+'0';
				*(q-4)=i+1+'0';
			}
			buf_len = q-buf;
		} else {
			q = buf;
			append_str(q,p,len_array[i]);
			buf_len = len_array[i];
		}
		if (i+1==max_sms_parts && i+1<nr_chunks) {
			/* simply override the end of the last allowed part */
			buf_len = MAX_SMS_LENGTH;
			q = buf + (MAX_SMS_LENGTH-SMS_TRUNCATED_LEN-SMS_FOOTER_LEN);
			append_str(q,SMS_TRUNCATED,SMS_TRUNCATED_LEN);
			append_str(q,SMS_FOOTER,SMS_FOOTER_LEN);
			p += MAX_SMS_LENGTH-SMS_TRUNCATED_LEN-SMS_FOOTER_LEN; 
			send_error(sms_messg, ERR_TRUNCATE_TEXT, ERR_TRUNCATE_TEXT_LEN,
				p, text_len-(p-text)-SMS_FOOTER_LEN);
		}
		DBG("---%d--<%d>--\n|%.*s|\n",i,buf_len,buf_len,buf);
		sms_messg->text = buf;
		sms_messg->text_len = buf_len;
		if ( (ret_code=putsms(sms_messg,mdm))!=1)
			goto error;
	}

	shm_free(sms_messg->text);
	return 1;
error:
	shm_free(sms_messg->text);
	if (ret_code==-1)
		/* bad number */
		send_error(sms_messg, sms_messg->to, sms_messg->to_len,
			ERR_NUMBER_TEXT, ERR_NUMBER_TEXT_LEN);
	else if (ret_code==-2)
		/* bad modem */
		send_error(sms_messg, ERR_MODEM_TEXT, ERR_MODEM_TEXT_LEN,
			text+SMS_HDR_BF_ADDR_LEN+sms_messg->from_len+SMS_HDR_AF_ADDR_LEN,
			text_len-SMS_FOOTER_LEN-SMS_HDR_BF_ADDR_LEN-sms_messg->from_len-
			SMS_HDR_AF_ADDR_LEN );
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
				(*queued_msgs)--;

				/*sets the apropriat sms center*/
				if (last_smsc_index!=mdm->net_list[i]) {
					setsmsc(mdm,net->smsc);
					last_smsc_index = mdm->net_list[i];
				}

				/* compute and send the sms */
				DBG("DEBUG:modem_process: processing sms: to:[%.*s] "
					"body=[%.*s]\n",sms_messg.to_len,sms_messg.to,
					sms_messg.text_len,sms_messg.text);
				if ( send_as_sms( &sms_messg , mdm)==-1 )
					last_smsc_index = -1;

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
#ifdef gata
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
#endif
		if (!dont_wait)
			sleep(mdm->looping_interval);
	}/*while*/
}






