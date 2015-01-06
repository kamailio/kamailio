/*
 * Copyright (C) 2008 Voice System SRL
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


/*!
 * \file
 * \brief Profile related functions for the dialog module
 * \ingroup dialog
 * Module: \ref dialog
 */


#include "../../mem/shm_mem.h"
#include "../../hashes.h"
#include "../../trim.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../route.h"
#include "../../modules/tm/tm_load.h"
#include "../../lib/srutils/sruid.h"
#include "dlg_hash.h"
#include "dlg_var.h"
#include "dlg_handlers.h"
#include "dlg_profile.h"


/*! size of dialog profile hash */
#define PROFILE_HASH_SIZE 16

/*! tm bindings */
extern struct tm_binds d_tmb;

/*! global dialog message id */
static unsigned int       current_dlg_msg_id  = 0 ;
static unsigned int       current_dlg_msg_pid = 0 ;

/*! pending dialog links */
static dlg_profile_link_t *current_pending_linkers = NULL;

/*! global dialog profile list */
static dlg_profile_table_t *profiles = NULL;


static dlg_profile_table_t* new_dlg_profile( str *name,
		unsigned int size, unsigned int has_value);

extern int update_dlg_timeout(dlg_cell_t *, int);

static sruid_t _dlg_profile_sruid;

/*!
 * \brief Add profile definitions to the global list
 * \see new_dlg_profile
 * \param profiles profile name
 * \param has_value set to 0 for a profile without value, otherwise it has a value
 * \return 0 on success, -1 on failure
 */
int add_profile_definitions( char* profiles, unsigned int has_value)
{
	char *p;
	char *d;
	str name;
	unsigned int i;

	if (profiles==NULL || strlen(profiles)==0 )
		return 0;

	p = profiles;
	do {
		/* locate name of profile */
		name.s = p;
		d = strchr( p, ';');
		if (d) {
			name.len = d-p;
			d++;
		} else {
			name.len = strlen(p);
		}

		/* we have the name -> trim it for spaces */
		trim_spaces_lr( name );

		/* check len name */
		if (name.len==0)
			/* ignore */
			continue;

		/* check the name format */
		for(i=0;i<name.len;i++) {
			if ( !isalnum(name.s[i]) && name.s[i] != '_' ) {
				LM_ERR("bad profile name <%.*s>, char %c - use only "
					"alphanumerical characters or '_'\n", name.len,name.s,name.s[i]);
				return -1;
			}
		}

		/* name ok -> create the profile */
		LM_DBG("creating profile <%.*s>\n",name.len,name.s);

		if (new_dlg_profile( &name, PROFILE_HASH_SIZE, has_value)==NULL) {
			LM_ERR("failed to create new profile <%.*s>\n",name.len,name.s);
			return -1;
		}

	}while( (p=d)!=NULL );

	return 0;
}


/*!
 * \brief Search a dialog profile in the global list
 * \note Linear search, this won't have the best performance for huge profile lists
 * \param name searched dialog profile
 * \return pointer to the profile on success, NULL otherwise
 */
struct dlg_profile_table* search_dlg_profile(str *name)
{
	struct dlg_profile_table *profile;

	for( profile=profiles ; profile ; profile=profile->next ) {
		if (name->len==profile->name.len &&
		memcmp(name->s,profile->name.s,name->len)==0 )
			return profile;
	}
	return NULL;
}


/*!
 * \brief Creates a new dialog profile
 * \see add_profile_definitions
 * \param name profile name
 * \param size profile size
 * \param has_value set to 0 for a profile without value, otherwise it has a value
 * \return pointer to the created dialog on success, NULL otherwise
 */
static struct dlg_profile_table* new_dlg_profile( str *name, unsigned int size,
		unsigned int has_value)
{
	struct dlg_profile_table *profile;
	struct dlg_profile_table *ptmp;
	unsigned int len;
	unsigned int i;

	if ( name->s==NULL || name->len==0 || size==0 ) {
		LM_ERR("invalid parameters\n");
		return NULL;
	}

	for( len=0,i=0 ; i<8*sizeof(size) ; i++ ) {
		if ( size & (1<<i) ) len++;
	}
	if (len!=1) {
		LM_ERR(" size %u is not power of 2!\n", size);
		return NULL;
	}

	profile = search_dlg_profile(name);
	if (profile!=NULL) {
		LM_ERR("duplicate dialog profile registered <%.*s>\n",
			name->len, name->s);
		return NULL;
	}

	len = sizeof(struct dlg_profile_table) +
		size*sizeof(struct dlg_profile_entry) +
		name->len + 1;
	profile = (struct dlg_profile_table *)shm_malloc(len);
	if (profile==NULL) {
		LM_ERR("no more shm mem\n");
		return NULL;
	}

	memset( profile , 0 , len);
	profile->size = size;
	profile->has_value = (has_value==0)?0:1;

	/* init lock */
	if (lock_init( &profile->lock )==NULL) {
		LM_ERR("failed to init lock\n");
		shm_free(profile);
		return NULL;
	}

	/* set inner pointers */
	profile->entries = (struct dlg_profile_entry*)(profile + 1);
	profile->name.s = ((char*)profile->entries) + 
		size*sizeof(struct dlg_profile_entry);

	/* copy the name of the profile */
	memcpy( profile->name.s, name->s, name->len );
	profile->name.len = name->len;
	profile->name.s[profile->name.len] = 0;

	/* link profile */
	for( ptmp=profiles ; ptmp && ptmp->next; ptmp=ptmp->next );
	if (ptmp==NULL) {
		profiles = profile;
		sruid_init(&_dlg_profile_sruid, '-', "dlgp", SRUID_INC);
	} else {
		ptmp->next = profile;
	}

	return profile;
}


