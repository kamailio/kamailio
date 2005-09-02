/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2005-06-22  first version (bogdan)
 */


#include "../../usr_avp.h"
#include "../../dset.h"
#include "../../dprint.h"
#include "../../qvalue.h"
#include "../../parser/contact/parse_contact.h"
#include "../../qvalue.h"
#include "rd_filter.h"
#include "rd_funcs.h"


#define MAX_CONTACTS_PER_REPLY   16
#define DEFAULT_Q_VALUE          10

static int shmcontact2dset(struct sip_msg *req, struct sip_msg *shrpl,
			long max, char *reason);


int get_redirect( struct sip_msg *msg , int maxt, int maxb, char *reason)
{
	struct cell *t;
	str backup_uri;
	int max;
	int cts_added;
	int n;
	int i;

	/* get transaction */
	t = rd_tmb.t_gett();
	if (t==T_UNDEFINED || t==T_NULL_CELL)
	{
		LOG(LOG_CRIT,"BUG:uac_redirect:get_redirect: no current "
			"transaction found\n");
		goto error;
	}

	DBG("DEBUG:uac_redirect:get_redirect: resume branch=%d\n",
		t->first_branch);

	cts_added = 0; /* no contact added */
	backup_uri = msg->new_uri; /* shmcontact2dset will ater this value */

	/* look if there are any 3xx branches starting from resume_branch */
	for( i=t->first_branch ; i<t->nr_of_outgoings ; i++) {
		DBG("DEBUG:uac_redirect:get_redirect: checking branch=%d "
			"(added=%d)\n", i, cts_added);
		/* is a redirected branch? */
		if (t->uac[i].last_received<300 || t->uac[i].last_received>399)
			continue;
		DBG("DEBUG:uac_redirect:get_redirect: branch=%d is a redirect "
			"(added=%d)\n", i, cts_added);
		/* ok - we have a new redirected branch -> how many contacts can
		 * we get from it*/
		if (maxb==0) {
			max = maxt?(maxt-cts_added):(-1);
		} else {
			max = maxt?((maxt-cts_added>=maxb)?maxb:(maxt-cts_added)):maxb;
		}
		if (max==0)
			continue;
		/* get the contact from it */
		n = shmcontact2dset( msg, t->uac[i].reply, max, reason);
		if ( n<0 ) {
			LOG(L_ERR,"ERROR:uac_redirect:get_redirects: get contact from "
				"shm_reply branch %d failed\n",i);
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
static int sort_contacts(contact_t *ct_list, contact_t **ct_array)
{
	static qvalue_t q_array[MAX_CONTACTS_PER_REPLY];
	param_t *q_para;
	qvalue_t q;
	int n;
	int i,j;
	char backup;

	n = 0; /* number of sorted contacts */

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
				LOG(L_ERR, "ERROR:uac_redirect:sort_contacts: "
					"invalid q param\n");
				/* skip this contact */
				continue;
			}
		}
		DBG("DEBUG:uac_redirect:sort_contacts: <%.*s> q=%d\n",
				ct_list->uri.len,ct_list->uri.s,q);
		/*insert the contact into the sorted array */
		for(i=0;i<n;i++) {
			/* keep in mind that the contact list is reversts */
			if (q_array[i]<q)
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
	return n;
}



/* returns : -1 - error
 *            0 - ok, but no contact added
 *            n - ok and n contacts added
 */
static int shmcontact2dset(struct sip_msg *req, struct sip_msg *sh_rpl,
													long max, char *reason)
{
	static struct sip_msg  dup_rpl;
	static contact_t *scontacts[MAX_CONTACTS_PER_REPLY];
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

	if (sh_rpl==0 || sh_rpl==FAKED_REPLY)
		return 0;

	if (sh_rpl->contact==0) {
		/* contact header is not parsed */
		if ( sh_rpl->msg_flags&FL_SHM_CLONE ) {
			/* duplicate the reply into private memory to be able 
			 * to parse it and after words to free the parsed mems */
			memcpy( &dup_rpl, sh_rpl, sizeof(struct sip_msg) );
			dup = 2;
			/* ok -> force the parsing of contact header */
			if ( parse_headers( &dup_rpl, HDR_CONTACT_T, 0)<0 ) {
				LOG(L_ERR,"ERROR:uac_redirect:shmcontact2dset: dup_rpl "
					"parse failed\n");
				ret = -1;
				goto restore;
			}
			if (dup_rpl.contact==0) {
				DBG("DEBUG:uac_redirect:shmcontact2dset: contact hdr not "
					"found in dup_rpl\n");
				goto restore;
			}
			contact_hdr = dup_rpl.contact;
		} else {
			dup = 3;
			/* force the parsing of contact header */
			if ( parse_headers( sh_rpl, HDR_CONTACT_T, 0)<0 ) {
				LOG(L_ERR,"ERROR:uac_redirect:shmcontact2dset: sh_rpl "
					"parse failed\n");
				ret = -1;
				goto restore;
			}
			if (sh_rpl->contact==0) {
				DBG("DEBUG:uac_redirect:shmcontact2dset: contact hdr not "
					"found in sh_rpl\n");
				goto restore;
			}
			contact_hdr = sh_rpl->contact;
		}
	} else {
		contact_hdr = sh_rpl->contact;
	}

	/* parse the body of contact header */
	if (contact_hdr->parsed==0) {
		if ( parse_contact(contact_hdr)<0 ) {
			LOG(L_ERR,"ERROR:uac_redirect:shmcontact2dset: contact hdr "
				"parse failed\n");
			ret = -1;
			goto restore;
		}
		if (dup==0)
			dup = 1;
	}


	/* we have the contact header and its body parsed -> sort the contacts
	 * based on the q value */
	contacts = ((contact_body_t*)contact_hdr->parsed)->contacts;
	if (contacts==0) {
		DBG("DEBUG:uac_redirect:shmcontact2dset: contact hdr "
			"has no contacts\n");
		goto restore;
	}
	n = sort_contacts( contacts, scontacts);

	/* to many branches ? */
	if (max!=-1 && n>max)
		n = max;

	added = 0;

	/* add the sortet contacts as branches in dset and log this! */
	for ( i=0 ; i<n ; i++ ) {
		DBG("DEBUG:uac_redirect:shmcontact2dset: adding contact <%.*s>\n",
			scontacts[i]->uri.len, scontacts[i]->uri.s);
		if (append_branch( 0, &scontacts[i]->uri, 0, Q_UNSPECIFIED, 0, 0)<0 ) {
			LOG(L_ERR,"ERROR:uac_redirect:shmcontact2dset: failed to add "
				"contact to dset\n");
		} else {
			added++;
			if (rd_acc_fct!=0 && reason) {
				/* log the redirect */
				req->new_uri =  scontacts[i]->uri;
				rd_acc_fct( req, reason, acc_db_table);
			}
		}
	}

	ret = (added==0)?-1:added;
restore:
	if (dup==1) {
		free_contact( (contact_body_t**)(&contact_hdr->parsed) );
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

