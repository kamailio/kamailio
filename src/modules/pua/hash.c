/*
 * pua module - presence user agent module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
#include <stdlib.h>
#include <string.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../hashes.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "hash.h" 
#include "pua.h"
#include "pua_db.h"
#include "send_publish.h"

void print_ua_pres(ua_pres_t* p)
{
	LM_DBG("\tpres_uri= %.*s   len= %d\n", p->pres_uri->len, p->pres_uri->s, p->pres_uri->len);
	if(p->watcher_uri)
	{	
		LM_DBG("\twatcher_uri= %.*s  len= %d\n", p->watcher_uri->len, p->watcher_uri->s, p->watcher_uri->len);
		LM_DBG("\tcall_id= %.*s   len= %d\n", p->call_id.len, p->call_id.s, p->call_id.len);
		LM_DBG("\tfrom_tag= %.*s   len= %d\n", p->from_tag.len, p->from_tag.s, p->from_tag.len);
		LM_DBG("\tto_tag= %.*s  len= %d\n", p->to_tag.len, p->to_tag.s, p->to_tag.len);
		LM_DBG("\tflag= %d\n", p->flag);
		LM_DBG("\tevent= %d\n", p->event);
	}	
	else
	{
		LM_DBG("\tetag= %.*s - len= %d\n", p->etag.len, p->etag.s, p->etag.len);
		if(p->id.s)
			LM_DBG("\tid= %.*s\n", p->id.len, p->id.s);
	}
	LM_DBG("\texpires= %d\n", p->expires- (int)time(NULL));
}

htable_t* new_htable(void)
{
	htable_t* H= NULL;
	int i= 0, j;
	H= (htable_t*)shm_malloc(sizeof(htable_t));

	if (dbmode==PUA_DB_ONLY)
	{
		LM_ERR( "new_htable shouldn't be called in PUA_DB_ONLY mode\n" );
		return(NULL);
	}

	if(H== NULL)
	{
		LM_ERR("No more memory\n");
		return NULL;
	}
	memset(H, 0, sizeof(htable_t));

	H->p_records= (hash_entry_t*)shm_malloc(HASH_SIZE* sizeof(hash_entry_t));
	if(H->p_records== NULL)
	{
		LM_ERR("No more share memory\n");
		goto error;		
	}

	for(i=0; i<HASH_SIZE; i++)
	{
		if(lock_init(&H->p_records[i].lock)== 0)
		{
			LM_CRIT("initializing lock [%d]\n", i);
			goto error;
		}
		H->p_records[i].entity= (ua_pres_t*)shm_malloc(sizeof(ua_pres_t));
		if(H->p_records[i].entity== NULL)
		{
			LM_ERR("No more share memory\n");
			goto error;		
		}	
		H->p_records[i].entity->next= NULL;
	}
	return H;

error:

	if(H->p_records)
	{
		for(j=0; j< i; j++)
		{
			if(H->p_records[j].entity)
				shm_free(H->p_records[j].entity);
			lock_destroy(&H->p_records[j].lock);

		}
		shm_free(H->p_records);
	}
	shm_free(H);
	return NULL;

}

ua_pres_t* search_htable(ua_pres_t* pres, unsigned int hash_code)
{
	ua_pres_t* p= NULL,* L= NULL;
	L= HashT->p_records[hash_code].entity;

	if (dbmode==PUA_DB_ONLY)
	{
		LM_ERR( "search_htable shouldn't be called in PUA_DB_ONLY mode\n" );
		return(NULL);
	}

	LM_DBG("core_hash= %u\n", hash_code);

	for(p= L->next; p; p=p->next)
	{
		if((p->flag & pres->flag) && (p->event & pres->event))
		{
			if((p->pres_uri->len==pres->pres_uri->len) &&
					(strncmp(p->pres_uri->s, pres->pres_uri->s,pres->pres_uri->len)==0))
			{
				if(pres->id.s && pres->id.len) 
				{	
					if(!(pres->id.len== p->id.len &&
						strncmp(p->id.s, pres->id.s,pres->id.len)==0))
							continue;
				}				

				if(pres->watcher_uri)
				{
					if(p->watcher_uri->len==pres->watcher_uri->len &&
						(strncmp(p->watcher_uri->s, pres->watcher_uri->s,
								  pres->watcher_uri->len )==0))
					{
						if (check_remote_contact == 0)
							break;

						if(pres->remote_contact.s)
							if(pres->remote_contact.len== p->remote_contact.len &&
								strncmp(pres->remote_contact.s, p->remote_contact.s,
										 p->remote_contact.len)== 0)
								break;
					}
				}
				else
				{
					if(pres->etag.s)
					{
						if(pres->etag.len== p->etag.len &&
							strncmp(p->etag.s, pres->etag.s,pres->etag.len)==0)
							break;		
					}
					else
					{
						LM_DBG("no etag restriction\n");
						break;
					}
				}
			}
		}
	}

	if(p)
		LM_DBG("found record\n");
	else
		LM_DBG("record not found\n");

	return p;
}

void update_htable(ua_pres_t* p, time_t desired_expires, int expires,
		str* etag, unsigned int hash_code, str* contact)
{

	if (dbmode==PUA_DB_ONLY)
	{
		LM_ERR( "update_htable shouldn't be called in PUA_DB_ONLY mode\n" );
		return;
	}

	if(etag)
	{	
		shm_free(p->etag.s);
		p->etag.s= (char*)shm_malloc(etag->len);
		memcpy(p->etag.s, etag->s, etag->len);
		p->etag.len= etag->len;
	}

	p->expires= expires+ (int)time(NULL);
	p->desired_expires= desired_expires;
		
	if(p->db_flag & NO_UPDATEDB_FLAG)
		p->db_flag= UPDATEDB_FLAG;

	if(p->watcher_uri)
		p->cseq++;

	if(contact)
	{
		if(!(p->remote_contact.len== contact->len && 
				strncmp(p->remote_contact.s, contact->s, contact->len)==0))
		{
			/* update remote contact */
			shm_free(p->remote_contact.s);
			p->remote_contact.s= (char*)shm_malloc(contact->len* sizeof(char));
			if(p->remote_contact.s== NULL)
			{
				LM_ERR("no more shared memory\n");
				return;
			}
			memcpy(p->remote_contact.s, contact->s, contact->len);
			p->remote_contact.len= contact->len;
		}
	}
}
/* insert in front; so when searching the most recent result is returned*/
void insert_htable(ua_pres_t* presentity, unsigned int hash_code)
{
	ua_pres_t* p= NULL;

	if (dbmode==PUA_DB_ONLY)
	{
		LM_ERR( "insert_htable shouldn't be called in PUA_DB_ONLY mode\n" );
		return;
	}

	p= HashT->p_records[hash_code].entity;

	presentity->db_flag= INSERTDB_FLAG;
	presentity->next= p->next;
	
	p->next= presentity;
}

