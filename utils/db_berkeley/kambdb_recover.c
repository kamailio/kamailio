/*
 * $Id$
 *
 * recovery for berkeley_db module
 * Copyright (C) 2007 Cisco Systems
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
 * 2007-09-19  genesis (wiquan)
 */

#include <unistd.h>
#include "kambdb_recover.h"

tbl_cache_p tables;
char* schema_dir = NULL;
char* db_home = NULL;
const char *progname;

/**
 * main -- 
 */
int main(int argc, char* argv[])
{
	int ret, ch, i;
	
	ret = 0;
	progname = argv[0];
	
	while ((ch = getopt(argc, argv, "s:h:c:C:r:R:")) != EOF)
	switch (ch) 
	{
		case 's':
		schema_dir = optarg;
		load_schema(optarg);
		break;
		
		case 'c': /*create <tablename> */
		ret = create(optarg);
		break;
		
		case 'C': /*Create all*/
		ret = create_all();
		break;
		
		case 'r': /*recover <filename> */
		ret = recover(optarg);
		break;
		
		case 'R': /*recover_all <MAXFILES> */
		ret = sscanf(optarg,"%i", &i);
		if(ret != 1) return -1;
		ret = recover_all(i);
		break;
		
		case 'h':
		db_home = optarg;
		break;
		
		case '?':
		default:
		return(usage());
		
	}
	
	argc -= optind;
	argv += optind;
	
	/*free mem; close open files.*/
	cleanup();
	
	return ret;
}


/**
* usage -- 
* 
*/
int usage(void)
{
	fprintf(stderr, "usage: %s %s\n", progname, 
		"-s schemadir [-h home] [-c tablename]");
	
	fprintf(stderr, "usage: %s %s\n", progname, 
		"-s schemadir [-h home] [-C all]");
	
	fprintf(stderr, "usage: %s %s\n", progname, 
		"-s schemadir [-h home] [-r journal-file]");
	
	fprintf(stderr, "usage: %s %s\n", progname, 
		"-s schemadir [-h home] [-R lastN]");
	
	return (EXIT_FAILURE);
}


/**
 * create -- creates a Berkeley DB file with tablename (tn), along with
 * 	 	the needed metadata.
 * 		requires the schema data to be already parsed '-L' option.
 */
int create(char* tn)
{
	DB* db;
	int rc;
	tbl_cache_p tbc = NULL;
	table_p tp = NULL;
	rc = 0;
	
	tbc = get_table(tn);
	if(!tbc)
	{	fprintf(stderr, "[create] Table %s is not supported.\n",tn);
		return 1;
	}
	
	tp  = tbc->dtp;
	db = get_db(tp);
	
	if(db)
	{
		printf("Created table %s\n",tn);
		rc = 0;
	}
	else
	{
		fprintf(stderr, "[create] Failed to create table %s\n",tn);
		rc = 1;
	}
	
	return rc;
}


/**
 * create_all -- creates a new Berkeley DB table for only the core tables
 */
int create_all(void)
{	
	tbl_cache_p _tbc = tables;
	int rc;
	rc = 0;
	
#ifdef EXTRA_DEBUG
	time_t tim1 = time(NULL);
	time_t tim2;
#endif

	while(_tbc)
	{
		if(_tbc->dtp)
			if((rc = create(_tbc->dtp->name)) != 0 ) 
				break;
		_tbc = _tbc->next;
	}
	
#ifdef EXTRA_DEBUG
	tim2 = time(NULL);
	int i = tim2 - tim1;
	printf("took %i sec\n", i);
#endif

	return rc;
}


/**
 * file_list --
 * returns a sorted linkedlist of all files in d
 *
 * parmameter d is the directory name
 * parameter tn is optional,
 * 	if tablename (tn) is specified returns only jnl files for tablename (tn)
 *	else returns a sorted linkedlist of all files in d
 * returns lnode_p 
 *	the head linknode points to the latests file.
 */