/*!
 * \brief Destroy a dialog profile list
 * \param profile dialog profile
 */
static void destroy_dlg_profile(struct dlg_profile_table *profile)
{
	if (profile==NULL)
		return;

	lock_destroy( &profile->lock );
	shm_free( profile );
	return;
}


/*!
 * \brief Destroy the global dialog profile list
 */
void destroy_dlg_profiles(void)
{
	struct dlg_profile_table *profile;

	while(profiles) {
		profile = profiles;
		profiles = profiles->next;
		destroy_dlg_profile( profile );
	}
	return;
}


/*!
 * \brief Destroy dialog linkers
 * \param linker dialog linker
 */ 
void destroy_linkers(struct dlg_profile_link *linker)
{
	struct dlg_profile_entry *p_entry;
	struct dlg_profile_link *l;
	struct dlg_profile_hash *lh;

	while(linker) {
		l = linker;
		linker = linker->next;
		/* unlink from profile table */
		if (l->hash_linker.next) {
			p_entry = &l->profile->entries[l->hash_linker.hash];
			lock_get( &l->profile->lock );
			lh = &l->hash_linker;
			/* last element on the list? */
			if (lh==lh->next) {
				p_entry->first = NULL;
			} else {
				if (p_entry->first==lh)
					p_entry->first = lh->next;
				lh->next->prev = lh->prev;
				lh->prev->next = lh->next;
			}
			lh->next = lh->prev = NULL;
			p_entry->content --;
			lock_release( &l->profile->lock );
		}
		/* free memory */
		shm_free(l);
	}
}


/*!
 * \brief Calculate the hash profile from a dialog
 * \see core_hash
 * \param value hash source
 * \param dlg dialog cell
 * \param profile dialog profile table (for hash size)
 * \return value hash if the value has a value, hash over dialog otherwise
 */
inline static unsigned int calc_hash_profile(str *value1, str *value2,
		dlg_profile_table_t *profile)
{
	if (profile->has_value) {
		/* do hash over the value1 */
		return core_hash( value1, NULL, profile->size);
	} else {
		/* do hash over the value2 */
		if(value2)
			return core_hash( value2, NULL, profile->size);
		return 0;
	}
}


/*!
 * \brief Remove remote profile items that are expired
 * \param te expiration time
 */
void remove_expired_remote_profiles(time_t te)
{
	struct dlg_profile_table *profile;
	struct dlg_profile_entry *p_entry;
	struct dlg_profile_hash *lh;
	struct dlg_profile_hash *kh;
	int i;

	for( profile=profiles ; profile ; profile=profile->next ) {
		if(profile->flags&FLAG_PROFILE_REMOTE) {
			for(i=0; i<profile->size; i++) {
				/* space for optimization */
				lock_get(&profile->lock);
				p_entry = &profile->entries[i];
				lh = p_entry->first;
				while(lh) {
					kh = lh->next;
					if(lh->dlg==NULL && lh->expires>0 && lh->expires<te) {
						/* last element on the list? */
						if (lh==lh->next) {
							p_entry->first = NULL;
						} else {
							if (p_entry->first==lh)
								p_entry->first = lh->next;
							lh->next->prev = lh->prev;
							lh->prev->next = lh->next;
						}
						lh->next = lh->prev = NULL;
						if(lh->linker) shm_free(lh->linker);
						p_entry->content--;
						lock_release(&profile->lock);
						return;
					}
					lh = kh;
				}
				lock_release(&profile->lock);
			}
		}
	}
}

/*!
 * \brief Remove profile
 * \param profile pointer to profile
 * \param value profile value
 * \param puid profile unique id
 */