/* This function used to perform a search to find the hash table
   entry that matches the presentity it is passed.  However,
   everywhere it is used it is passed a pointer to the correct
   hash table entry already...  so let's just delete that */
void delete_htable(ua_pres_t* presentity, unsigned int hash_code)
{ 
	ua_pres_t *q = NULL;

	if (dbmode==PUA_DB_ONLY)
	{
		LM_ERR( "delete_htable shouldn't be called in PUA_DB_ONLY mode\n" );
		return;
	}

	if (presentity == NULL)
		return;

	q = HashT->p_records[hash_code].entity;

	while (q->next != presentity)
		q = q->next;
	q->next = presentity->next;
	
	if(presentity->etag.s)
		shm_free(presentity->etag.s);
	else
		if(presentity->remote_contact.s)
			shm_free(presentity->remote_contact.s);

	shm_free(presentity);
	presentity = NULL;

}
	
void destroy_htable(void)
{
	ua_pres_t* p= NULL,*q= NULL;
	int i;

	if (dbmode==PUA_DB_ONLY)
	{
		LM_ERR( "destroy_htable shouldn't be called in PUA_DB_ONLY mode\n" );
		return;
	}

	for(i=0; i<HASH_SIZE; i++)
	{	
		lock_destroy(&HashT->p_records[i].lock);
		p=HashT->p_records[i].entity;
		while(p->next)
		{
			q=p->next;
			p->next=q->next;
			if(q->etag.s)
				shm_free(q->etag.s);
			else
				if(q->remote_contact.s)
					shm_free(q->remote_contact.s);

			shm_free(q);
			q= NULL;
		}
		shm_free(p);
	}
    shm_free(HashT->p_records);
	shm_free(HashT);
  
  return;
}