lnode_p file_list(char* d, char* tn)
{
	DIR *dirp;
	int i, j, len;
	char *fn;
	char *list[MAXFILES];
	char dir[MAX_FILENAME_SIZE];
	struct dirent *dp; 
	lnode_p h,n;
	
	h = n = NULL;
	i = j = 0;
	
	if(!d)
	{
		fprintf(stderr, "[file_list]: null path to schema files.\n");
		return NULL;
	}
	
	memset(dir, 0, MAX_FILENAME_SIZE);
	strcpy(dir, d);
	strcat(dir, "/");
	//strcat(dir, ".");
	dirp = opendir(dir);
	
	while ((dp = readdir(dirp)) != NULL)
	{  j=0;
	   if (i> (MAXFILES-1) )
		   continue;
	   
	   fn = dp->d_name;
	   
	   if (fn[0] == '.')
		   continue;
	   
	   if(tn)
	   {
		   /* only looking for jnl files */
		   len = strlen(tn);
		   if (!strstr(fn, ".jnl"))	continue;
		   if (strncmp(fn, tn, len))	continue;
	   }
	   
	   j = strlen(fn) +1;
	   list[i] = malloc(sizeof(char) * j);
	   memset(list[i], 0 , j);
	   strcat(list[i], fn);
	   i++;
	}
	
	closedir(dirp);
	qsort(list, i, sizeof(char*), compare);
	
	for(j=0;j<i;j++)
	{
		n = malloc(sizeof(lnode_t));
		if(!n) return NULL;
		n->prev=NULL;
		n->p = list[j];
		if(h) h->prev = n;
		n->next = h;
		h = n;
	}
	return h;
}


/** qsort C-string comparison function */
int compare (const void *a, const void *b)
{
    const char **ia = (const char **)a;
    const char **ib = (const char **)b;
    return strcmp(*ia, *ib);
}



/**
* recover -- given a journal filename, creates a new db w. metadata, and replays
*	the events in journalized order. 
*	Results in a new db containing the journaled data.
*
*	fn (filename) must be in the form:
*	location-20070803175446.jnl 
*/
int recover(char* jfn)
{

#ifdef EXTRA_DEBUG
	time_t tim1 = time(NULL);
	time_t tim2;
#endif

	int len, i, cs, ci, cd, cu;
	char *v, *s;
	char line [MAX_ROW_SIZE];
	char tn [MAX_TABLENAME_SIZE];
	char fn [MAX_FILENAME_SIZE];
	char op [7]; //INSERT, DELETE, UPDATE are all 7 char wide (w. null)
	FILE * fp = NULL;
	tbl_cache_p tbc = NULL;
	table_p tp = NULL;
	i = 0 ;
	cs = ci = cd = cu = 0;
	
	if(!strstr(jfn, ".jnl"))
	{
		fprintf(stderr, "[recover]: Does NOT look like a journal file: %s.\n", jfn);
		return 1;
	}
	
	if(!db_home)
	{
		fprintf(stderr, "[recover]: null path to db_home.\n");
		return 1;
	}
	
	/*tablename tn*/
	s = strchr(jfn, '-');
	len = s - jfn;
	strncpy(tn, jfn, len);
	tn[len] = 0;
	
	/*create abs path to journal file relative to db_home*/
	memset(fn, 0 , MAX_FILENAME_SIZE);
	strcat(fn, db_home);
	strcat(fn, "/");
	strcat(fn, jfn);
	
	fp = fopen(fn, "r");
	if(!fp) 
	{
		fprintf(stderr, "[recover]: FAILED to load journal file: %s.\n", jfn);
		return 2;
	}
	
	tbc = get_table(tn);
	if(!tbc)
	{
		fprintf(stderr, "[recover]: Table %s is not supported.\n",tn);
		fprintf(stderr, "[recover]: FAILED to load journal file: %s.\n", jfn);
		fclose(fp);
		return 2;
	}
	
	tp  = tbc->dtp;
	
	if(!tbc || !tp)
	{
		fprintf(stderr, "[recover]: FAILED to get find metadata for : %s.\n", tn);
		fclose(fp);
		return 3;
	}
	
	while ( fgets(line , MAX_ROW_SIZE, fp) != NULL )
	{
		len = strlen(line);
		if(line[0] == '#' || line[0] == '\n') continue;
		
		if(len > 0) line[len-1] = 0; /*chomp trailing \n */
		
		v = strchr(line, '|');
		len = v - line;
		
		strncpy(op, line, len);
		op[len] = 0;
		
		switch( get_op(op, len) )
		{
		case INSERT:
			v++; //now v points to data
			len = strlen(v);
			insert(tp, v, len);
			ci++;
			break;
			
		case UPDATE:
			v++;
			len = strlen(v);
			update(tp, v, len);
			cu++;
			break;
			
		case DELETE:
			//v is really the key
			delete(tp, v, len);
			cd++;
			break;
			
		case UNKNOWN_OP:
			fprintf(stderr,"[recover]: UnknownOP - Skipping ROW: %s\n",line);
			cs++;
			continue;
		}
		i++;
	}
	
#ifdef EXTRA_DEBUG
	printf("Processed journal file: %s.\n", jfn);
	printf("INSERT   %i records.\n",ci);
	printf("UPDATE   %i records.\n",cu);
	printf("DELETE   %i records.\n",cd);
	printf("SKIPed   %i records.\n",cs);
	printf("------------------------\n");
	printf("Total    %i records.\n",i);
	
	tim2 = time(NULL);
	i = tim2 - tim1;
	printf("took %i sec\n", i);
#endif
	
	fclose(fp);
	
	return 0;
}