int remove_profile(dlg_profile_table_t *profile, str *value, str *puid)
{
	unsigned int hash;
	struct dlg_profile_entry *p_entry;
	struct dlg_profile_hash *lh;

	hash = calc_hash_profile(value, puid, profile);
	lock_get(&profile->lock );
	p_entry = &profile->entries[hash];
	lh = p_entry->first;
	if(lh) {
		do {
			if(lh->dlg==NULL && lh->puid_len==puid->len
					&& lh->value.len==value->len
					&& strncmp(lh->puid, puid->s, puid->len)==0
					&& strncmp(lh->value.s, value->s, value->len)==0) {
				/* last element on the list? */
				if (lh==lh->next) {
					p_entry->first = NULL;
				} else {
					if (p_entry->first==lh)
						p_entry->first = lh->next;
					lh->next->prev = lh->prev;
					lh->prev->next = lh->next;
				}
				lh->next = lh->prev = NULL;
				if(lh->linker) shm_free(lh->linker);
				p_entry->content--;
				return 1;
			}
			lh = lh->next;
		} while(lh != p_entry->first);
	}
	lock_release(&profile->lock );
	return 0;
}


/*!
 * \brief Callback for cleanup of profile local vars
 * \param msg SIP message
 * \param flags unused
 * \param param unused
 * \return 1
 */
int cb_profile_reset( struct sip_msg *msg, unsigned int flags, void *param )
{
	current_dlg_msg_id = 0;
	current_dlg_msg_pid = 0;
	if (current_pending_linkers) {
		destroy_linkers(current_pending_linkers);
		current_pending_linkers = NULL;
	}

	/* need to return non-zero - 0 will break the exec of the request */
	return 1;
}


/*!
 * \brief Cleanup a profile
 * \param msg SIP message
 * \param flags unused
 * \param param unused
 * \return 1
 */
int profile_cleanup( struct sip_msg *msg, unsigned int flags, void *param )
{
	dlg_cell_t *dlg;

	current_dlg_msg_id = 0;
	current_dlg_msg_pid = 0;
	dlg = dlg_get_ctx_dialog();
	if (dlg!=NULL) {
		if(dlg->dflags & DLG_FLAG_TM) {
			dlg_unref(dlg, 1);
		} else {
			/* dialog didn't make it to tm */
			dlg_unref(dlg, 2);
		}
	}
	if (current_pending_linkers) {
		destroy_linkers(current_pending_linkers);
		current_pending_linkers = NULL;
	}

	/* need to return non-zero - 0 will break the exec of the request */
	return 1;
}


/*!
 * \brief Link a dialog profile
 * \param linker dialog linker
 * \param vkey key for profile hash table
 */
static void link_profile(struct dlg_profile_link *linker, str *vkey)
{
	unsigned int hash;
	struct dlg_profile_entry *p_entry;

	/* calculate the hash position */
	hash = calc_hash_profile(&linker->hash_linker.value, vkey, linker->profile);
	linker->hash_linker.hash = hash;

	/* insert into profile hash table */
	p_entry = &linker->profile->entries[hash];
	lock_get( &linker->profile->lock );
	if (p_entry->first) {
		linker->hash_linker.prev = p_entry->first->prev;
		linker->hash_linker.next = p_entry->first;
		p_entry->first->prev->next = &linker->hash_linker;
		p_entry->first->prev = &linker->hash_linker;
	} else {
		p_entry->first = linker->hash_linker.next 
			= linker->hash_linker.prev = &linker->hash_linker;
	}
	p_entry->content ++;
	lock_release( &linker->profile->lock );
}

/*!
 * \brief Link a dialog profile
 * \param linker dialog linker
 * \param dlg dialog cell
 */
static void link_dlg_profile(struct dlg_profile_link *linker, struct dlg_cell *dlg)
{
	struct dlg_entry *d_entry;

	/* add the linker to the dialog */
	/* FIXME zero h_id is not 100% for testing if the dialog is inserted
	 * into the hash table -> we need circular lists  -bogdan */
	if (dlg->h_id) {
		d_entry = &d_table->entries[dlg->h_entry];
		dlg_lock( d_table, d_entry);
		linker->next = dlg->profile_links;
		dlg->profile_links =linker;
		linker->hash_linker.dlg = dlg;
		dlg_unlock( d_table, d_entry);
	} else {
		linker->next = dlg->profile_links;
		dlg->profile_links =linker;
		linker->hash_linker.dlg = dlg;
	}

	link_profile(linker, &dlg->callid);
}


/*!
 * \brief Set the global variables to the current dialog
 * \param msg SIP message
 * \param dlg dialog cell
 */
void set_current_dialog(sip_msg_t *msg, dlg_cell_t *dlg)
{
	struct dlg_profile_link *linker;
	struct dlg_profile_link *tlinker;

	LM_DBG("setting current dialog [%u:%u]\n", dlg->h_entry, dlg->h_id);
	/* if linkers are not from current request, just discard them */
	if (msg->id!=current_dlg_msg_id || msg->pid!=current_dlg_msg_pid) {
		current_dlg_msg_id = msg->id;
		current_dlg_msg_pid = msg->pid;
		destroy_linkers(current_pending_linkers);
	} else {
		/* add the linker, one by one, to the dialog */
		linker = current_pending_linkers;
		while (linker) {
			tlinker = linker;
			linker = linker->next;
			/* process tlinker */
			tlinker->next = NULL;
			link_dlg_profile( tlinker, dlg);
		}
	}
	current_pending_linkers = NULL;
}


