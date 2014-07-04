/*
 * $Id$
 *
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
 * History:
 * --------
 * 2008-04-20  initial version (bogdan)
 *
 */


/*!
 * \file
 * \brief Profile related functions for the dialog module
 * \ingroup dialog
 * Module: \ref dialog
 */


#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../route.h"
#include "../../modules/tm/tm_load.h"
#include "../../trim.h"
#include "dlg_hash.h"
#include "dlg_handlers.h"
#include "dlg_profile.h"


/*! size of dialog profile hash */
#define PROFILE_HASH_SIZE 16

/*! tm bindings */
extern struct tm_binds d_tmb;

/*! global dialog message id */
static unsigned int            current_dlg_msg_id = 0 ;

/*! global dialog */
struct dlg_cell                *current_dlg_pointer = NULL ;

/*! pending dialog links */
static struct dlg_profile_link *current_pending_linkers = NULL;

/*! global dialog profile list */
static struct dlg_profile_table *profiles = NULL;


static struct dlg_profile_table* new_dlg_profile( str *name,
		unsigned int size, unsigned int has_value);


struct dlg_cell *get_current_dlg_pointer(void)
{
	return current_dlg_pointer;
}

void reset_current_dlg_pointer(void)
{
	current_dlg_pointer = NULL;
}

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
			if ( !isalnum(name.s[i]) ) {
				LM_ERR("bad profile name <%.*s>, char %c - use only "
					"alphanumerical characters\n", name.len,name.s,name.s[i]);
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
	if (ptmp==NULL)
		profiles = profile;
	else
		ptmp->next = profile;

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
 * \brief Cleanup a profile
 * \param msg SIP message
 * \param flags unused
 * \param unused
 * \return 1
 */
int profile_cleanup( struct sip_msg *msg, unsigned int flags, void *param )
{
	current_dlg_msg_id = 0;
	if (current_dlg_pointer) {
		unref_dlg( current_dlg_pointer, 1);
		current_dlg_pointer = NULL;
	}
	if (current_pending_linkers) {
		destroy_linkers(current_pending_linkers);
		current_pending_linkers = NULL;
	}

	/* need to return non-zero - 0 will break the exec of the request */
	return 1;
}



struct dlg_cell* get_dialog_from_tm(struct cell *t)
{
    if (t==NULL || t==T_UNDEFINED)
        return NULL;

    struct tm_callback* x = (struct tm_callback*)(t->tmcb_hl.first);

    while(x){
        membar_depends();
        if (x->types==TMCB_MAX && x->callback==dlg_tmcb_dummy){
            return (struct dlg_cell*)(x->param);
        }
        x=x->next;
    }

    return NULL;
}

/*!
 * \brief Get the current dialog for a message, if exists
 * \param msg SIP message
 * \return NULL if called in REQUEST_ROUTE, pointer to dialog ctx otherwise
 */
struct dlg_cell *get_current_dialog(struct sip_msg *msg)
{

	if (is_route_type(REQUEST_ROUTE|BRANCH_ROUTE)) {
            LM_DBG("Get Current Dialog: Route type is REQUEST ROUTE or BRANCH ROUTE");
            LM_DBG("Get Current Dialog: SIP Method - %.*s", msg->first_line.u.request.method.len, msg->first_line.u.request.method.s);
		/* use the per-process static holder */
		if (msg->id==current_dlg_msg_id){
                    LM_DBG("Message Id [%i] equals current dlg msg id [%i] - returning current dlg pointer", msg->id, current_dlg_msg_id);
                    return current_dlg_pointer;
                }
                LM_DBG("Message Id [%i] not equal to current point dlg id [%i] - returning null", msg->id, current_dlg_msg_id);
		current_dlg_pointer = NULL;
		current_dlg_msg_id = msg->id;
		destroy_linkers(current_pending_linkers);
		current_pending_linkers = NULL;
		return NULL;
	} else {
		/* use current transaction to get dialog */
            LM_DBG("Route type is not REQUEST ROUTE or brancg route - getting from tm");
	    return get_dialog_from_tm(d_tmb.t_gett());
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
inline static unsigned int calc_hash_profile(str *value, struct dlg_cell *dlg,
		struct dlg_profile_table *profile)
{
	if (profile->has_value) {
		/* do hash over the value */
		return core_hash( value, NULL, profile->size);
	} else {
		/* do hash over dialog pointer */
		return ((unsigned long)dlg) % profile->size ;
	}
}


/*!
 * \brief Link a dialog profile
 * \param linker dialog linker
 * \param dlg dialog cell
 */
static void link_dlg_profile(struct dlg_profile_link *linker, struct dlg_cell *dlg)
{
	unsigned int hash;
	struct dlg_profile_entry *p_entry;
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

	/* calculate the hash position */
	hash = calc_hash_profile(&linker->hash_linker.value, dlg, linker->profile);
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
 * \brief Set the global variables to the current dialog
 * \param msg SIP message
 * \param dlg dialog cell
 */
void set_current_dialog(struct sip_msg *msg, struct dlg_cell *dlg)
{
	struct dlg_profile_link *linker;
	struct dlg_profile_link *tlinker;

	/* if linkers are not from current request, just discard them */
	if (msg->id!=current_dlg_msg_id) {
		current_dlg_msg_id = msg->id;
		destroy_linkers(current_pending_linkers);
	} else {
		/* add the linker, one be one, to the dialog */
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
	current_dlg_pointer = dlg;

	/* do not increase reference counter here, let caller handle it
	 * (yes, this is somewhat ugly) */
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
	struct dlg_cell *dlg;
	struct dlg_profile_link *linker;

	/* get current dialog */
	dlg = get_current_dialog(msg);

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
		return -1;
	}
	memset(linker, 0, sizeof(struct dlg_profile_link));

	/* set backpointer to profile */
	linker->profile = profile;

	/* set the value */
	if (profile->has_value) {
		linker->hash_linker.value.s = (char*)(linker+1);
		memcpy( linker->hash_linker.value.s, value->s, value->len);
		linker->hash_linker.value.len = value->len;
	}

	if (dlg!=NULL) {
		/* add linker directly to the dialog and profile */
		link_dlg_profile( linker, dlg);
	} else {
		/* no dialog yet -> set linker as pending */
		linker->next = current_pending_linkers;
		current_pending_linkers = linker;
	}

	return 0;
}


/*!
 * \brief Unset a dialog profile
 * \param msg SIP message
 * \param value value
 * \param profile dialog profile table
 * \return 1 on success, -1 on failure
 */
int unset_dlg_profile(struct sip_msg *msg, str *value,
		struct dlg_profile_table *profile)
{
	struct dlg_cell *dlg;
	struct dlg_profile_link *linker;
	struct dlg_profile_link *linker_prev;
	struct dlg_entry *d_entry;

	/* get current dialog */
	dlg = get_current_dialog(msg);

	if (dlg==NULL || is_route_type(REQUEST_ROUTE)) {
		LM_CRIT("BUG - dialog NULL or del_profile used in request route\n");
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
		str *value) {
	struct dlg_cell *dlg;
	struct dlg_profile_link *linker;
	struct dlg_entry *d_entry;

	LM_DBG("Getting current dialog");
	/* get current dialog */
	dlg = get_current_dialog(msg);

	if (dlg == NULL) {
		LM_DBG("Error: Current dlg is null");

		return -1;
	}
	LM_DBG("Current dlg found");

	/* check the dialog linkers */
	d_entry = &d_table->entries[dlg->h_entry];
	dlg_lock( d_table, d_entry);
	for (linker = dlg->profile_links; linker; linker = linker->next) {
		LM_DBG("Running through linkers");
		if (linker->profile == profile) {
			LM_DBG("Profile matches");
			if (profile->has_value == 0) {
				LM_DBG("Profile has value is zero returning true");
				dlg_unlock( d_table, d_entry);
				return 1;
			} else if (value && value->len == linker->hash_linker.value.len
					&& memcmp(value->s, linker->hash_linker.value.s, value->len)
							== 0) {
				LM_DBG("Profile has value equal to passed value returning true");
				dlg_unlock( d_table, d_entry);
				return 1;
			}
			/* allow further search - maybe the dialog is inserted twice in
			 * the same profile, but with different values -bogdan
			 */
		}
	}
	dlg_unlock( d_table, d_entry);
	return -1;
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
	if(get_current_dialog(msg) == NULL)
		return -1;

	return 1;
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
			lock_release( &profile->lock );
		}
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
			lock_release( &profile->lock );
		}
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return NULL;
}