/**
* recover_all -- Iterates over all core tables in enumerated order for recovery from 
*	journal files (.jnl).
*	The parm 'lastn' is the number of journal files needed to be recovered.
*	Hardcoded to only find MAXFILES.
*
* 	e.g.
*	25 journal files are present for the 'acc' table, however you only
* 	want to restore the latest 3; so lastn=3.
*/
int recover_all(int lastn)
{
	lnode_p n, h;
	tbl_cache_p _tbc = tables;
	
	if(MAXFILES < lastn) return 1;

	if(!schema_dir)
	{
		fprintf(stderr, "[recover_all]: null path to schema files.\n");
		return 1;
	}

	if(!db_home)
	{
		fprintf(stderr, "[recover_all]: null path to db_home.\n");
		return 1;
	}
	
	while(_tbc)
	{	
		int j;
		
		if(_tbc->dtp)
			h = file_list(db_home, _tbc->dtp->name);
		n = h;
		
		/*lastn; move to the oldest of the N*/
		for(j=1;j<lastn;j++)
			if(n && (n->next != NULL) )
				n = n->next;
		while(n)
		{	printf("[recover_all] recovering file: %s\n",n->p);
			if(recover(n->p))
				fprintf(stderr, "[recover_all]: Error while recovering: [%s]\n. Continuing..\n",n->p);
			n = n->prev;
		}
		
		while(h) /*free mem*/
		{	n = h->next;
			free(h->p);
			free(h);
			h = n;
		}
		
		_tbc = _tbc->next;
	}
	
	return 0;
}


/**
* extract_key -- uses the internal schema to extract the key from the data
* 	row that was found in the journal.
* caller provides inititialize memory for destination key (k).
* data is provided ; key is filled in 
*/
int extract_key(table_p tp, char* k, char* d)
{
	char *s, *p;
	char buf[MAX_ROW_SIZE];
	int n, len;
	
	if(!tp || !k || !d) return  -1;
	len=n=0;
	p = k;
	
	/*copy data so we can tokenize w.o trampling */
	len = strlen(d);
	strncpy(buf, d, len);
	buf[len] = 0;
	
	s = strtok(buf, "|"); 
	while(s!=NULL && n<MAX_NUM_COLS) 
	{
		len = strlen(s);
		if( (tp->ncols-1) > n)
		{
			if( tp->colp[n]->kflag )
			{
				strncpy(p, s, len);
				p+=len;
				
				*p = '|';
				p++;
			}
		}
		
		s=strtok(NULL, "|");
		n++;
	}
	
	*p = 0;
	return 0;
}

/**
* delete -- deletes a row from the db we are trying to rebuild 
*/
int delete(table_p tp, char* k, int len)
{
	DBT key;
	DB *db;
	
	if(!tp || !k)	return 1;
	if((db = get_db(tp)) == NULL)	return 2;
	
	memset(&key, 0, sizeof(DBT));
	key.data = k;
	key.ulen = MAX_ROW_SIZE;
	key.size = len;
	
	if ( db->del(db, NULL, &key, 0))
	{
		fprintf(stderr, "[delete] FAILED --> [%.*s]  \n", len, k);
		return 3;
	}
	
	return 0;
}