int convert_temporary_dialog(ua_pres_t *dialog)
{
	ua_pres_t *temp_dialog;
	unsigned int hash_code;

	hash_code= core_hash(dialog->pres_uri,dialog->watcher_uri, HASH_SIZE); 
	lock_get(&HashT->p_records[hash_code].lock);

	temp_dialog = get_temporary_dialog(dialog, hash_code);
	if (temp_dialog)
		delete_htable(temp_dialog, hash_code);
	else {
		lock_release(&HashT->p_records[hash_code].lock);
		return -1;
	}

	insert_htable(dialog, hash_code);

	lock_release(&HashT->p_records[hash_code].lock);

	return 1;
}

/* must lock the record line before calling this function*/
ua_pres_t* get_dialog(ua_pres_t* dialog, unsigned int hash_code)
{
	ua_pres_t* p= NULL, *L;

	if (dbmode==PUA_DB_ONLY)
	{
		LM_ERR( "get_dialog shouldn't be called in PUA_DB_ONLY mode\n" );
		return(NULL);
	}

	if (dialog->to_tag.len == 0 || dialog->to_tag.s == NULL)
		return(NULL);

	LM_DBG("core_hash= %u\n", hash_code);

	L= HashT->p_records[hash_code].entity;
	for(p= L->next; p; p=p->next)
	{

		if(p->flag& dialog->flag)
		{
			LM_DBG("pres_uri= %.*s\twatcher_uri=%.*s\n\t"
					"callid= %.*s\tto_tag= %.*s\tfrom_tag= %.*s\n",
				p->pres_uri->len, p->pres_uri->s, p->watcher_uri->len,
				p->watcher_uri->s,p->call_id.len, p->call_id.s,
				p->to_tag.len, p->to_tag.s, p->from_tag.len, p->from_tag.s);

			LM_DBG("searched to_tag= %.*s\tfrom_tag= %.*s\n",
				 p->to_tag.len, p->to_tag.s, p->from_tag.len, p->from_tag.s);
	    
			if( (strncmp(p->call_id.s, dialog->call_id.s, p->call_id.len)== 0) &&
				p->to_tag.len > 0 &&
				(strncmp(p->to_tag.s, dialog->to_tag.s, p->to_tag.len)== 0) &&
				(strncmp(p->from_tag.s, dialog->from_tag.s, p->from_tag.len)== 0) )
				{	
					LM_DBG("FOUND dialog\n");
					break;
				}
		}	
	
	}
		
	return p;
}

/* must lock the record line before calling this function*/
ua_pres_t* get_temporary_dialog(ua_pres_t* dialog, unsigned int hash_code)
{
	ua_pres_t* p= NULL, *L;
	LM_DBG("core_hash= %u\n", hash_code);

	L= HashT->p_records[hash_code].entity;
	for(p= L->next; p; p=p->next)
	{
		LM_DBG("pres_uri= %.*s\twatcher_uri=%.*s\n\t"
				"callid= %.*s\tfrom_tag= %.*s\n",
			p->pres_uri->len, p->pres_uri->s, p->watcher_uri->len,
			p->watcher_uri->s,p->call_id.len, p->call_id.s,
			p->from_tag.len, p->from_tag.s);

		if((p->call_id.len == dialog->call_id.len) &&
			(strncmp(p->call_id.s, dialog->call_id.s, p->call_id.len)== 0) &&
			(p->from_tag.len == dialog->from_tag.len) &&
			(strncmp(p->from_tag.s, dialog->from_tag.s, p->from_tag.len)== 0))
		{
			LM_DBG("FOUND temporary dialog\n");
			break;
		}
	}

	return p;
}

