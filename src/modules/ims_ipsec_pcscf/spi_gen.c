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

typedef struct spi_generator{
	pthread_mutex_t	spis_mut;
	spi_list_t		used_spis;
	uint32_t		spi_val;
	uint32_t		min_spi;
	uint32_t		max_spi;
} spi_generator_t;

spi_generator_t* spi_data = NULL;

int init_spi_gen(uint32_t start_val, uint32_t range)
{
    if(start_val < 1) {
        return 1;
    }

    if(UINT32_MAX - range < start_val) {
        return 2;
    }

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

    spi_data->used_spis = create_list();

    spi_data->spi_val = spi_data->min_spi = start_val;
    spi_data->max_spi = start_val + range;

	pthread_mutex_unlock(&spi_data->spis_mut);

    return 0;
}

uint32_t acquire_spi()
{
	if(!spi_data){
		return 0;
	}

	if(pthread_mutex_lock(&spi_data->spis_mut) != 0){
		return 0;
	}

    //save the initial value for the highly unlikely case where there are no free SPIs
	uint32_t initial_val = spi_data->spi_val;
    uint32_t ret = 0; // by default return invalid SPI

    while(1) {
		if(spi_in_list(&spi_data->used_spis, spi_data->spi_val) == 0){
			ret = spi_data->spi_val;
			spi_data->spi_val++;

			if(spi_data->spi_val >= spi_data->max_spi) { //reached the top of the range - reset
				spi_data->spi_val = spi_data->min_spi;
			}

            break;
        }

		spi_data->spi_val++; //the current SPI is not available - increment

		if(spi_data->spi_val >= spi_data->max_spi){ //reached the top of the range - reset
			spi_data->spi_val = spi_data->min_spi;
        }


		pthread_mutex_unlock(&spi_data->spis_mut);
		return ret;
        }

    }

    //found unused SPI - add it to the used list
	if(spi_add(&spi_data->used_spis, ret) != 0){
        ret = 0;
    }

	pthread_mutex_unlock(&spi_data->spis_mut);

    return ret;
}

int release_spi(uint32_t id)
{
	if(!spi_data){
		return 1;
	}

	if(pthread_mutex_lock(&spi_data->spis_mut) != 0){
        return 1;
    }

	spi_remove(&spi_data->used_spis, id);

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

	destroy_list(&spi_data->used_spis);
	spi_data->spi_val = spi_data->min_spi;

	pthread_mutex_unlock(&spi_data->spis_mut);

	return 0;
}

int destroy_spi_gen()
{
	if(!spi_data){
		return 1;
	}

	destroy_list(&spi_data->used_spis);

	int ret = pthread_mutex_destroy(&spi_data->spis_mut);
	shm_free(spi_data);
	return ret;
}
