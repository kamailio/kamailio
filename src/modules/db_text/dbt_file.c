/*
 * DBText library
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "../../core/mem/shm_mem.h"
#include "../../core/mem/mem.h"
#include "../../core/dprint.h"

#include "dbt_util.h"
#include "dbt_lib.h"


/**
 * -1 - error
 *  0 - no change
 *  1 - changed
 */
int dbt_check_mtime(const str *tbn, const str *dbn, time_t *mt)
{
	char path[512];
	struct stat s;
	int ret = 0;

	path[0] = 0;
	if(dbn && dbn->s && dbn->len>0)
	{
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
	if(stat(path, &s) == 0)
	{
		if((int)s.st_mtime > (int)*mt)
		{
			ret = 1;
			*mt = s.st_mtime;
			LM_DBG("[%.*s] was updated\n", tbn->len, tbn->s);
		}
	} else {
		LM_DBG("stat failed on [%.*s]\n", tbn->len, tbn->s);
		ret = -1;
	}
	return ret;
}

/**
 *
 */
dbt_table_p dbt_load_file(const str *tbn, const str *dbn)
{
	FILE *fin=NULL;
	char path[512];
	char *buf;
	int c, crow, ccol, bp, sign, max_auto;
	dbt_val_t dtval;
	dbt_table_p dtp = NULL;
	dbt_column_p colp, colp0 = NULL;
	dbt_row_p rowp, rowp0 = NULL;

	enum {DBT_FLINE_ST, DBT_NLINE_ST, DBT_DATA_ST} state;

	if(!tbn || !tbn->s || tbn->len<=0 || tbn->len>=255)
		return NULL;

	LM_DBG("request for table [%.*s] (len: %d)\n", tbn->len, tbn->s, tbn->len);

	path[0] = 0;
	if(dbn && dbn->s && dbn->len>0)
	{
		LM_DBG("db is [%.*s] (len: %d)\n", dbn->len, dbn->s, dbn->len);
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

	LM_DBG("loading file [%s]\n", path);
	fin = fopen(path, "rt");
	if(!fin) {
		LM_ERR("failed to open file [%s]\n", path);
		return NULL;
	}

	buf = pkg_malloc(_db_text_read_buffer_size);
	if(!buf) {
		LM_ERR("error allocating read buffer, %i\n", _db_text_read_buffer_size);
		goto done;
	}

	dtp = dbt_table_new(tbn, dbn, path);
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
				//LM_DBG("state FLINE!\n");
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
				//LM_DBG("new col [%.*s]\n", bp, buf);
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
						colp->type = DB1_STR;
						LM_DBG("column[%d] is STR!\n", ccol+1);
					break;
					case 'i':
					case 'I':
						colp->type = DB1_INT;
						LM_DBG("column[%d] is INT!\n", ccol+1);
					break;
					case 'd':
					case 'D':
						colp->type = DB1_DOUBLE;
						LM_DBG("column[%d] is DOUBLE!\n", ccol+1);
					break;
					case 'b':
					case 'B':
						colp->type = DB1_BLOB;
						LM_DBG("column[%d] is BLOB!\n", ccol+1);
					break;
					case 't':
					case 'T':
						colp->type = DB1_DATETIME;
						LM_DBG("column[%d] is TIME!\n", ccol+1);
					break;
					default:
						LM_DBG("wrong column type!\n");
						goto clean;
				}

				while(c!='\n' && c!=EOF && c!=')' && c!= ',')
				{
					if(colp->type == DB1_STR && (c=='i'|| c=='I'))
					{
						colp->type = DB1_STRING;
						LM_DBG("column[%d] is actually STRING!\n", ccol+1);
					}
					c = fgetc(fin);
				}
				if(c==',')
				{
					//LM_DBG("c=%c!\n", c);
					c = fgetc(fin);
					while(c==DBT_DELIM_C)
						c = fgetc(fin);
					if(c=='N' || c=='n')
					{
						LM_DBG("column[%d] NULL flag set!\n", ccol+1);
						colp->flag |= DBT_FLAG_NULL;
					}
					else if(colp->type==DB1_INT && dtp->auto_col<0
							&& (c=='A' || c=='a'))
					{
						//LM_DBG("AUTO flag set!\n");
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
					//LM_DBG("c=%c!\n", c);
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
				//LM_DBG("state NLINE!\n");
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
				//LM_DBG("state DATA!\n");
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
					case DB1_INT:
					case DB1_DATETIME:
						//LM_DBG("INT value!\n");
						dtval.val.int_val = 0;
						dtval.type = dtp->colv[ccol]->type;

						if(c==_dbt_delim ||
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
							//LM_DBG("data[%d,%d]=%d\n", crow,
							//	ccol, dtval.val.int_val);
						}
						if(c!=_dbt_delim && c!=DBT_DELIM_R && c!=EOF)
							goto clean;
						if(dbt_row_set_val(rowp,&dtval,dtp->colv[ccol]->type,
									ccol))
							goto clean;
						if(ccol == dtp->auto_col)
							max_auto = (max_auto<dtval.val.int_val)?
									dtval.val.int_val:max_auto;
					break;

					case DB1_DOUBLE:
						//LM_DBG("DOUBLE value!\n");
						dtval.val.double_val = 0.0;
						dtval.type = DB1_DOUBLE;

						if(c==_dbt_delim ||
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
							//LM_DBG("data[%d,%d]=%10.2f\n",
							//	crow, ccol, dtval.val.double_val);
						}
						if(c!=_dbt_delim && c!=DBT_DELIM_R && c!=EOF)
							goto clean;
						if(dbt_row_set_val(rowp,&dtval,DB1_DOUBLE,ccol))
							goto clean;
					break;

					case DB1_STR:
					case DB1_STRING:
					case DB1_BLOB:
						//LM_DBG("STR value!\n");

						dtval.val.str_val.s = NULL;
						dtval.val.str_val.len = 0;
						dtval.type = dtp->colv[ccol]->type;

						bp = 0;
						if(c==_dbt_delim ||
							(ccol == dtp->nrcols-1
								&& (c == DBT_DELIM_R || c==EOF))) {
							/* If empty_string is enabled, we'll just return
							 * an empty string and avoid NULL
							 */
							if (empty_string == 0) {
								/* Default - NULL */
								dtval.nul = 1;
							}
						} else
						{
							dtval.nul = 0;
							while(c!=_dbt_delim && c!=DBT_DELIM_R && c!=EOF)
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
										case '0':
											c = 0;
										break;
										default:
											if (c==_dbt_delim)
												break;
											goto clean;
									}
								}
								buf[bp++] = c;
								c = fgetc(fin);
							}
							dtval.val.str_val.s = buf;
							dtval.val.str_val.len = bp;
							//LM_DBG("data[%d,%d]=%.*s\n",
							///	crow, ccol, bp, buf);
						}
						if(c!=_dbt_delim && c!=DBT_DELIM_R && c!=EOF)
							goto clean;
						if(dbt_row_set_val(rowp,&dtval,dtp->colv[ccol]->type,
									ccol))
							goto clean;
					break;
					default:
						goto clean;
				}
				if(c==_dbt_delim)
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
	if(buf)
		pkg_free(buf);
	return dtp;
