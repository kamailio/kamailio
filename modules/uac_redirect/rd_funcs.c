/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 */


#include "../../usr_avp.h"
#include "../../dset.h"
#include "../../dprint.h"
#include "../../qvalue.h"
#include "../../parser/contact/parse_contact.h"
#include "../../lib/srutils/sruid.h"
#include "../../qvalue.h"
#include "rd_filter.h"
#include "rd_funcs.h"


extern sruid_t _redirect_sruid;

#define MAX_CONTACTS_PER_REPLY   16
#define DEFAULT_Q_VALUE          10

static int shmcontact2dset(struct sip_msg *req, struct sip_msg *shrpl,
		long max, struct acc_param *reason, unsigned int bflags);


int get_redirect( struct sip_msg *msg , int maxt, int maxb,
									struct acc_param *reason, unsigned int bflags)
{
	struct cell *t;
	str backup_uri;
	int max;
	int cts_added;
	int n;
	int i;
	int first_branch;
	char code_buf[INT2STR_MAX_LEN];

	/* get transaction */
	t = rd_tmb.t_gett();
	if (t==T_UNDEFINED || t==T_NULL_CELL)
	{
		LM_CRIT("no current transaction found\n");
		goto error;
	}
	for(first_branch=t->nr_of_outgoings-1; first_branch>=0; first_branch--)
		if(t->uac[first_branch].flags&TM_UAC_FLAG_FB)
			break;
	if(first_branch<0)
	{
		LM_CRIT("no current first branch found\n");
		goto error;
	}

	LM_DBG("resume branch=%d\n", first_branch);

	cts_added = 0; /* no contact added */
	backup_uri = msg->new_uri; /* shmcontact2dset will ater this value */

	/* look if there are any 3xx branches starting from resume_branch */
	for( i=first_branch ; i<t->nr_of_outgoings ; i++) {
		LM_DBG("checking branch=%d (added=%d)\n", i, cts_added);
		/* is a redirected branch? */
		if (t->uac[i].last_received<300 || t->uac[i].last_received>399)
			continue;
		LM_DBG("branch=%d is a redirect (added=%d)\n", i, cts_added);
		/* ok - we have a new redirected branch -> how many contacts can
		 * we get from it*/
		if (maxb==0) {
			max = maxt?(maxt-cts_added):(-1);
		} else {
			max = maxt?((maxt-cts_added>=maxb)?maxb:(maxt-cts_added)):maxb;
		}
		if (max==0)
			continue;
		if(reason!=NULL)
		{
			/* put the response code into the acc_param reason struct */
			reason->code = t->uac[i].last_received;
			reason->code_s.s = int2bstr((unsigned long)reason->code, code_buf, &reason->code_s.len);
		}
		/* get the contact from it */
		n = shmcontact2dset( msg, t->uac[i].reply, max, reason, bflags);
		if ( n<0 ) {
			LM_ERR("get contact from shm_reply branch %d failed\n",i);
			/* do not go to error, try next branches */
		} else {
			/* count the added contacts */
			cts_added += n;
		}
	}

	/* restore original new_uri */
	msg->new_uri = backup_uri;

	/* return false if no contact was appended */
	return (cts_added>0)?1:-1;
error:
	return -1;
}



/* returns the number of contacts put in the sorted array */
static int sort_contacts(hdr_field_t *chdr, contact_t **ct_array,
														qvalue_t *q_array)
{
	param_t *q_para;
	qvalue_t q;
	int n;
	int i,j;
	char backup;
	contact_t *ct_list;
	hdr_field_t *hdr;

	n = 0; /* number of sorted contacts */

	for(hdr=chdr; hdr; hdr=hdr->next) {
		if(hdr->type != HDR_CONTACT_T) continue;
		ct_list = ((contact_body_t*)hdr->parsed)->contacts;
		for( ; ct_list ; ct_list = ct_list->next ) {
			/* check the filters first */
			backup = ct_list->uri.s[ct_list->uri.len];
			ct_list->uri.s[ct_list->uri.len] = 0;
			if ( run_filters( ct_list->uri.s )==-1 ){
				ct_list->uri.s[ct_list->uri.len] = backup;
				continue;
			}
			ct_list->uri.s[ct_list->uri.len] = backup;
			/* does the contact has a q val? */
			q_para = ct_list->q;
			if (q_para==0 || q_para->body.len==0) {
				q = DEFAULT_Q_VALUE;
			} else {
				if (str2q( &q, q_para->body.s, q_para->body.len)!=0) {
					LM_ERR("invalid q param\n");
					/* skip this contact */
					continue;
				}
			}
			LM_DBG("sort_contacts: <%.*s> q=%d\n",
					ct_list->uri.len,ct_list->uri.s,q);
			/*insert the contact into the sorted array */
			for(i=0;i<n;i++) {
				/* keep in mind that the contact list is reversed */
				if (q_array[i]<=q)
					continue;
				break;
			}
			if (i!=MAX_CONTACTS_PER_REPLY) {
				/* insert the contact at this position */
				for( j=n-1-1*(n==MAX_CONTACTS_PER_REPLY) ; j>=i ; j-- ) {
					ct_array[j+1] = ct_array[j];
					q_array[j+1] = q_array[j];
				}
				ct_array[j+1] = ct_list;
				q_array[j+1] = q;
				if (n!=MAX_CONTACTS_PER_REPLY)
					n++;
			}
		}
	}
	return n;
}



