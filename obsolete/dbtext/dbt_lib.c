/*
 * $Id$
 *
 * DBText library
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * DBText library
 *   
 * 2003-01-30 created by Daniel
 * 
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"

#include "dbt_util.h"
#include "dbt_lib.h"

static dbt_cache_p *_cachedb = NULL;
static gen_lock_t *_cachesem = NULL;

/**
 *
 */
int dbt_init_cache()
{
	if(!_cachesem)
	{
	/* init locks */
		_cachesem = lock_alloc();
		if(!_cachesem)
		{
			LOG(L_CRIT,"dbtext:dbt_init_cache: could not alloc a lock\n");
			return -1;
		}
		if (lock_init(_cachesem)==0)
		{
			LOG(L_CRIT,"dbtext:dbt_init_cache: could not initialize a lock\n");
			lock_dealloc(_cachesem);
			return -1;
		}
	}
	/* init pointer to caches list */
	if (!_cachedb) {
		_cachedb = shm_malloc( sizeof(dbt_cache_p) );
		if (!_cachedb) {
			LOG(L_CRIT,"dbtext:dbt_init_cache: no enough shm mem\n");
			lock_dealloc(_cachesem);
			return -1;
		}
		*_cachedb = NULL;
	}
	
	return 0;
}

/**
 *
 */
dbt_cache_p dbt_cache_get_db(str *_s)
{
	dbt_cache_p _dcache=NULL;;
	if(!_cachesem || !_cachedb)
	{
		LOG(L_ERR, "DBT:dbt_cache_get_db:dbtext cache is not initialized!\n");
		return NULL;
	}
	if(!_s || !_s->s || _s->len<=0)
		return NULL;

	DBG("DBT:dbt_cache_get_db: looking for db %.*s!\n",_s->len,_s->s);

	lock_get(_cachesem);
	
	_dcache = *_cachedb;
	while(_dcache)
	{
		lock_get(&_dcache->sem);
		if(_dcache->dbp)
		{
			if(_dcache->dbp->name.len==_s->len 
					&& !strncasecmp(_dcache->dbp->name.s, _s->s, _s->len))
			{
				lock_release(&_dcache->sem);
				DBG("DBT:dbt_cache_get_db: db already cached!\n");
				goto done;
			}
		}
		lock_release(&_dcache->sem);
		
		_dcache = _dcache->next;
	}
	if(!dbt_is_database(_s))
	{
		LOG(L_ERR, "DBT:dbt_cache_get_db: database [%.*s] does not exists!\n", 
				_s->len, _s->s);
		goto done;
	}
	DBG("DBT:dbt_cache_get_db: new db!\n");
	
	_dcache = (dbt_cache_p)shm_malloc(sizeof(dbt_cache_t));
	if(!_dcache)
	{
		LOG(L_ERR, "DBT:dbt_cache_get_db: no memory for dbt_cache_t.\n");
		goto done;
	}
	
	_dcache->dbp = (dbt_db_p)shm_malloc(sizeof(dbt_db_t));
	if(!_dcache->dbp)
	{
		LOG(L_ERR, "DBT:dbt_cache_get_db: no memory for dbt_db_t!\n");
		shm_free(_dcache);
		goto done;
	}

	_dcache->dbp->name.s = (char*)shm_malloc(_s->len*sizeof(char));
	if(!_dcache->dbp->name.s)
	{
		LOG(L_ERR, "DBT:dbt_cache_get_db: no memory for s!!\n");
		shm_free(_dcache->dbp);
		shm_free(_dcache);
		_dcache = NULL;
		goto done;
	}
	
	memcpy(_dcache->dbp->name.s, _s->s, _s->len);
	_dcache->dbp->name.len = _s->len;
	_dcache->dbp->tables = NULL;
	
	if(!lock_init(&_dcache->sem))
	{
		LOG(L_ERR, "DBT:dbt_cache_get_db: no sems!\n");
		shm_free(_dcache->dbp->name.s);
		shm_free(_dcache->dbp);
		shm_free(_dcache);
		_dcache = NULL;
		goto done;
	}
	
	_dcache->prev = NULL;

	if(*_cachedb)
	{
		_dcache->next = *_cachedb;
		(*_cachedb)->prev = _dcache;
	}
	else
		_dcache->next = NULL;

	*_cachedb = _dcache;

done:
	lock_release(_cachesem);
	return _dcache;
}