/*!
 * \brief Set a dialog profile
 * \param msg SIP message
 * \param value value
 * \param profile dialog profile table
 * \return 0 on success, -1 on failure
 */
int set_dlg_profile(struct sip_msg *msg, str *value, struct dlg_profile_table *profile)
{
	dlg_cell_t *dlg = NULL;
	dlg_profile_link_t *linker;

	/* get current dialog */
	dlg = dlg_get_msg_dialog(msg);

	if (dlg==NULL && !is_route_type(REQUEST_ROUTE)) {
		LM_CRIT("BUG - dialog not found in a non REQUEST route (%d)\n",
			REQUEST_ROUTE);
		return -1;
	}

	/* build new linker */
	linker = (struct dlg_profile_link*)shm_malloc(
		sizeof(struct dlg_profile_link) + (profile->has_value?value->len:0) );
	if (linker==NULL) {
		LM_ERR("no more shm memory\n");
		goto error;
	}
	memset(linker, 0, sizeof(struct dlg_profile_link));

	/* set backpointers to profile and linker (itself) */
	linker->profile = profile;
	linker->hash_linker.linker = linker;

	/* set the value */
	if (profile->has_value) {
		linker->hash_linker.value.s = (char*)(linker+1);
		memcpy( linker->hash_linker.value.s, value->s, value->len);
		linker->hash_linker.value.len = value->len;
	}
	sruid_next_safe(&_dlg_profile_sruid);
	strcpy(linker->hash_linker.puid, _dlg_profile_sruid.uid.s);
	linker->hash_linker.puid_len = _dlg_profile_sruid.uid.len;

	if (dlg!=NULL) {
		/* add linker directly to the dialog and profile */
		link_dlg_profile( linker, dlg);
	} else {
		/* if existing linkers are not from current request, just discard them */
		if (msg->id!=current_dlg_msg_id || msg->pid!=current_dlg_msg_pid) {
			current_dlg_msg_id = msg->id;
			current_dlg_msg_pid = msg->pid;
			destroy_linkers(current_pending_linkers);
			current_pending_linkers = NULL;
		}
		/* no dialog yet -> set linker as pending */
		if (msg->id!=current_dlg_msg_id || msg->pid!=current_dlg_msg_pid) {
			current_dlg_msg_id = msg->id;
			current_dlg_msg_pid = msg->pid;
			destroy_linkers(current_pending_linkers);
		}

		linker->next = current_pending_linkers;
		current_pending_linkers = linker;
	}

	dlg_release(dlg);
	return 0;
error:
	dlg_release(dlg);
	return -1;
}

/*!
 * \brief Add dialog to a profile
 * \param dlg dialog
 * \param value value
 * \param profile dialog profile table
 * \return 0 on success, -1 on failure
 */
int dlg_add_profile(dlg_cell_t *dlg, str *value, struct dlg_profile_table *profile,
		str *puid, time_t expires, int flags)
{
	dlg_profile_link_t *linker;
	str vkey;

	/* build new linker */
	linker = (struct dlg_profile_link*)shm_malloc(
		sizeof(struct dlg_profile_link) + (profile->has_value?(value->len+1):0) );
	if (linker==NULL) {
		LM_ERR("no more shm memory\n");
		goto error;
	}
	memset(linker, 0, sizeof(struct dlg_profile_link));

	/* set backpointers to profile and linker (itself) */
	linker->profile = profile;
	linker->hash_linker.linker = linker;

	/* set the value */
	if (profile->has_value) {
		linker->hash_linker.value.s = (char*)(linker+1);
		memcpy( linker->hash_linker.value.s, value->s, value->len);
		linker->hash_linker.value.len = value->len;
		linker->hash_linker.value.s[value->len] = '\0';
	}
	if(puid && puid->s && puid->len>0 && puid->len<SRUID_SIZE) {
		strcpy(linker->hash_linker.puid, puid->s);
		linker->hash_linker.puid_len = puid->len;
	} else {
		sruid_next_safe(&_dlg_profile_sruid);
		strcpy(linker->hash_linker.puid, _dlg_profile_sruid.uid.s);
		linker->hash_linker.puid_len = _dlg_profile_sruid.uid.len;
	}
	linker->hash_linker.expires = expires;
	linker->hash_linker.flags = flags;

	/* add linker directly to the dialog and profile */
	if(dlg!=NULL) {
		link_dlg_profile(linker, dlg);
	} else {
		vkey.s = linker->hash_linker.puid;
		vkey.len = linker->hash_linker.puid_len;
		profile->flags |= FLAG_PROFILE_REMOTE;
		link_profile(linker, &vkey);
	}
	return 0;
error:
	return -1;
}

