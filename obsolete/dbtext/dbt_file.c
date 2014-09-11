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
 * 2003-02-03 created by Daniel
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


/**
 *
 */
dbt_table_p dbt_load_file(str *tbn, str *dbn)
{
	FILE *fin=NULL;
	char path[512], buf[4096];
	int c, crow, ccol, bp, sign, max_auto;
	dbt_val_t dtval;
	dbt_table_p dtp = NULL;
	dbt_column_p colp, colp0 = NULL;
	dbt_row_p rowp, rowp0 = NULL;
		
	enum {DBT_FLINE_ST, DBT_NLINE_ST, DBT_DATA_ST} state;
	
	DBG("DBT:dbt_load_file: request for table [%.*s]\n", tbn->len, tbn->s);
	
	if(!tbn || !tbn->s || tbn->len<=0 || tbn->len>=255)
		return NULL;
	path[0] = 0;
	if(dbn && dbn->s && dbn->len>0)
	{
		DBG("DBT:dbt_load_file: db is [%.*s]\n", dbn->len, dbn->s);
		if(dbn->len+tbn->len<511)
		{
			strncpy(path, dbn->s, dbn->len);
			path[dbn->len] = '/';
			strncpy(path+dbn->len+1, tbn->s, tbn->len);
			path[dbn->len+tbn->len+1] = 0;
		}
	}
	if(path[0] == 0)
	{
		strncpy(path, tbn->s, tbn->len);
		path[tbn->len] = 0;
	}
	
	DBG("DBT:dbt_load_file: loading file [%s]\n", path);
	fin = fopen(path, "rt");
	if(!fin)
		return NULL;	
	
	dtp = dbt_table_new(tbn->s, tbn->len);
	if(!dtp)
		goto done;
	
	state = DBT_FLINE_ST;
	crow = ccol = -1;
	colp = colp0 = NULL;
	rowp = rowp0 = NULL;
	c = fgetc(fin);
	max_auto = 0;
	while(c!=EOF)
	{
		switch(state)
		{
			case DBT_FLINE_ST:
				//DBG("DBT:dbt_load_file: state FLINE!\n");
				bp = 0;
				while(c==DBT_DELIM_C)
					c = fgetc(fin);
				if(c==DBT_DELIM_R && !colp0)
					goto clean;
				if(c==DBT_DELIM_R)
				{
					if(dtp->nrcols <= 0)
						goto clean;
					dtp->colv = (dbt_column_p*)
							shm_malloc(dtp->nrcols*sizeof(dbt_column_p));
					if(!dtp->colv)
						goto clean;
					colp0 = dtp->cols;
					for(ccol=0; ccol<dtp->nrcols && colp0; ccol++)
					{
						dtp->colv[ccol] = colp0;
						colp0 = colp0->next;
					}
					state = DBT_NLINE_ST;
					break;
				}
				while(c!=DBT_DELIM_C && c!='(' && c!=DBT_DELIM_R)
				{
					if(c==EOF)
						goto clean;
					buf[bp++] = c;
					c = fgetc(fin);
				}
				colp = dbt_column_new(buf, bp);
				if(!colp)
					goto clean;
				//DBG("DBT:dbt_load_file: new col [%.*s]\n", bp, buf);
				while(c==DBT_DELIM_C)
					c = fgetc(fin);
				if(c!='(')
					goto clean;
				c = fgetc(fin);
				while(c==DBT_DELIM_C)
					c = fgetc(fin);
				
				switch(c)
				{
					case 's':
					case 'S':
						colp->type = DB_STR;
						DBG("DBT: column[%d] is STR!\n", ccol+1);
					break;
					case 'i':
					case 'I':
						colp->type = DB_INT;
						DBG("DBT: column[%d] is INT!\n", ccol+1);
					break;
				        case 'f':
				        case 'F':
						colp->type = DB_FLOAT;
						DBG("DBT: column[%d] is FLOAT!\n", ccol+1);
					break;
					case 'd':
					case 'D':
						colp->type = DB_DOUBLE;
						DBG("DBT: column[%d] is DOUBLE!\n", ccol+1);
					break;
					default:
						DBG("DBT: wrong column type!\n");
						goto clean;
				}

				while(c!=')' && c!= ',')
					c = fgetc(fin);
				if(c==',')
				{
					//DBG("DBT: c=%c!\n", c);
					c = fgetc(fin);
					while(c==DBT_DELIM_C)
						c = fgetc(fin);
					if(c=='N' || c=='n')
					{
						//DBG("DBT:dbt_load_file: NULL flag set!\n");
						colp->flag |= DBT_FLAG_NULL;
					}
					else if(colp->type==DB_INT && dtp->auto_col<0
							&& (c=='A' || c=='a'))
					{
						//DBG("DBT:dbt_load_file: AUTO flag set!\n");
						colp->flag |= DBT_FLAG_AUTO;
						dtp->auto_col = ccol+1;
					}
					else
						goto clean;
					while(c!=')' && c!=DBT_DELIM_R && c!=EOF)
						c = fgetc(fin);
				}
				if(c == ')')
				{
					//DBG("DBT: c=%c!\n", c);
					if(colp0)
					{
						colp->prev = colp0;
						colp0->next = colp;
					}
					else
						dtp->cols = colp;
					colp0 = colp;
					dtp->nrcols++;
					c = fgetc(fin);
				}
				else
					goto clean;
				ccol++;
			break;

			case DBT_NLINE_ST:
				//DBG("DBT:dbt_load_file: state NLINE!\n");
				while(c==DBT_DELIM_R)
					c = fgetc(fin);
				if(rowp)
				{
					if(dbt_table_check_row(dtp, rowp))
						goto clean;

					if(!rowp0)
						dtp->rows = rowp;
					else
					{
						rowp0->next = rowp;
						rowp->prev = rowp0;
					}
					rowp0 = rowp;
					dtp->nrrows++;
				}
				if(c==EOF)
					break;
				crow++;
				ccol = 0;
				rowp = dbt_row_new(dtp->nrcols);
				if(!rowp)
					goto clean;
				state = DBT_DATA_ST;
				
			break;
			
			case DBT_DATA_ST:
				//DBG("DBT:dbt_load_file: state DATA!\n");
				//while(c==DBT_DELIM)
				//	c = fgetc(fin);
				if(ccol == dtp->nrcols && (c==DBT_DELIM_R || c==EOF))
				{
					state = DBT_NLINE_ST;
					break;
				}
				if(ccol>= dtp->nrcols)
					goto clean;
				
				switch(dtp->colv[ccol]->type)
				{
					case DB_INT:
						//DBG("DBT:dbt_load_file: INT value!\n");
						dtval.val.int_val = 0;
						dtval.type = DB_INT;

						if(c==DBT_DELIM || 
								(ccol==dtp->nrcols-1
								 && (c==DBT_DELIM_R || c==EOF)))
							dtval.nul = 1;
						else
						{
							dtval.nul = 0;
							sign = 1;
							if(c=='-')
							{
								sign = -1;
								c = fgetc(fin);
							}
							if(c<'0' || c>'9')
								goto clean;
							while(c>='0' && c<='9')
							{
								dtval.val.int_val=dtval.val.int_val*10+c-'0';
								c = fgetc(fin);
							}
							dtval.val.int_val *= sign;
							//DBG("DBT:dbt_load_file: data[%d,%d]=%d\n", crow,
							//	ccol, dtval.val.int_val);
						}
						if(c!=DBT_DELIM && c!=DBT_DELIM_R && c!=EOF)
							goto clean;
						if(dbt_row_set_val(rowp,&dtval,DB_INT,ccol))
							goto clean;
						if(ccol == dtp->auto_col)
							max_auto = (max_auto<dtval.val.int_val)?
									dtval.val.int_val:max_auto;
					break;
					
					case DB_FLOAT:
						//DBG("DBT:dbt_load_file: FLOAT value!\n");
						dtval.val.float_val = 0.0;
						dtval.type = DB_FLOAT;

						if(c==DBT_DELIM || 
								(ccol==dtp->nrcols-1
								 && (c==DBT_DELIM_R || c==EOF)))
							dtval.nul = 1;
						else
						{
							dtval.nul = 0;
							sign = 1;
							if(c=='-')
							{
								sign = -1;
								c = fgetc(fin);
							}
							if(c<'0' || c>'9')
								goto clean;
							while(c>='0' && c<='9')
							{
								dtval.val.float_val = dtval.val.float_val*10
										+ c - '0';
								c = fgetc(fin);
							}
							if(c=='.')
							{
								c = fgetc(fin);
								bp = 1;
								while(c>='0' && c<='9')
								{
									bp *= 10;
									dtval.val.float_val+=((float)(c-'0'))/bp;
									c = fgetc(fin);
								}
							}
							dtval.val.float_val *= sign;
							//DBG("DBT:dbt_load_file: data[%d,%d]=%10.2f\n",
							//	crow, ccol, dtval.val.float_val);
						}
						if(c!=DBT_DELIM && c!=DBT_DELIM_R && c!=EOF)
							goto clean;
						if(dbt_row_set_val(rowp,&dtval,DB_FLOAT,ccol))
							goto clean;
					break;

					case DB_DOUBLE:
						//DBG("DBT:dbt_load_file: DOUBLE value!\n");
						dtval.val.double_val = 0.0;
						dtval.type = DB_DOUBLE;

						if(c==DBT_DELIM || 
								(ccol==dtp->nrcols-1
								 && (c==DBT_DELIM_R || c==EOF)))
							dtval.nul = 1;
						else
						{
							dtval.nul = 0;
							sign = 1;
							if(c=='-')
							{
								sign = -1;
								c = fgetc(fin);
							}
							if(c<'0' || c>'9')
								goto clean;
							while(c>='0' && c<='9')
							{
								dtval.val.double_val = dtval.val.double_val*10
										+ c - '0';
								c = fgetc(fin);
							}
							if(c=='.')
							{
								c = fgetc(fin);
								bp = 1;
								while(c>='0' && c<='9')
								{
									bp *= 10;
									dtval.val.double_val+=((double)(c-'0'))/bp;
									c = fgetc(fin);
								}
							}
							dtval.val.double_val *= sign;
							//DBG("DBT:dbt_load_file: data[%d,%d]=%10.2f\n",
							//	crow, ccol, dtval.val.double_val);
						}
						if(c!=DBT_DELIM && c!=DBT_DELIM_R && c!=EOF)
							goto clean;
						if(dbt_row_set_val(rowp,&dtval,DB_DOUBLE,ccol))
							goto clean;
					break;
					
					case DB_STR:
						//DBG("DBT:dbt_load_file: STR value!\n");
						
						dtval.val.str_val.s = NULL;
						dtval.val.str_val.len = 0;
						dtval.type = DB_STR;
						
						bp = 0;
						if(c==DBT_DELIM || 
								(ccol == dtp->nrcols-1
								 && (c == DBT_DELIM_R || c==EOF)))
							dtval.nul = 1;
						else
						{
							dtval.nul = 0;
							while(c!=DBT_DELIM && c!=DBT_DELIM_R && c!=EOF)
							{
								if(c=='\\')
								{
									c = fgetc(fin);
									switch(c)
									{
										case 'n':
											c = '\n';	
										break;
										case 'r':
											c = '\r';
										break;
										case 't':
											c = '\t';
										break;
										case '\\':
											c = '\\';
										break;
										case DBT_DELIM:
											c = DBT_DELIM;
										break;
										case '0':
											c = 0;
										break;
										default:
											goto clean;
									}
								}
								buf[bp++] = c;
								c = fgetc(fin);
							}
							dtval.val.str_val.s = buf;
							dtval.val.str_val.len = bp;
							//DBG("DBT:dbt_load_file: data[%d,%d]=%.*s\n",
							///	crow, ccol, bp, buf);
						}
						if(c!=DBT_DELIM && c!=DBT_DELIM_R && c!=EOF)
							goto clean;
						if(dbt_row_set_val(rowp,&dtval,DB_STR,ccol))
							goto clean;
					break;
					default:
						goto clean;
				}
				if(c==DBT_DELIM)
					c = fgetc(fin);
				ccol++;
			break; // state DBT_DATA_ST
		}
	}

	if(max_auto)
		dtp->auto_val = max_auto;

done:
	if(fin)
		fclose(fin);
	return dtp;
clean:
	/// ????? FILL IT IN - incomplete row/column
	// memory leak?!?! with last incomplete row
	DBG("DBT:dbt_load_file: error at row=%d col=%d c=%c\n", crow+1, ccol+1, c);
	if(dtp)
		dbt_table_free(dtp);
	return NULL;
}