int get_record_id(ua_pres_t* dialog, str** rec_id)
{
	unsigned int hash_code;
	ua_pres_t* rec;
	str* id;

	if (dbmode==PUA_DB_ONLY)
	{
		return( get_record_id_puadb( dialog, rec_id ) );
	}

	*rec_id= NULL;

	hash_code= core_hash(dialog->pres_uri, dialog->watcher_uri, HASH_SIZE);
	lock_get(&HashT->p_records[hash_code].lock);

	rec= get_dialog(dialog, hash_code);
	if(rec== NULL)
	{
		LM_DBG("Record not found - looking for temporary\n");
		rec = get_temporary_dialog(dialog, hash_code);
		if (rec == NULL)
		{
			LM_DBG("Temporary record not found\n");
			lock_release(&HashT->p_records[hash_code].lock);
			return 0;
		}
	}
	id= (str*)pkg_malloc(sizeof(str));
	if(id== NULL)
	{
		LM_ERR("No more memory\n");
		lock_release(&HashT->p_records[hash_code].lock);
		return -1;
	}
	id->s= (char*)pkg_malloc(rec->id.len* sizeof(char));
	if(id->s== NULL)
	{
		LM_ERR("No more memory\n");
		pkg_free(id);
		lock_release(&HashT->p_records[hash_code].lock);
		return -1;
	}
	memcpy(id->s, rec->id.s, rec->id.len);
	id->len= rec->id.len;

	lock_release(&HashT->p_records[hash_code].lock);

	LM_DBG("rec did= %.*s\n", id->len, id->s);

	*rec_id= id;

	return 0;
}

/**
 * return -1 on not found, 0 on established dialog, 1 on temporary dialog
 */
int is_dialog(ua_pres_t* dialog)
{
	int ret_code= 0;
	unsigned int hash_code;

	if (dbmode==PUA_DB_ONLY)
	{
		return( is_dialog_puadb(dialog) );
	}	

	hash_code= core_hash(dialog->pres_uri, dialog->watcher_uri, HASH_SIZE);
	lock_get(&HashT->p_records[hash_code].lock);

	if(get_dialog(dialog, hash_code)==NULL)
	{
		if(get_temporary_dialog(dialog, hash_code)==NULL)
			ret_code= -1;
		else
			ret_code= 1;
	} else {
		ret_code= 0;
	}
	lock_release(&HashT->p_records[hash_code].lock);
	
	return ret_code;

}