/**
* _insert -- inserts a new row in to the db we are trying to rebuild
* 	I needed this to directly insert metadata when the db is created.
*/
int _insert(DB* db, char* k, char* v, int klen, int vlen)
{
	DBT key, data;
	
	if(!db || !k || !v) 	return 1;
	
	memset(&key,  0, sizeof(DBT));
	key.data = k;
	key.ulen = MAX_ROW_SIZE;
	key.size = klen;
	
	memset(&data, 0, sizeof(DBT));
	data.data = v;
	data.ulen = MAX_ROW_SIZE;
	data.size = vlen;
	if (db->put(db, NULL, &key, &data, 0))
	{
		fprintf(stderr, "[insert] FAILED --> [%.*s]  \n", vlen, v);
		return 1;
	}
	return 0;
}


/**
* insert -- given the data row (v) and its length (vlen), we build the corresponding
* 	key, and insert the data in to the db.
*	This will over-right the value if already present.
*/
int insert(table_p tp, char* v, int vlen)
{
	char k[MAX_ROW_SIZE];
	int rc, klen;
	DB *db;
	
	if(!tp || !v) 	return 1;
	if((db = get_db(tp)) == NULL)	return 2;
	
	memset(k,0,MAX_ROW_SIZE);
	if( extract_key(tp, k, v) )
	{
		fprintf(stderr, "[insert] failed to extract key for row: %.*s",vlen, v);
		return 2;
	}
	
	klen = strlen(k);
	rc = _insert(db, k, v, klen, vlen);
	
	return rc;
}


/**
* update -- given the data row (v) and its length (vlen), we build the corresponding
* 	key, and update the data in the db.
*	This is implemented as DELETE + INSERT.
*/
int update(table_p tp, char* v, int len)
{
	char k[MAX_ROW_SIZE];
	
	if(!tp || !v)	return 1;
	
	memset(k,0,MAX_ROW_SIZE);
	if( extract_key(tp, k, v) )
	{
		fprintf(stderr, "[update] failed to extract key for row: %.*s",len, v);
		return 2;
	}
	
/*	if( delete(tp, k, strlen(k)) )	return 3;  */
	if( insert(tp, v, len) )	return 4;
	return 0;
}



/**
* get_op -- used to convert the string operation name to an enumerated op
*/
int get_op(char* op, int len)
{
	if((len==6) && strstr("INSERT",op) )	return INSERT;
	if((len==6) && strstr("UPDATE",op) )	return UPDATE;
	if((len==6) && strstr("DELETE",op) )	return DELETE;
	
	return UNKNOWN_OP;
}


/**
* load_schema -- sets up the internal representation of the schema.
*/
int load_schema(char* d)
{	int rc;
	char *tn;
	char line1 [MAX_ROW_SIZE];
	char line2 [MAX_ROW_SIZE];
	char fn [MAX_FILENAME_SIZE];
	tbl_cache_p tbc = NULL;
	table_p tp = NULL;
	FILE * fp = NULL;
	lnode_p h,n;
	
	rc=0;
	h = n = NULL;
	
	if(!d)
	{
		fprintf(stderr, "[load_schema]: null path to schema files.\n");
		return 1;
	}
	
	tables = (tbl_cache_p)malloc(sizeof(tbl_cache_t));
	if(!tables)	return 1;
	
	h = file_list(d, NULL);
	
	while(h)
	{
		n = h->next;
		
		/*create abs path to journal file (relative to db_home) */
		memset(fn, 0 , MAX_FILENAME_SIZE);
		strcat(fn, d);
		strcat(fn, "/");
		strcat(fn, h->p);
		
		fp = fopen(fn, "r");
		if(!fp)
		{
			fprintf(stderr, "[load_schema]: FAILED to load schema file: %s.\n", h->p);
			break;
		}
		
		tn = h->p;
		tbc = get_table(tn);
		if(!tbc)
		{
			fprintf(stderr, "[load_schema]: Table %s is not supported.\n",tn);
			fprintf(stderr, "[load_schema]: FAILED to load data for table: %s.\n", tn);
			goto done;
		}
		
		tp = tbc->dtp;
		
		while ( fgets(line1 , MAX_ROW_SIZE, fp) != NULL )
		{
			if ( fgets(line2 , MAX_ROW_SIZE, fp) != NULL )
			{
				if(strstr(line1, METADATA_COLUMNS))
				{
					if(0!=load_metadata_columns(tp, line2))
					{
						fprintf(stderr, "[load_schema]: FAILED to load METADATA COLS in table: %s.\n", tn);
						goto done;
					}
				}
				
				if(strstr(line1, METADATA_KEY))
				{
					if(0!=load_metadata_key(tp, line2))
					{
						fprintf(stderr, "[load_schema]: FAILED to load METADATA KEYS in table: %s.\n", tn);
						goto done;
					}
				}
			}
			else
			{
				fprintf(stderr, "[load_schema]: FAILED to read schema value in table: %s.\n", tn);
				goto done;
			}
			
		}
done:		
		fclose(fp);
		h = n;
	}
	
	while(h) /*free mem*/
	{	n = h->next;
		free(h->p);
		free(h);
		h = n;
	}
	
	return rc;
}