/**
 *
 */
int dbt_cache_check_db(str *_s)
{
	dbt_cache_p _dcache=NULL;;
	if(!_cachesem || !(*_cachedb) || !_s || !_s->s || _s->len<=0)
		return -1;
	
	lock_get(_cachesem);
	
	_dcache = *_cachedb;
	while(_dcache)
	{
		if(_dcache->dbp)
		{
			if(_dcache->dbp->name.len == _s->len &&
				strncasecmp(_dcache->dbp->name.s, _s->s, _s->len))
			{
				lock_release(_cachesem);
				return 0;
			}
		}
		_dcache = _dcache->next;
	}
	
	lock_release(_cachesem);
	return -1;
}

/**
 *
 */
int dbt_cache_del_db(str *_s)
{
	dbt_cache_p _dcache=NULL;;
	if(!_cachesem || !(*_cachedb) || !_s || !_s->s || _s->len<=0)
		return -1;
	
	lock_get(_cachesem);
	
	_dcache = *_cachedb;
	while(_dcache)
	{
		if(_dcache->dbp)
		{
			if(_dcache->dbp->name.len == _s->len 
					&& strncasecmp(_dcache->dbp->name.s, _s->s, _s->len))
				break;
		}
		// else - delete this cell
		_dcache = _dcache->next;
	}
	if(!_dcache)
	{
		lock_release(_cachesem);
		return 0;
	}
	
	if(_dcache->prev)
		(_dcache->prev)->next = _dcache->next;
	else
		*_cachedb = _dcache->next;

	if(_dcache->next)
		(_dcache->next)->prev = _dcache->prev;
	
	lock_release(_cachesem);
	
	dbt_cache_free(_dcache);
	
	return 0;
}

/**
 *
 */
tbl_cache_p dbt_db_get_table(dbt_cache_p _dc, str *_s)
{
//	dbt_db_p _dbp = NULL;
	tbl_cache_p _tbc = NULL;
	dbt_table_p _dtp = NULL;

	if(!_dc || !_s || !_s->s || _s->len<=0)
		return NULL;

	lock_get(&_dc->sem);
	if(!_dc->dbp)
	{
		lock_release(&_dc->sem);
		return NULL;
	}
	
	_tbc = _dc->dbp->tables;
	while(_tbc)
	{
		if(_tbc->dtp)
		{
			lock_get(&_tbc->sem);
			if(_tbc->dtp->name.len == _s->len 
				&& !strncasecmp(_tbc->dtp->name.s, _s->s, _s->len ))
			{
				lock_release(&_tbc->sem);
				lock_release(&_dc->sem);
				return _tbc;
			}
			lock_release(&_tbc->sem);
		}
		_tbc = _tbc->next;
	}

	// new table
	_tbc = tbl_cache_new();
	if(!_tbc)
	{
		lock_release(&_dc->sem);
		return NULL;
	}
	
	_dtp = dbt_load_file(_s, &(_dc->dbp->name));

#ifdef DBT_EXTRA_DEBUG
	DBG("DTB:dbt_db_get_table: %.*s\n", _s->len, _s->s);
	dbt_print_table(_dtp, NULL);
#endif

	if(!_dtp)
	{
		lock_release(&_dc->sem);
		return NULL;
	}
	_tbc->dtp = _dtp;
	
	if(_dc->dbp->tables)
		(_dc->dbp->tables)->prev = _tbc;
	_tbc->next = _dc->dbp->tables;
	_dc->dbp->tables = _tbc;
		
	lock_release(&_dc->sem);

	return _tbc;
}

/**
 *
 */
int dbt_db_del_table(dbt_cache_p _dc, str *_s)
{
	tbl_cache_p _tbc = NULL;
	if(!_dc || !_s || !_s->s || _s->len<=0)
		return -1;

	lock_get(&_dc->sem);
	if(!_dc->dbp)
	{
		lock_release(&_dc->sem);
		return -1;
	}

	_tbc = _dc->dbp->tables;
	while(_tbc)
	{
		if(_tbc->dtp)
		{
			lock_get(&_tbc->sem);
			if(_tbc->dtp->name.len == _s->len 
				&& !strncasecmp(_tbc->dtp->name.s, _s->s, _s->len))
			{
				if(_tbc->prev)
					(_tbc->prev)->next = _tbc->next;
				else
					_dc->dbp->tables = _tbc->next;
	
				if(_tbc->next)
					(_tbc->next)->prev = _tbc->prev;
				break;
			}
			lock_release(&_tbc->sem);
		}
		_tbc = _tbc->next;
	}

	lock_release(&_dc->sem);

	tbl_cache_free(_tbc);
	
	return 0;
}