/*!
 * \brief Unset a dialog profile
 * \param msg SIP message
 * \param value value
 * \param profile dialog profile table
 * \return 1 on success, -1 on failure
 */
int unset_dlg_profile(sip_msg_t *msg, str *value,
		dlg_profile_table_t *profile)
{
	dlg_cell_t *dlg;
	dlg_profile_link_t *linker;
	dlg_profile_link_t *linker_prev;
	dlg_entry_t *d_entry;

	if (is_route_type(REQUEST_ROUTE)) {
		LM_ERR("dialog delete profile cannot be used in request route\n");
		return -1;
	}

	/* get current dialog */
	dlg = dlg_get_msg_dialog(msg);

	if (dlg==NULL) {
		LM_WARN("dialog is NULL for delete profile\n");
		return -1;
	}

	/* check the dialog linkers */
	d_entry = &d_table->entries[dlg->h_entry];
	dlg_lock( d_table, d_entry);
	linker = dlg->profile_links;
	linker_prev = NULL;
	for( ; linker ; linker_prev=linker,linker=linker->next) {
		if (linker->profile==profile) {
			if (profile->has_value==0) {
				goto found;
			} else if (value && value->len==linker->hash_linker.value.len &&
			memcmp(value->s,linker->hash_linker.value.s,value->len)==0){
				goto found;
			}
			/* allow further search - maybe the dialog is inserted twice in
			 * the same profile, but with different values -bogdan
			 */
		}
	}
	dlg_unlock( d_table, d_entry);
	dlg_release(dlg);
	return -1;

found:
	/* table still locked */
	/* remove the linker element from dialog */
	if (linker_prev==NULL) {
		dlg->profile_links = linker->next;
	} else {
		linker_prev->next = linker->next;
	}
	linker->next = NULL;
	dlg_unlock( d_table, d_entry);
	/* remove linker from profile table and free it */
	destroy_linkers(linker);
	dlg_release(dlg);
	return 1;
}


/*!
 * \brief Check if a dialog belongs to a profile
 * \param msg SIP message
 * \param profile dialog profile table
 * \param value value
 * \return 1 on success, -1 on failure
 */
int is_dlg_in_profile(struct sip_msg *msg, struct dlg_profile_table *profile,
		str *value)
{
	struct dlg_cell *dlg;
	struct dlg_profile_link *linker;
	struct dlg_entry *d_entry;
	int ret;

	/* get current dialog */
	dlg = dlg_get_msg_dialog(msg);

	if (dlg==NULL)
		return -1;

	ret = -1;
	/* check the dialog linkers */
	d_entry = &d_table->entries[dlg->h_entry];
	dlg_lock( d_table, d_entry);
	for( linker=dlg->profile_links ; linker ; linker=linker->next) {
		if (linker->profile==profile) {
			if (profile->has_value==0) {
				dlg_unlock( d_table, d_entry);
				ret = 1;
				goto done;
			} else if (value && value->len==linker->hash_linker.value.len &&
			memcmp(value->s,linker->hash_linker.value.s,value->len)==0){
				dlg_unlock( d_table, d_entry);
				ret = 1;
				goto done;
			}
			/* allow further search - maybe the dialog is inserted twice in
			 * the same profile, but with different values -bogdan
			 */
		}
	}
	dlg_unlock( d_table, d_entry);

done:
	dlg_release(dlg);
	return ret;
}


/*!
 * \brief Get the size of a profile
 * \param profile evaluated profile
 * \param value value
 * \return the profile size
 */
unsigned int get_profile_size(struct dlg_profile_table *profile, str *value)
{
	unsigned int n,i;
	struct dlg_profile_hash *ph;

	if (profile->has_value==0 || value==NULL) {
		/* iterate through the hash and count all records */
		lock_get( &profile->lock );
		for( i=0,n=0 ; i<profile->size ; i++ )
			n += profile->entries[i].content;
		lock_release( &profile->lock );
		return n;
	} else {
		/* iterate through the hash entry and count only matching */
		/* calculate the hash position */
		i = calc_hash_profile( value, NULL, profile);
		n = 0;
		lock_get( &profile->lock );
		ph = profile->entries[i].first;
		if(ph) {
			do {
				/* compare */
				if ( value->len==ph->value.len &&
				memcmp(value->s,ph->value.s,value->len)==0 ) {
					/* found */
					n++;
				}
				/* next */
				ph=ph->next;
			}while( ph!=profile->entries[i].first );
		}
		lock_release( &profile->lock );
		return n;
	}
}

/*
 * Determine if message is in a dialog currently being tracked
 */
int	is_known_dlg(struct sip_msg *msg) {
	dlg_cell_t *dlg;

	dlg = dlg_get_msg_dialog(msg);
	
	if(dlg == NULL)
		return -1;

	dlg_release(dlg);

	return 1;
}

/*
 * \brief Set the timeout of all the dialogs in a given profile, by value.
 * \param profile The evaluated profile name.
 * \param value The value constraint.
 * \param timeout The dialog timeout to apply.
 */