/* returns : -1 - error
 *            0 - ok, but no contact added
 *            n - ok and n contacts added
 */
static int shmcontact2dset(struct sip_msg *req, struct sip_msg *sh_rpl,
								long max, struct acc_param *reason, unsigned int bflags)
{
	static struct sip_msg  dup_rpl;
	static contact_t *scontacts[MAX_CONTACTS_PER_REPLY];
	static qvalue_t  sqvalues[MAX_CONTACTS_PER_REPLY];
	struct hdr_field *hdr;
	struct hdr_field *contact_hdr;
	contact_t        *contacts;
	int n,i;
	int added;
	int dup;
	int ret;

	/* dup can be:
	 *    0 - sh reply but nothing duplicated 
	 *    1 - sh reply but only contact body parsed
	 *    2 - sh reply and contact header and body parsed
	 *    3 - private reply
	 */
	dup = 0; /* sh_rpl not duplicated */
	ret = 0; /* success and no contact added */
	contact_hdr = 0;

	if (sh_rpl==0 || sh_rpl==FAKED_REPLY)
		return 0;

	if (sh_rpl->contact==0) {
		/* contact header is not parsed */
		if ( sh_rpl->msg_flags&FL_SHM_CLONE ) {
			/* duplicate the reply into private memory to be able 
			 * to parse it and afterwards to free the parsed mems */
			memcpy( &dup_rpl, sh_rpl, sizeof(struct sip_msg) );
			dup = 2;
			/* ok -> force the parsing of contact header */
			if ( parse_headers( &dup_rpl, HDR_EOH_F, 0)<0 ) {
				LM_ERR("dup_rpl parse failed\n");
				ret = -1;
				goto restore;
			}
			if (dup_rpl.contact==0) {
				LM_DBG("contact hdr not found in dup_rpl\n");
				goto restore;
			}
			contact_hdr = dup_rpl.contact;
		} else {
			dup = 3;
			/* force the parsing of contact header */
			if ( parse_headers( sh_rpl, HDR_EOH_F, 0)<0 ) {
				LM_ERR("sh_rpl parse failed\n");
				ret = -1;
				goto restore;
			}
			if (sh_rpl->contact==0) {
				LM_DBG("contact hdr not found in sh_rpl\n");
				goto restore;
			}
			contact_hdr = sh_rpl->contact;
		}
	} else {
		contact_hdr = sh_rpl->contact;
	}

	/* parse the body of contact headers */
	hdr = contact_hdr;
	while(hdr) {
		if (hdr->type == HDR_CONTACT_T) {
			if (hdr->parsed==0) {
				if(parse_contact(hdr) < 0) {
					LM_ERR("failed to parse Contact body\n");
					ret = -1;
					goto restore;
				}
				if (dup==0)
					dup = 1;
				}
		}
		hdr = hdr->next;
	}

	/* we have the contact header and its body parsed -> sort the contacts
	 * based on the q value */
	contacts = ((contact_body_t*)contact_hdr->parsed)->contacts;
	if (contacts==0) {
		LM_DBG("contact hdr has no contacts\n");
		goto restore;
	}
	n = sort_contacts(contact_hdr, scontacts, sqvalues);
	if (n==0) {
		LM_DBG("no contacts left after filtering\n");
		goto restore;
	}

	i=0;

	/* more branches than requested in the parameter
	 * - add only the last ones from sorted array,
	 *   because the order is by increasing q */
	if (max!=-1 && n>max)
		i = n - max;

	added = 0;

	/* add the sortet contacts as branches in dset and log this! */
	for (  ; i<n ; i++ ) {
		LM_DBG("adding contact <%.*s>\n", scontacts[i]->uri.len,
				scontacts[i]->uri.s);
		if(sruid_next(&_redirect_sruid)==0) {
			if(append_branch( 0, &scontacts[i]->uri, 0, 0, sqvalues[i],
						bflags, 0, &_redirect_sruid.uid, 0,
						&_redirect_sruid.uid, &_redirect_sruid.uid)<0) {
				LM_ERR("failed to add contact to dset\n");
			} else {
				added++;
				if (rd_acc_fct!=0 && reason) {
					/* log the redirect */
					req->new_uri =  scontacts[i]->uri;
					//FIXME
					rd_acc_fct( req, (char*)reason, acc_db_table);
				}
			}
		} else {
			LM_ERR("failed to generate ruid for a new branch\n");
		}
	}

	ret = (added==0)?-1:added;
restore:
	if (dup==1) {
		free_contact( (contact_body_t**)(void*)(&contact_hdr->parsed) );
	} else if (dup==2) {
		/* are any new headers found? */
		if (dup_rpl.last_header!=sh_rpl->last_header) {
			/* identify in the new headere list (from dup_rpl) 
			 * the sh_rpl->last_header and start remove everything after */
			hdr = sh_rpl->last_header;
			free_hdr_field_lst(hdr->next);
			hdr->next=0;
		}
	}
	return ret;

}