/**
* get_table -- return pointer to lazy initialized table struct
*/
tbl_cache_p get_table(char *_s)
{
	tbl_cache_p _tbc = tables;
	table_p _tp = NULL;

	while(_tbc)
	{
		if(_tbc->dtp)
		{
			if(_tbc->dtp->name
			&& !strcmp(_tbc->dtp->name,_s))
			{
				return _tbc;
			}
		}
		_tbc = _tbc->next;
	}
	
	_tbc = (tbl_cache_p)malloc(sizeof(tbl_cache_t));
	if(!_tbc)
		return NULL;
	
	_tp = create_table(_s);
	
	if(!_tp)
	{
		fprintf(stderr, "[get_table]: failed to create table.\n");
		free(_tbc);
		return NULL;
	}
	
	_tbc->dtp = _tp;
	
	if(tables)
		(tables)->prev = _tbc;
	
	_tbc->next = tables;
	tables = _tbc;

	return _tbc;
}


/**
* create_table -- returns an initialed table struct
*/
table_p create_table(char *_s)
{
	int i;
	table_p tp = NULL;
	
	tp = (table_p)malloc(sizeof(table_t));
	if(!tp)	return NULL;
	
	i=strlen(_s)+1;
	tp->name = (char*)malloc(i*sizeof(char));
	strncpy(tp->name, _s, i);
	
	tp->ncols=0;
	tp->nkeys=0;
	tp->ro=0;
	tp->logflags=0;
	tp->db = NULL;
	
	for(i=0;i<MAX_NUM_COLS;i++)
		tp->colp[i] = NULL;
	
	return tp;
}


/**
* load_metadata_columns -- parses the METADATA_COLUMNS line into the internal
* 	representation.
*/
int load_metadata_columns(table_p _tp, char* line)
{
	int ret,n,len;
	char *s = NULL;
	char cn[64], ct[16];
	column_p col;
	ret = n = len = 0;
	
	if(!_tp) return -1;
	if(_tp->ncols!=0) return 0;
	
	/* eg: line = "table_name(str) table_version(int)" */
	s = strtok(line, " \t");
	while(s!=NULL && n<MAX_NUM_COLS) 
	{
		/* eg: meta[0]=table_name  meta[1]=str */
		sscanf(s,"%20[^(](%10[^)])[^\n]", cn, ct);
		
		/* create column*/
		col = (column_p) malloc(sizeof(column_t));
		if(!col)
		{	fprintf(stderr, "load_metadata_columns: out of memory \n");
			return -1;
		}
		
		/* set name*/
		len = strlen( cn )+1;
		col->name = (char*)malloc(len * sizeof(char));
		strcpy(col->name, cn );
		
		/* set type*/
		len = strlen( ct )+1;
		col->type = (char*)malloc(len * sizeof(char));
		strcpy(col->type, ct );
		
		_tp->colp[n] = col;
		n++;
		_tp->ncols++;
		s=strtok(NULL, " \t");
	}

	return 0;
}