int update_contact(struct sip_msg* msg, char* str1, char* str2)
{
	ua_pres_t* p, hentity;
	str contact;
	struct to_body *pto = NULL, TO = {0}, *pfrom = NULL;
	unsigned int hash_code;

	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
	{
		LM_ERR("when parsing headers\n");
		return -1;
	}

	/* find the record */
	if( msg->callid==NULL || msg->callid->body.s==NULL)
	{
		LM_ERR("cannot parse callid header\n");
		return -1;
	}		
	
	if (!msg->from || !msg->from->body.s)
	{
		LM_ERR("cannot find 'from' header!\n");
		return -1;
	}
	if (msg->from->parsed == NULL)
	{
		if ( parse_from_header( msg )<0 ) 
		{
			LM_ERR("cannot parse From header\n");
			return -1;
		}
	}
	
	pfrom = (struct to_body*)msg->from->parsed;
	
	if( pfrom->tag_value.s ==NULL || pfrom->tag_value.len == 0)
	{
		LM_ERR("no from tag value present\n");
		return -1;
	}		
	
	if( msg->to==NULL || msg->to->body.s==NULL)
	{
		LM_ERR("cannot parse TO header\n");
		return -1;
	}			
	
	if(msg->to->parsed != NULL)
	{
		pto = (struct to_body*)msg->to->parsed;
		LM_DBG("'To' header ALREADY PARSED: <%.*s>\n",pto->uri.len,pto->uri.s);
	}
	else
	{
		parse_to(msg->to->body.s,msg->to->body.s +
			msg->to->body.len + 1, &TO);
		if(TO.uri.len <= 0) 
		{
			LM_DBG("'To' header NOT parsed\n");
			goto error;
		}
		pto = &TO;
	}			
	if( pto->tag_value.s ==NULL || pto->tag_value.len == 0)
	{
		LM_ERR("no from tag value present\n");
		goto error;
	}
	hentity.watcher_uri= &pto->uri;
	hentity.pres_uri= &pfrom->uri; 
	hentity.call_id=  msg->callid->body;
	hentity.to_tag= pto->tag_value;
	hentity.from_tag= pfrom->tag_value;
	

	/* extract the contact */
	if(msg->contact== NULL || msg->contact->body.s== NULL)
	{
		LM_ERR("no contact header found in 200 OK reply");
		goto error;
	}
	contact= msg->contact->body;

	if (dbmode==PUA_DB_ONLY)
	{
		update_contact_puadb(&hentity, &contact );
		free_to_params(&TO);
		return(1);
	}

	hash_code= core_hash(hentity.pres_uri,hentity.watcher_uri,
				HASH_SIZE);

	lock_get(&HashT->p_records[hash_code].lock);

	p= get_dialog(&hentity, hash_code);
	if(p== NULL)
	{
		lock_release(&HashT->p_records[hash_code].lock);
		LM_ERR("no record for the dialog found in hash table\n");
		goto error;
	}

	shm_free(p->remote_contact.s);

	if(!(p->remote_contact.len== contact.len && 
				strncmp(p->remote_contact.s, contact.s, contact.len)==0))
	{
		/* update remote contact */
		shm_free(p->remote_contact.s);
		p->remote_contact.s= (char*)shm_malloc(contact.len* sizeof(char));
		if(p->remote_contact.s== NULL)
		{
			LM_ERR("no more shared memory\n");
			lock_release(&HashT->p_records[hash_code].lock);
			goto error;
		}
		memcpy(p->remote_contact.s, contact.s, contact.len);
		p->remote_contact.len= contact.len;
	}

	lock_release(&HashT->p_records[hash_code].lock);

	free_to_params(&TO);
	return 1;

error:
	free_to_params(&TO);
	return -1;
}

list_entry_t *get_subs_list(str *did)
{
	int i;
	str *tmp_str;
	list_entry_t *list = NULL;

	if (dbmode==PUA_DB_ONLY)
		return get_subs_list_puadb(did);

	for (i = 0; i < HASH_SIZE; i++)
	{
		ua_pres_t *dialog;

		lock_get(&HashT->p_records[i].lock);
		dialog = HashT->p_records[i].entity;
		while (dialog != NULL)
		{
			if (dialog->id.s != NULL && dialog->id.len > 0 &&
				strncmp(dialog->id.s, did->s, did->len) == 0 &&
				dialog->pres_uri != NULL && dialog->pres_uri->s != NULL &&
				dialog->pres_uri->len > 0)
			{
				if ((tmp_str = (str *)pkg_malloc(sizeof(str))) == NULL)
				{
					LM_ERR("out of private memory\n");
					lock_release(&HashT->p_records[i].lock);
					goto done;
				}
				if ((tmp_str->s = (char *)pkg_malloc(sizeof(char) * dialog->pres_uri->len + 1)) == NULL)
				{
					pkg_free(tmp_str);
					LM_ERR("out of private memory\n");
					lock_release(&HashT->p_records[i].lock);
					goto done;
				}
				memcpy(tmp_str->s, dialog->pres_uri->s, dialog->pres_uri->len);
				tmp_str->len = dialog->pres_uri->len;
				tmp_str->s[tmp_str->len] = '\0';

				list = list_insert(tmp_str, list, NULL);
			}
			dialog = dialog->next;
		}
		lock_release(&HashT->p_records[i].lock);
	}
done:
	return list;
}