/**
 *
 */
int dbt_cache_destroy()
{
	dbt_cache_p _dc=NULL, _dc0=NULL;
	
	if(!_cachesem)
		return -1;
	
	lock_get(_cachesem);
	if(	_cachedb!=NULL )
	{
		_dc = *_cachedb;
		while(_dc)
		{
			_dc0 = _dc;
			_dc = _dc->next;
			dbt_cache_free(_dc0);
		}
		shm_free(_cachedb);
	}
	lock_destroy(_cachesem);
	lock_dealloc(_cachesem);
	
	return 0;
}

/**
 *
 */
int dbt_cache_print(int _f)
{
	dbt_cache_p _dc=NULL;
	tbl_cache_p _tbc = NULL;
	
	if(!_cachesem)
		return -1;
	
	lock_get(_cachesem);
	
	_dc = *_cachedb;
	while(_dc)
	{
		lock_get(&_dc->sem);
		if(_dc->dbp)
		{
			if(_f)
				fprintf(stdout, "\n--- Database [%.*s]\n", _dc->dbp->name.len,
								_dc->dbp->name.s);

			_tbc = _dc->dbp->tables;
			while(_tbc)
			{
				lock_get(&_tbc->sem);
				if(_tbc->dtp)
				{
					if(_f)
					{
						fprintf(stdout, "\n----- Table [%.*s]\n",
								_tbc->dtp->name.len, _tbc->dtp->name.s);
						fprintf(stdout, "-------  LA=<%d> FL=<%x> AC=<%d>"
								" AV=<%d>\n", _tbc->dtp->mark, _tbc->dtp->flag,
								_tbc->dtp->auto_col, _tbc->dtp->auto_val);
						dbt_print_table(_tbc->dtp, NULL);
					}
					else
					{
						if(_tbc->dtp->flag & DBT_TBFL_MODI)
						{
							dbt_print_table(_tbc->dtp, &(_dc->dbp->name));
							dbt_table_update_flags(_tbc->dtp,DBT_TBFL_MODI, 
									DBT_FL_UNSET, 0);
						}
					}
				}
				lock_release(&_tbc->sem);
				_tbc = _tbc->next;
			}
		}
		lock_release(&_dc->sem);
		
		_dc = _dc->next;
	}
	
	lock_release(_cachesem);
	
	return 0;
}

/**
 *
 */
int dbt_cache_free(dbt_cache_p _dc)
{
	if(!_dc)
		return -1;

	lock_get(&_dc->sem);

	if(_dc->dbp)
		dbt_db_free(_dc->dbp);
	
	lock_destroy(&_dc->sem);

	shm_free(_dc);

	return 0;
}

/**
 *
 */
int dbt_db_free(dbt_db_p _dbp)
{
	tbl_cache_p _tbc = NULL, _tbc0=NULL;
	if(!_dbp)
		return -1;

	_tbc = _dbp->tables;

	while(_tbc)
	{
		_tbc0 = _tbc;
		tbl_cache_free(_tbc0);
		_tbc = _tbc->next;
	}
	
	if(_dbp->name.s)
		shm_free(_dbp->name.s);
	
	shm_free(_dbp);

	return 0;
}

/**
 *
 */
tbl_cache_p tbl_cache_new()
{
	tbl_cache_p _tbc = NULL;
	_tbc = (tbl_cache_p)shm_malloc(sizeof(tbl_cache_t));
	if(!_tbc)
		return NULL;
	if(!lock_init(&_tbc->sem))
	{
		shm_free(_tbc);
		return NULL;
	}
	return _tbc;
}

/**
 *
 */
int tbl_cache_free(tbl_cache_p _tbc)
{
	// FILL IT IN ?????????????
	if(!_tbc)
		return -1;
	lock_get(&_tbc->sem);

	if(_tbc->dtp)
		dbt_table_free(_tbc->dtp);
	
	lock_destroy(&_tbc->sem);
	shm_free(_tbc);
	
	return 0;
}