int	dlg_set_timeout_by_profile(struct dlg_profile_table *profile, 
				   str *value, int timeout) 
{
	unsigned int		i = 0;
	dlg_cell_t		*this_dlg = NULL;
	struct dlg_profile_hash	*ph = NULL;

	/* Private structure necessary for manipulating dialog 
         * timeouts outside of profile locks.  Admittedly, an
         * ugly hack, but avoids some concurrency issues.
         */

	struct dlg_map_list {
		unsigned int		h_id;
		unsigned int		h_entry;
		struct dlg_map_list	*next;
	} *map_head, *map_scan, *map_scan_next;

	map_head = NULL;

	/* If the profile has no value, iterate through every 
	 * node and set its timeout.
	 */

	if(profile->has_value == 0 || value == NULL) {
		lock_get(&profile->lock);

		for(i = 0; i < profile->size; i ++) {
			ph = profile->entries[i].first;

			if(!ph) continue;
			
			do { 
				struct dlg_map_list *d = malloc(sizeof(struct dlg_map_list));

				if(!d)
					return -1;

				memset(d, 0, sizeof(struct dlg_map_list));

				d->h_id = ph->dlg->h_id;
				d->h_entry = ph->dlg->h_entry;

				if(map_head == NULL)
					map_head = d;
				else {
					d->next = map_head;
					map_head = d;
				}
	
				ph = ph->next;
			} while(ph != profile->entries[i].first);
		} 

		lock_release(&profile->lock);
	}

	else {
		i = calc_hash_profile(value, NULL, profile);

		lock_get(&profile->lock);

		ph = profile->entries[i].first;

		if(ph) {
			do {
				if(ph && value->len == ph->value.len &&
				   memcmp(value->s, ph->value.s, value->len) == 0) {
					struct dlg_map_list *d = malloc(sizeof(struct dlg_map_list));

					if(!d)
						return -1;

					memset(d, 0, sizeof(struct dlg_map_list));

					d->h_id = ph->dlg->h_id;
					d->h_entry = ph->dlg->h_entry;

					if(map_head == NULL)
						map_head = d;
					else {
						d->next = map_head;
						map_head = d;
					}
				}

				ph = ph->next;
			} while(ph && ph != profile->entries[i].first);
		}

		lock_release(&profile->lock);
	}

	/* Walk the list and bulk-set the timeout */
	
	for(map_scan = map_head; map_scan != NULL; map_scan = map_scan_next) {
		map_scan_next = map_scan->next;

		this_dlg = dlg_lookup(map_scan->h_entry, map_scan->h_id);

		if(!this_dlg) {
			LM_CRIT("Unable to find dialog %d:%d\n", map_scan->h_entry, map_scan->h_id);
		} else if(this_dlg->state >= DLG_STATE_EARLY) {	
			if(update_dlg_timeout(this_dlg, timeout) < 0) {
               			LM_ERR("Unable to set timeout on %d:%d\n", map_scan->h_entry,
					map_scan->h_id);
			}

	                dlg_release(this_dlg);
		}

		free(map_scan);
	}

	return 0;
}

/****************************** MI commands *********************************/

/*!
 * \brief Output a profile via MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return MI root output on success, NULL on failure
 */
struct mi_root * mi_get_profile(struct mi_root *cmd_tree, void *param)
{
	struct mi_node* node;
	struct mi_root* rpl_tree= NULL;
	struct mi_node* rpl = NULL;
	struct mi_attr* attr;
	struct dlg_profile_table *profile;
	str *value;
	str *profile_name;
	unsigned int size;
	int len;
	char *p;

	node = cmd_tree->node.kids;
	if (node==NULL || !node->value.s || !node->value.len)
		return init_mi_tree( 400, MI_SSTR(MI_MISSING_PARM));
	profile_name = &node->value;

	if (node->next) {
		node = node->next;
		if (!node->value.s || !node->value.len)
			return init_mi_tree( 400, MI_SSTR(MI_BAD_PARM));
		if (node->next)
			return init_mi_tree( 400, MI_SSTR(MI_MISSING_PARM));
		value = &node->value;
	} else {
		value = NULL;
	}

	/* search for the profile */
	profile = search_dlg_profile( profile_name );
	if (profile==NULL)
		return init_mi_tree( 404, MI_SSTR("Profile not found"));

	size = get_profile_size( profile , value );

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	node = add_mi_node_child(rpl, MI_DUP_VALUE, "profile", 7, NULL, 0);
	if (node==0) {
		free_mi_tree(rpl_tree);
		return NULL;
	}

	attr = add_mi_attr(node, MI_DUP_VALUE, "name", 4, 
		profile->name.s, profile->name.len);
	if(attr == NULL) {
		goto error;
	}

