/*
 * IMS IPSEC PCSCF module
 *
 * Copyright (C) 2018 Tsvetomir Dimitrov
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

#include "spi_gen.h"
#include "spi_list.h"
#include <pthread.h>
#include "../../core/mem/shm_mem.h"

#define MAX_HASH_SPI 10000

typedef struct spi_generator{
	pthread_mutex_t	spis_mut;
	spi_list_t	used_spis[MAX_HASH_SPI];
	spi_list_t	free_spi;
	uint32_t		spi_val;
	uint32_t		min_spi;
	uint32_t		max_spi;
} spi_generator_t;

spi_generator_t* spi_data = NULL;

int init_spi_gen(uint32_t spi_start_val, uint32_t spi_range, uint32_t sport_start_val, uint32_t cport_start_val, uint32_t port_range)
{
    if(spi_start_val < 1) {
        return 1;
    }

    if(UINT32_MAX - spi_range < spi_start_val) {
        return 2;
    }

    //save the initial value for the highly unlikely case where there are no free SPIs
    uint32_t sport = sport_start_val;
    uint32_t cport = cport_start_val;
    uint32_t j = 0;

    if(spi_data){
        return 3;
    }

    spi_data = shm_malloc(sizeof(spi_generator_t));
    if(spi_data == NULL){
        return 4;
    }

    if(pthread_mutex_init(&spi_data->spis_mut, NULL)){
        shm_free(spi_data);
        return 5;
    }

	if(pthread_mutex_lock(&spi_data->spis_mut) != 0){
		return 6;
	}

    for(j = 0; j < MAX_HASH_SPI; j++) {
        spi_data->used_spis[j] = create_list();
    }

    spi_data->free_spi = create_list();
    spi_data->spi_val  = spi_data->min_spi = spi_start_val;
    spi_data->max_spi  = spi_start_val + spi_range;

    LM_ERR("spi_data:%p init free spi for IPSEC tunnel min_spi:%u max_api:%u\n", spi_data, spi_data->min_spi, spi_data->max_spi);

    for(j = spi_data->min_spi; j < spi_data->max_spi; j+=2)
    {
        spi_add(&spi_data->free_spi, j, j+1, cport, sport);
        LM_ERR("spi_data:%p add element to spi free list head:%p\n", spi_data, spi_data->free_spi.head);
        cport++;
        sport++;

        if(cport >= cport_start_val + port_range) {
            cport = cport_start_val;
        }

        if(sport >= sport_start_val + port_range) {
            sport = sport_start_val;
        }
    }

	pthread_mutex_unlock(&spi_data->spis_mut);

    return 0;
}

uint32_t acquire_spi(uint32_t* spi_cid, uint32_t* spi_sid, uint16_t* cport, uint16_t* sport)
{
	if(!spi_data){
		LM_ERR("spi_data is NULL\n");
		return 0;
	}

	if(pthread_mutex_lock(&spi_data->spis_mut) != 0){
		LM_ERR("spi_data failed to lock\n");
		return 0;
	}

    if(!spi_data->free_spi.head)
    {
        LM_ERR("spi_data:%p spi_data->free_spi.head %p\n", spi_data, spi_data->free_spi.head);
        pthread_mutex_unlock(&spi_data->spis_mut);
        return 0;
    }

    *spi_cid = spi_data->free_spi.head->spi_cid;
    *spi_sid = spi_data->free_spi.head->spi_sid;
    *sport   = spi_data->free_spi.head->sport;
    *cport   = spi_data->free_spi.head->cport;
    spi_remove_head(&spi_data->free_spi);
    spi_add(&spi_data->used_spis[*spi_cid % MAX_HASH_SPI], *spi_cid, *spi_sid, *cport, *sport);
    pthread_mutex_unlock(&spi_data->spis_mut);

    LM_DBG("spi acquired spi_cid:%u spi_sid:%u sport:%u cport:%u \n",  *spi_cid, *spi_sid, *sport, *cport);

    return 1;
}

int release_spi(uint32_t spi_cid, uint32_t spi_sid, uint16_t cport, uint16_t sport)
{
	LM_DBG("releasing spi spi_data:%p spi_cid:%u spi_sid:%u cport:%u sport:%u\n", spi_data, spi_cid, spi_sid, cport, sport);
	if(!spi_data){
		return 1;
	}

	if(pthread_mutex_lock(&spi_data->spis_mut) != 0){
        return 1;
    }

    // if we successfully remove from used spi we will insert into free spi
    if(spi_remove(&spi_data->used_spis[spi_cid % MAX_HASH_SPI], spi_cid, spi_sid))
    {
        spi_add(&spi_data->free_spi, spi_cid, spi_sid, cport, sport);
    }

	pthread_mutex_unlock(&spi_data->spis_mut);

    return 0;
}

int clean_spi_list()
{
	if(!spi_data){
		return 1;
	}

	if(pthread_mutex_lock(&spi_data->spis_mut) != 0){
		return 1;
	}

  uint32_t j = 0;
	for(j = 0; j < MAX_HASH_SPI; j++) {
		destroy_list(&spi_data->used_spis[j]);
	}
  destroy_list(&spi_data->free_spi);
	spi_data->spi_val = spi_data->min_spi;

	pthread_mutex_unlock(&spi_data->spis_mut);

	return 0;
}

int destroy_spi_gen()
{
	LM_ERR("destroy spi list \n" );
	if(!spi_data){
		return 1;
	}

	clean_spi_list();

	int ret = pthread_mutex_destroy(&spi_data->spis_mut);
	shm_free(spi_data);
	return ret;
}