clean:
	// ????? FILL IT IN - incomplete row/column
	// memory leak?!?! with last incomplete row
	LM_DBG("error at row=%d col=%d c=%c\n", crow+1, ccol+1, c);
	if(dtp)
		dbt_table_free(dtp);
	if(fin)
		fclose(fin);
	if(buf)
		pkg_free(buf);
	return NULL;
}


/**
 *
 */
int dbt_print_table_header(dbt_table_p _dtp, FILE *fout)
{
        dbt_column_p colp = NULL;
        colp = _dtp->cols;
        while(colp)
        {
                switch(colp->type)
                {
                        case DB1_INT:
                                fprintf(fout, "%.*s(int", colp->name.len, colp->name.s);
                        break;
                        case DB1_DOUBLE:
                                fprintf(fout, "%.*s(double", colp->name.len, colp->name.s);
                        break;
                        case DB1_STR:
                                fprintf(fout, "%.*s(str", colp->name.len, colp->name.s);
                        break;
                        case DB1_STRING:
                                fprintf(fout, "%.*s(string", colp->name.len, colp->name.s);
                        break;
                        case DB1_BLOB:
                                fprintf(fout, "%.*s(blob", colp->name.len, colp->name.s);
                        break;
                        case DB1_DATETIME:
                                fprintf(fout, "%.*s(time", colp->name.len, colp->name.s);
                        break;
                        default:
                                if(fout!=stdout)
                                        fclose(fout);
                                return -1;
                }

                if(colp->flag & DBT_FLAG_NULL)
                                fprintf(fout,",null");
                else if(colp->type==DB1_INT && colp->flag & DBT_FLAG_AUTO)
                                        fprintf(fout,",auto");
                fprintf(fout,")");

                colp = colp->next;
                if(colp)
                        fprintf(fout,"%c", DBT_DELIM_C);
        }
        fprintf(fout, "%c", DBT_DELIM_R);
        return 0;
}