	if (value) {
		attr = add_mi_attr(node, MI_DUP_VALUE, "value", 5, value->s, value->len);
	} else {
		attr = add_mi_attr(node, MI_DUP_VALUE, "value", 5, NULL, 0);
	}
	if(attr == NULL) {
		goto error;
	}

	p= int2str((unsigned long)size, &len);
	attr = add_mi_attr(node, MI_DUP_VALUE, "count", 5, p, len);
	if(attr == NULL) {
		goto error;
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return NULL;
}


/*!
 * \brief List the profiles via MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return MI root output on success, NULL on failure
 */
struct mi_root * mi_profile_list(struct mi_root *cmd_tree, void *param )
{
	struct mi_node* node;
	struct mi_root* rpl_tree= NULL;
	struct mi_node* rpl = NULL;
	struct dlg_profile_table *profile;
	struct dlg_profile_hash *ph;
	str *profile_name;
	str *value;
	unsigned int i;

	node = cmd_tree->node.kids;
	if (node==NULL || !node->value.s || !node->value.len)
		return init_mi_tree( 400, MI_SSTR(MI_MISSING_PARM));
	profile_name = &node->value;

	if (node->next) {
		node = node->next;
		if (!node->value.s || !node->value.len)
			return init_mi_tree( 400, MI_SSTR(MI_BAD_PARM));
		if (node->next)
			return init_mi_tree( 400, MI_SSTR(MI_MISSING_PARM));
		value = &node->value;
	} else {
		value = NULL;
	}

