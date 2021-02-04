/**
 * keepalive module - remote destinations probing
 *
 * Copyright (C) 2017 Guillaume Bour <guillaume@bour.cc>
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

/*! \file
 * \ingroup keepalive
 * \brief Keepalive :: Send keepalives
 */

/*! \defgroup keepalive Keepalive :: Probing remote gateways by sending keepalives
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../tm/tm_load.h"

#include "keepalive.h"
#include "api.h"


/*
 * Regroup all exported functions in keepalive_api_t structure
 *
 */
int bind_keepalive(keepalive_api_t *api)
{
	if(!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	api->add_destination = ka_add_dest;
	api->destination_state = ka_destination_state;
	api->del_destination = ka_del_destination;
	api->lock_destination_list = ka_lock_destination_list;
	api->unlock_destination_list = ka_unlock_destination_list;
	return 0;
}

/*
 * Add a new destination in keepalive pool
 */
int ka_add_dest(str *uri, str *owner, int flags, int ping_interval,
    ka_statechanged_f statechanged_clb, ka_response_f response_clb, void *user_attr)
{
	struct sip_uri _uri;
	ka_dest_t *dest=0,*hollow=0;

	LM_DBG("adding destination: %.*s\n", uri->len, uri->s);
	ka_lock_destination_list();
	if(ka_find_destination(uri , owner , &dest , &hollow)){
		LM_INFO("uri [%.*s] already in stack --ignoring \r\n",uri->len, uri->s);
		dest->counter=0;
		ka_unlock_destination_list();
		return -2;
	}

	dest = (ka_dest_t *)shm_malloc(sizeof(ka_dest_t));
	if(dest == NULL) {
		LM_ERR("no more memory.\n");
		goto err;
	}
	memset(dest, 0, sizeof(ka_dest_t));

	if(uri->len >= 4 && (!strncasecmp("sip:", uri->s, 4)
							   || !strncasecmp("sips:", uri->s, 5))) {
		// protocol found
		if(ka_str_copy(uri, &(dest->uri), NULL) < 0)
			goto err;
	} else {
		if(ka_str_copy(uri, &(dest->uri), "sip:") < 0)
			goto err;
	}

	// checking uri is valid
	if(parse_uri(dest->uri.s, dest->uri.len, &_uri) != 0) {
		LM_ERR("invalid uri <%.*s>\n", dest->uri.len, dest->uri.s);
		goto err;
	}

	if(ka_str_copy(owner, &(dest->owner), NULL) < 0)
		goto err;

	if(sruid_next(&ka_sruid) < 0)
		goto err;
	ka_str_copy(&(ka_sruid.uid), &(dest->uuid), NULL);

	dest->flags = flags;
	dest->statechanged_clb = statechanged_clb;
	dest->response_clb = response_clb;
	dest->user_attr = user_attr;
	dest->ping_interval = MS_TO_TICKS((ping_interval == 0 ? ka_ping_interval : ping_interval) * 1000) ;
	if (lock_init(&dest->lock)==0){
		LM_ERR("failed initializing Lock \n");
	}

    dest->timer = timer_alloc();
	if (dest->timer == NULL) {
		LM_ERR("failed allocating timer\n");
		goto err;
  	}

	timer_init(dest->timer, ka_check_timer, dest, 0);

	if(timer_add(dest->timer, MS_TO_TICKS(KA_FIRST_TRY_DELAY)) < 0){
		LM_ERR("failed to start timer\n");
		goto err;
	}

	dest->next = ka_destinations_list->first;
	ka_destinations_list->first = dest;

	ka_unlock_destination_list();

	return 1;

err:
	if(dest) {
		if(dest->uri.s)
			shm_free(dest->uri.s);

		shm_free(dest);
	}
	ka_unlock_destination_list();

	return -1;
}
/*
 *
 */
ka_state ka_destination_state(str *destination)
{
	ka_dest_t *ka_dest = NULL;
	ka_lock_destination_list();
	for(ka_dest = ka_destinations_list->first; ka_dest != NULL;
			ka_dest = ka_dest->next) {
		if((destination->len == ka_dest->uri.len - 4)
				&& (strncmp(ka_dest->uri.s + 4, destination->s, ka_dest->uri.len - 4)
						== 0)) {
			break;
		}
	}
	ka_unlock_destination_list();
	if(ka_dest == NULL) {
		return (-1);
	}

	return ka_dest->state;
}
/*!
* @function ka_del_destination
* @abstract deletes given sip uri in allocated destination stack as named ka_alloc_destinations_list
*
* @param msg sip message
* @param uri given uri
* @param owner given owner name, not using now
*	*
* @result 1 successful  , -1 fail
*/
int ka_del_destination(str *uri, str *owner){
	LM_DBG("removing destination: %.*s\n", uri->len, uri->s);
	ka_dest_t *target=0,*head=0;
	ka_lock_destination_list();

	if(!ka_find_destination(uri,owner,&target,&head)){
		LM_ERR("Couldn't find destination \r\n");
		goto err;
	}

	if(!target){
		LM_ERR("Couldn't find destination \r\n");
		goto err;
	}
	lock_get(&target->lock);
	if(!head){
		LM_DBG("There isn't any head so maybe it is first \r\n");
		ka_destinations_list->first = target->next;
	} else {
		head->next = target->next;
	}
	lock_release(&target->lock);
	free_destination(target);
	ka_unlock_destination_list();
	return 1;
err:
	ka_unlock_destination_list();
	return -1;
}
/*!
* @function ka_find_destination
* @abstract find given destination uri address in destination_list stack
*           don't forget to add lock via ka_lock_destination_list to prevent crashes
* @param *uri given uri
* @param *owner given owner name
* @param **target searched address in stack
* @param **head which points target
*	*
* @result 1 successful  , -1 fail
*/
int ka_find_destination(str *uri, str *owner, ka_dest_t **target, ka_dest_t **head){

	ka_dest_t  *dest=0 ,*temp=0;
	LM_DBG("finding destination: %.*s\n", uri->len, uri->s);

	for(dest = ka_destinations_list->first ;dest; temp=dest, dest= dest->next ){
		if(!dest)
			break;

		if (STR_EQ(*uri, dest->uri) && STR_EQ(*owner, dest->owner)){
			*head = temp;
			*target = dest;
			LM_DBG("destination is found [target : %p] [head : %p] \r\n",target,temp);
			return 1;
		}
	}

	return 0;

}

/*!
* @function ka_find_destination_by_uuid
*			don't forget to add lock via ka_lock_destination_list to prevent crashes
*
* @param *uuid uuid of ka_dest record
* @param **target searched address in stack
* @param **head which points target
*	*
* @result 1 successful  , 0 fail
*/
int ka_find_destination_by_uuid(str uuid, ka_dest_t **target, ka_dest_t **head){
	ka_dest_t  *dest=0 ,*temp=0;

	LM_DBG("finding destination with uuid:%.*s\n", uuid.len, uuid.s);

	for(dest = ka_destinations_list->first ;dest ; temp = dest, dest = dest->next ){
		if(!dest)
			break;

		if (STR_EQ(uuid, dest->uuid)){
			*head = temp;
			*target = dest;
			LM_DBG("destination is found [target : %p] [head : %p] \r\n",target,temp);
			return 1;
		}
	}

	return 0;

}


/*!
* @function free_destination
* @abstract free ka_dest_t members
*
* @param *dest which is freed

* @result 1 successful  , -1 fail
*/
int free_destination(ka_dest_t *dest){
	
	if(dest){
        if(timer_del(dest->timer) < 0){
          LM_ERR("failed to remove timer for destination <%.*s>\n", dest->uri.len, dest->uri.s);
          return -1;
        }

        timer_free(dest->timer);
		lock_destroy(dest->lock);
		if(dest->uri.s)
			shm_free(dest->uri.s);

		if(dest->owner.s)
			shm_free(dest->owner.s);

		if(dest->uuid.s)
			shm_free(dest->uuid.s);
		
		shm_free(dest);
	}

	return 1;
}

int ka_lock_destination_list(){
	if(ka_destinations_list){
		lock_get(ka_destinations_list->lock);
		return 1;
	}
	return 0;
}

int ka_unlock_destination_list(){
	if(ka_destinations_list){
		lock_release(ka_destinations_list->lock);
		return 1;
	}
	return 0;
}