int dbt_print_table_row_ex(dbt_table_p _dtp, dbt_row_p rowp, FILE *fout, int newline)
{
        int ccol;
        char *p;
        for(ccol=0; ccol<_dtp->nrcols; ccol++)
        {
                switch(_dtp->colv[ccol]->type)
                {
                case DB1_DATETIME:
                case DB1_INT:
                        if(!rowp->fields[ccol].nul)
                                fprintf(fout,"%d",
                                                rowp->fields[ccol].val.int_val);
                        break;
                case DB1_DOUBLE:
                        if(!rowp->fields[ccol].nul)
                                fprintf(fout, "%.2f",
                                                rowp->fields[ccol].val.double_val);
                        break;
                case DB1_STR:
                case DB1_STRING:
                case DB1_BLOB:
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
                                        case '\0':
                                                fprintf(fout, "\\0");
                                                break;
                                        default:
                                                if (*p==_dbt_delim) {
                                                        fprintf(fout, "\\%c", _dbt_delim);
                                                        break;
                                                }
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
                        fprintf(fout, "%c",_dbt_delim);
        }
        if(newline)
                fprintf(fout, "%c", DBT_DELIM_R);

        return 0;
}

int dbt_print_table_row(dbt_table_p _dtp, dbt_row_p rowp, FILE *fout)
{
        return dbt_print_table_row_ex(_dtp, rowp, fout, 1);
}

int dbt_print_table_rows(dbt_table_p _dtp, FILE *fout)
{
        dbt_row_p rowp = _dtp->rows;
        while(rowp) {
                if(dbt_print_table_row(_dtp, rowp, fout))
                        return -1;
                rowp = rowp->next;
        }

        return 0;
}

int dbt_print_table_content(dbt_table_p _dtp, FILE *fout)
{
        if(dbt_print_table_header(_dtp, fout))
                return -1;
        return dbt_print_table_rows(_dtp, fout);
}

int dbt_print_table(dbt_table_p _dtp, str *_dbn)
{
        FILE *fout = NULL;
        int res=0;
        char path[512];

        if(!_dtp || !_dtp->name.s || _dtp->name.len <= 0)
                return -1;

        if(!_dbn || !_dbn->s || _dbn->len <= 0)
        {
                fout = stdout;
                fprintf(fout, "\n Content of [%.*s::%.*s]\n",
                                _dtp->dbname.len, _dtp->dbname.s,
                                _dtp->name.len, _dtp->name.s);
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

        res = dbt_print_table_content(_dtp, fout);
        if(fout!=stdout)
                fclose(fout);

        return res;

}