/**
* load_metadata_key -- parses the METADATA_KEY line into the internal
* 	representation.
*/
int load_metadata_key(table_p _tp, char* line)
{
	int ret,n,ci;
	char *s = NULL;
	ret = n = ci = 0;
	
	if(!_tp)return -1;
	
	s = strtok(line, " \t");
	while(s!=NULL && n< _tp->ncols) 
	{
		ret = sscanf(s,"%i", &ci);
		if(ret != 1) return -1;
		if( _tp->colp[ci] ) 
		{	_tp->colp[ci]->kflag = 1;
			_tp->nkeys++;
		}
		n++;
		s=strtok(NULL, " ");
	}
	
	return 0;
}


/**
* get_db -- lazy initialized DB access
*	Its like this so we get new db files only for the tables that have
*	journal files.
*	The db file on disk will be named:
*		<tablename>.new
*/
DB* get_db(table_p tp)
{
	int rc;
	DB* db;
	char dfn[MAX_FILENAME_SIZE];
	
	if( !tp) 	return NULL;
	if( tp->db) 	return tp->db;
	
	memset(dfn, 0, MAX_FILENAME_SIZE);
	if(db_home)
	{
		strcpy(dfn, db_home);
		strcat(dfn, "/");
	}
	
	/*creation of DB follows*/
	strcat(dfn, tp->name);
	
	if ((rc = db_create(&db, NULL, 0)) != 0)
	{ 
		fprintf(stderr, "[create_table]: error db_create for table: %s.\n",dfn);
		return NULL;
	}
	
	if ((rc = db->open(db, NULL, dfn, NULL, DB_HASH, DB_CREATE, 0664)) != 0)
	{ 
		fprintf(stderr, "[create_table]: error opening %s.\n",dfn);
		fprintf(stderr, "[create_table]: error msg: %s.\n",db_strerror(rc));
		return NULL;
	}
	tp->db = db;
	
	import_schema(tp);
	
	return db;
}


/**
*/
int import_schema(table_p tp)
{
	int rc, len1, len2;
	char line1 [MAX_ROW_SIZE];
	char line2 [MAX_ROW_SIZE];
	char fn [MAX_FILENAME_SIZE];
	FILE * fp = NULL;
	rc = 0;
	
	if(!schema_dir)
	{
		fprintf(stderr, "[import_schema]: null schema dir.\n");
		return 1;
	}
	
	if(!tp)
	{
		fprintf(stderr, "[import_schema]: null table parameter.\n");
		return 1;
	}
	
	/*create abs path to journal file (relative to db_home) */
	memset(fn, 0 , MAX_FILENAME_SIZE);
	strcat(fn, schema_dir);
	strcat(fn, "/");
	strcat(fn, tp->name);
	
	fp = fopen(fn, "r");
	if(!fp)
	{
		fprintf(stderr, "[import_schema]: FAILED to open def schema file: %s.\n", fn);
		return 1;
	}

	while ( fgets(line1 , MAX_ROW_SIZE, fp) != NULL )
	{
		if ( fgets(line2 , MAX_ROW_SIZE, fp) != NULL )
		{
			len1 = strlen(line1)-1;
			len2 = strlen(line2)-1;
			line1[len1] = 0;
			line2[len2] = 0;
			
			if((rc = _insert(tp->db, line1, line2, len1, len2) )!=0)
			{
				fprintf(stderr, "[import_schema]: FAILED to write schema def into table: %s.\n", tp->name);
				goto done;
			}
		}
		else
		{
			fprintf(stderr, "[import_schema]: FAILED to read schema def value in table: %s.\n", tp->name);
			goto done;
		}
		
	}
done:		
	fclose(fp);
	return rc;
}



/**
* cleanup -- frees memory; closes any files.
*/
void cleanup(void)
{
	//cleanup
	while(tables)
	{	int i;
		tbl_cache_p n   = tables->next;
		table_p     tp  = tables->dtp;
		if(tp)
		{
			free(tp->name);
			for(i=0;i< tp->ncols;i++)
			{
				free(tp->colp[i]->name);
				free(tp->colp[i]->type);
				free(tp->colp[i]);
			}
			
			if(tp->db)
				tp->db->close(tp->db, 0);
			
			free(tp);
		}
		free(tables);
		tables = n;
	}
}