	/* search for the profile */
	profile = search_dlg_profile( profile_name );
	if (profile==NULL)
		return init_mi_tree( 404, MI_SSTR("Profile not found"));

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	/* go through the hash and print the dialogs */
	if (profile->has_value==0 || value==NULL) {
		/* no value */
		lock_get( &profile->lock );
		for ( i=0 ; i< profile->size ; i++ ) {
			ph = profile->entries[i].first;
			if(ph) {
				do {
					/* print dialog */
					if ( mi_print_dlg( rpl, ph->dlg, 0)!=0 )
						goto error;
					/* next */
					ph=ph->next;
				}while( ph!=profile->entries[i].first );
			}
		}
		lock_release( &profile->lock );
	} else {
		/* check for value also */
		lock_get( &profile->lock );
		for ( i=0 ; i< profile->size ; i++ ) {
			ph = profile->entries[i].first;
			if(ph) {
				do {
					if ( value->len==ph->value.len &&
					memcmp(value->s,ph->value.s,value->len)==0 ) {
						/* print dialog */
						if ( mi_print_dlg( rpl, ph->dlg, 0)!=0 )
							goto error;
					}
					/* next */
					ph=ph->next;
				}while( ph!=profile->entries[i].first );
			}
		}
		lock_release( &profile->lock );
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return NULL;
}


/**
 * json serialization of dialog profiles
 */
int dlg_profiles_to_json(dlg_cell_t *dlg, srjson_doc_t *jdoc)
{
	dlg_profile_link_t *l;
	srjson_t *aj = NULL;
	srjson_t *pj = NULL;

	LM_DBG("serializing profiles for dlg[%u:%u]\n",
				dlg->h_entry, dlg->h_id);
	if(dlg==NULL || dlg->profile_links==NULL)
		return -1;
	LM_DBG("start of serializing profiles for dlg[%u:%u]\n",
				dlg->h_entry, dlg->h_id);

	for (l = dlg->profile_links ; l ; l=l->next) {
		if(aj==NULL)
		{
			aj = srjson_CreateArray(jdoc);
			if(aj==NULL)
			{
				LM_ERR("cannot create json profiles array object\n");
				goto error;
			}
		}
		pj = srjson_CreateObject(jdoc);
		if(pj==NULL)
		{
			LM_ERR("cannot create json dynamic profiles obj\n");
			goto error;
		}

		srjson_AddStrStrToObject(jdoc, pj,
					"name", 4,
					l->profile->name.s, l->profile->name.len);
		if(l->profile->has_value)
		{
			srjson_AddStrStrToObject(jdoc, pj,
					"value", 5,
					l->hash_linker.value.s, l->hash_linker.value.len);
		}
		if(l->hash_linker.puid[0]!='\0')
			srjson_AddStringToObject(jdoc, pj, "puid", l->hash_linker.puid);
		if(l->hash_linker.expires!=0)
			srjson_AddNumberToObject(jdoc, pj, "expires", l->hash_linker.expires);
		if(l->hash_linker.flags!=0)
			srjson_AddNumberToObject(jdoc, pj, "flags", l->hash_linker.flags);
		srjson_AddItemToArray(jdoc, aj, pj);
	}

	if(jdoc->root==NULL)
	{
		jdoc->root = srjson_CreateObject(jdoc);
		if(jdoc->root==NULL)
		{
			LM_ERR("cannot create json root\n");
			goto error;
		}
	}
	if(aj!=NULL)
		srjson_AddItemToObject(jdoc, jdoc->root, "profiles", aj);
	if(jdoc->buf.s != NULL)
	{
		jdoc->free_fn(jdoc->buf.s);
		jdoc->buf.s = NULL;
		jdoc->buf.len = 0;
	}
	jdoc->buf.s = srjson_PrintUnformatted(jdoc, jdoc->root);
	if(jdoc->buf.s!=NULL)
	{
		jdoc->buf.len = strlen(jdoc->buf.s);
		LM_DBG("serialized profiles for dlg[%u:%u] = [[%.*s]]\n",
				dlg->h_entry, dlg->h_id, jdoc->buf.len, jdoc->buf.s);
		return 0;
	}
	return -1;

error:
	srjson_Delete(jdoc, aj);
	return -1;
}


/**
 * json de-serialization of dialog profiles
 */
int dlg_json_to_profiles(dlg_cell_t *dlg, srjson_doc_t *jdoc)
{
	srjson_t *aj = NULL;
	srjson_t *it = NULL;
	srjson_t *jt = NULL;
	dlg_profile_table_t *profile;
	str name;
	str val;
	str puid;
	time_t expires;
	int flags;

	if(dlg==NULL || jdoc==NULL || jdoc->buf.s==NULL)
		return -1;

	if(jdoc->root == NULL)
	{
		jdoc->root = srjson_Parse(jdoc, jdoc->buf.s);
		if(jdoc->root == NULL)
		{
			LM_ERR("invalid json doc [[%s]]\n", jdoc->buf.s);
			return -1;
		}
	}
	aj = srjson_GetObjectItem(jdoc, jdoc->root, "profiles");
	if(aj!=NULL)
	{
		for(it=aj->child; it; it = it->next)
		{
			name.s = val.s = puid.s = NULL;
			expires = 0; flags = 0;
			for(jt = it->child; jt; jt = jt->next) {
				if(strcmp(jt->string, "name")==0) {
					name.s = jt->valuestring;
					name.len = strlen(name.s);
				} else if(strcmp(jt->string, "value")==0) {
					val.s = jt->valuestring;
					val.len = strlen(val.s);
				} else if(strcmp(jt->string, "puid")==0) {
					puid.s = jt->valuestring;
					puid.len = strlen(puid.s);
				} else if(strcmp(jt->string, "expires")==0) {
					expires = (time_t)jt->valueint;
				} else if(strcmp(jt->string, "flags")==0) {
					flags = jt->valueint;
				}
			}
			if(name.s==NULL)
				continue;
			profile = search_dlg_profile(&name);
			if(profile==NULL)
			{
				LM_ERR("profile [%.*s] not found\n", name.len, name.s);
				continue;
			}
			if(val.s!=NULL) {
				if(profile->has_value)
				{
					if(dlg_add_profile(dlg, &val, profile, &puid, expires, flags) < 0)
						LM_ERR("dynamic profile cannot be added, ignore!\n");
					else
						LM_DBG("dynamic profile added [%s : %s]\n", name.s, val.s);
				}
			} else {
				if(!profile->has_value)
				{
					if(dlg_add_profile(dlg, NULL, profile, &puid, expires, flags) < 0)
						LM_ERR("static profile cannot be added, ignore!\n");
					else
						LM_DBG("static profile added [%s]\n", name.s);
				}
			}
		}
	}
	return 0;
}

/*!
 *
 */
int dlg_cmd_remote_profile(str *cmd, str *pname, str *value, str *puid,
		time_t expires, int flags)
{
	dlg_profile_table_t *dprofile;
	int ret;

	if(cmd==NULL || cmd->s==NULL || cmd->len<=0
			|| pname==NULL || pname->s==NULL || pname->len<=0
			|| puid==NULL || puid->s==NULL || puid->len<=0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}
	dprofile = search_dlg_profile(pname);
	if(dprofile==NULL) {
		LM_ERR("profile [%.*s] not found\n", pname->len, pname->s);
		return -1;
	}
	if(dprofile->has_value) {
		if(value==NULL || value->s==NULL || value->len<=0) {
			LM_ERR("profile [%.*s] requires a value\n", pname->len, pname->s);
			return -1;
		}
	}

	if(cmd->len==3 && strncmp(cmd->s, "add", 3)==0) {
		if(value && value->s && value->len>0) {
			ret = dlg_add_profile(NULL, value, dprofile, puid, expires, flags);
		} else {
			ret = dlg_add_profile(NULL, NULL, dprofile, puid, expires, flags);
		}
		if(ret<0) {
			LM_ERR("failed to add to profile [%.*s]\n", pname->len, pname->s);
			return -1;
		}
	} else if(cmd->len==2 && strncmp(cmd->s, "rm", 2)==0) {
		ret = remove_profile(dprofile, value, puid);
		return ret;
	} else {
		LM_ERR("unknown command [%.*s]\n", cmd->len, cmd->s);
		return -1;
	}
	return 0;
}