/**
 *
 */
int dbt_print_table(dbt_table_p _dtp, str *_dbn)
{
	dbt_column_p colp = NULL;
	dbt_row_p rowp = NULL;
	FILE *fout = NULL;
	int ccol;
	char *p, path[512];
	
	if(!_dtp || !_dtp->name.s || _dtp->name.len <= 0)
		return -1;

	if(!_dbn || !_dbn->s || _dbn->len <= 0)
	{
		fout = stdout;
		fprintf(fout, "\n Content of [%.*s]\n", _dtp->name.len, _dtp->name.s);
	}
	else
	{
		if(_dtp->name.len+_dbn->len > 510)
			return -1;
		strncpy(path, _dbn->s, _dbn->len);
		path[_dbn->len] = '/';
		strncpy(path+_dbn->len+1, _dtp->name.s, _dtp->name.len);
		path[_dbn->len+_dtp->name.len+1] = 0;
		fout = fopen(path, "wt");
		if(!fout)
			return -1;	
	}
	
	colp = _dtp->cols;
	while(colp)
	{
		switch(colp->type)
		{
			case DB_INT:
				fprintf(fout, "%.*s(int", colp->name.len, colp->name.s);
			break;
			case DB_FLOAT:
				fprintf(fout, "%.*s(float", colp->name.len, colp->name.s);
			break;
			case DB_DOUBLE:
				fprintf(fout, "%.*s(double", colp->name.len, colp->name.s);
			break;
			case DB_STR:
				fprintf(fout, "%.*s(str", colp->name.len, colp->name.s);
			break;
			default:
				if(fout!=stdout)
					fclose(fout);
				return -1;
		}
		
		if(colp->flag & DBT_FLAG_NULL)
				fprintf(fout,",null");
		else if(colp->type==DB_INT && colp->flag & DBT_FLAG_AUTO)
					fprintf(fout,",auto");
		fprintf(fout,")");
		
		colp = colp->next;
		if(colp)
			fprintf(fout,"%c", DBT_DELIM_C);
	}
	fprintf(fout, "%c", DBT_DELIM_R);
	rowp = _dtp->rows;
	while(rowp)
	{
		for(ccol=0; ccol<_dtp->nrcols; ccol++)
		{
			switch(_dtp->colv[ccol]->type)
			{
				case DB_INT:
					if(!rowp->fields[ccol].nul)
						fprintf(fout,"%d",
								rowp->fields[ccol].val.int_val);
				break;
				case DB_FLOAT:
					if(!rowp->fields[ccol].nul)
						fprintf(fout, "%.2f",
								rowp->fields[ccol].val.float_val);
				break;
				case DB_DOUBLE:
					if(!rowp->fields[ccol].nul)
						fprintf(fout, "%.2f",
								rowp->fields[ccol].val.double_val);
				break;
				case DB_STR:
					if(!rowp->fields[ccol].nul)
					{
						p = rowp->fields[ccol].val.str_val.s;
						while(p < rowp->fields[ccol].val.str_val.s
								+ rowp->fields[ccol].val.str_val.len)
						{
							switch(*p)
							{
								case '\n':
									fprintf(fout, "\\n");
								break;
								case '\r':
									fprintf(fout, "\\r");
								break;
								case '\t':
									fprintf(fout, "\\t");
								break;
								case '\\':
									fprintf(fout, "\\\\");
								break;
								case DBT_DELIM:
									fprintf(fout, "\\%c", DBT_DELIM);
								break;
								case '\0':
									fprintf(fout, "\\0");
								break;
								default:
									fprintf(fout, "%c", *p);
							}
							p++;
						}
					}
				break;
				default:
					if(fout!=stdout)
						fclose(fout);
					return -1;
			}
			if(ccol<_dtp->nrcols-1)
				fprintf(fout, "%c",DBT_DELIM);
		}
		fprintf(fout, "%c", DBT_DELIM_R);
		rowp = rowp->next;
	}
	
	if(fout!=stdout)
		fclose(fout);
	
	return 0;
}

