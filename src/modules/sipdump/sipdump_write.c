/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/dprint.h"
#include "../../core/ut.h"

#include "sipdump_write.h"

extern int sipdump_rotate;

static time_t sipdump_last_rotate = 0;

static sipdump_list_t *_sipdump_list = NULL;

#define SIPDUMP_FPATH_SIZE 256
static char _sipdump_fpath[SIPDUMP_FPATH_SIZE];
static str _sipdump_fpath_prefix = {0, 0};

static FILE *_sipdump_file = NULL;

/**
 *
 */
int sipdump_list_init(int en)
{
	if(_sipdump_list!=NULL)
		return 0;
	_sipdump_list = (sipdump_list_t*)shm_malloc(sizeof(sipdump_list_t));
	if(_sipdump_list==NULL) {
		LM_ERR("not enough shared memory\n");
		return -1;
	}
	memset(_sipdump_list, 0, sizeof(sipdump_list_t));
	if(lock_init(&_sipdump_list->lock)==NULL) {
		shm_free(_sipdump_list);
		LM_ERR("failed to init lock\n");
		return -1;
	}
	_sipdump_list->enable = en;
	return 0;
}

/**
 * 
 */
int sipdump_enabled(void)
{
	if(_sipdump_list==NULL)
		return 0;
	if(_sipdump_list->enable==0)
		return 0;
	return 1;
}
/**
 *
 */
int sipdump_list_destroy(void)
{
	sipdump_data_t *sdd = NULL;
	sipdump_data_t *sdd0 = NULL;
	if(_sipdump_list==NULL)
		return 0;
	
	sdd=_sipdump_list->first;
	while(sdd!=NULL) {
		sdd0 = sdd;
		sdd=sdd->next;
		shm_free(sdd0);
	}
	return 0;
}

/**
 *
 */
int sipdump_list_add(str *data)
{
	sipdump_data_t *sdd = NULL;

	sdd = (sipdump_data_t*)shm_malloc(sizeof(sipdump_data_t)
				+ (data->len+1)*sizeof(char));
	if(sdd==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(sdd, 0, sizeof(sipdump_data_t));
	sdd->data.s = (char*)sdd + sizeof(sipdump_data_t);
	sdd->data.len = data->len;
	memcpy(sdd->data.s, data->s, data->len);
	sdd->data.s[data->len] = '\0';
	lock_get(&_sipdump_list->lock);
	if(_sipdump_list->last) {
		_sipdump_list->last->next = sdd;
	} else {
		_sipdump_list->first = sdd;
	}
	_sipdump_list->last = sdd;
	lock_release(&_sipdump_list->lock);
	return 0;
}

/**
 * 
 */
static int sipdump_rotate_file(void)
{
	time_t tv;
	struct tm *ti = NULL;
	int n;

	tv = time(NULL);

	if(_sipdump_file!=NULL
			&& sipdump_last_rotate>0
			&& sipdump_last_rotate+sipdump_rotate>tv) {
		/* not yet the time for rotation */
		return 0;
	}

	if(_sipdump_file != NULL) {
		fclose(_sipdump_file);
	}
	ti = localtime(&tv);
	n = snprintf(_sipdump_fpath+_sipdump_fpath_prefix.len,
			SIPDUMP_FPATH_SIZE-_sipdump_fpath_prefix.len,
			"%d-%02d-%02d--%02d-%02d-%02d.data",
			1900+ti->tm_year, ti->tm_mon, ti->tm_mday,
			ti->tm_hour, ti->tm_min, ti->tm_sec);
	_sipdump_file = fopen( _sipdump_fpath , "w" );
	if(_sipdump_file==NULL) {
		LM_ERR("failed to open file %s\n", _sipdump_fpath);
		return -1;
	}
	sipdump_last_rotate = tv;
	
	return 0;
}

/**
 * 
 */
int sipdump_file_init(str *folder, str *fprefix)
{
	_sipdump_fpath_prefix.len = snprintf(_sipdump_fpath, SIPDUMP_FPATH_SIZE-64,
			"%.*s/%.*s",
			folder->len, folder->s,
			fprefix->len, fprefix->s);
	if(_sipdump_fpath_prefix.len<0
			|| _sipdump_fpath_prefix.len>=SIPDUMP_FPATH_SIZE-64) {
		LM_ERR("sipdump file path failed or is too long\n");
		return -1;
	}
	_sipdump_fpath_prefix.s = _sipdump_fpath;
	return 0;
}

/**
 *
 */
void sipdump_timer_exec(unsigned int ticks, void *param)
{
	sipdump_data_t *sdd = NULL;
	int cnt = 0;

	if(_sipdump_list==NULL || _sipdump_list->first==NULL)
		return;

	if(sipdump_rotate_file()<0) {
		LM_ERR("sipdump rotate file failed\n");
		return;
	}

	while(1) {
		lock_get(&_sipdump_list->lock);
		if(_sipdump_list->first==NULL) {
			lock_release(&_sipdump_list->lock);
			if(_sipdump_file) fflush(_sipdump_file);
			return;
		}
		sdd = _sipdump_list->first;
		_sipdump_list->first = sdd->next;
		if(sdd->next==NULL) {
			_sipdump_list->last = NULL;
		}
		_sipdump_list->count--;
		lock_release(&_sipdump_list->lock);
		cnt++;
		if(cnt>2000) {
			if(sipdump_rotate_file()<0) {
				LM_ERR("sipdump rotate file failed\n");
				return;
			}
			cnt=0;
		}
		if(_sipdump_file==NULL) {
			LM_ERR("sipdump file is not open\n");
			return;
		}
		/* LM_NOTICE("writing: [[%.*s]] (%d)\n",
			sdd->data.len, sdd->data.s, sdd->data.len); */
		fwrite(sdd->data.s, 1, sdd->data.len, _sipdump_file);
		shm_free(sdd);
	}
}
