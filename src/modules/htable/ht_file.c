/**
 *
 * Copyright (C) 2026 The Kamailio Project
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../../core/basex.h"
#include "../../core/dprint.h"
#include "../../core/usr_avp.h"
#include "../../core/ut.h"

#include "ht_file.h"


static char *ht_file_pathz(str *path)
{
	char *ret;

	ret = malloc(path->len + 1);
	if(ret == NULL)
		return NULL;
	memcpy(ret, path->s, path->len);
	ret[path->len] = '\0';
	return ret;
}


static int ht_file_b64_decode(str *src, str *dst)
{
	char *in = NULL;
	char *out = NULL;
	int dlen;
	int ilen;
	int npad = 0;
	int required;
	int i;

	if(src->len < 0 || src->len > INT_MAX - 4)
		return -1;
	ilen = src->len;
	while(ilen > 0 && src->s[ilen - 1] == '=') {
		npad++;
		ilen--;
	}
	if(npad > 2 || (npad > 0 && src->len % 4 != 0) || ilen % 4 == 1)
		goto error;
	for(i = 0; i < ilen; i++) {
		if(src->s[i] == '=')
			goto error;
	}
	required = (4 - ilen % 4) % 4;
	if(npad > 0 && npad != required)
		goto error;

	in = malloc(ilen + required + 1);
	if(in == NULL)
		goto error;
	memcpy(in, src->s, ilen);
	for(i = 0; i < required; i++)
		in[ilen + i] = '=';
	in[ilen + required] = '\0';

	out = malloc(((ilen + required) / 4) * 3 + 1);
	if(out == NULL)
		goto error;
	dlen = base64url_dec(
			in, ilen + required, out, ((ilen + required) / 4) * 3 + 1);
	if(dlen < 0)
		goto error;

	dst->s = out;
	dst->len = dlen;
	free(in);
	return 0;

error:
	free(in);
	free(out);
	return -1;
}


static int ht_file_b64_encode(str *src, str *dst)
{
	char *out;
	int olen;
	int osize;

	if(src->len < 0 || src->len > (INT_MAX >> 2))
		return -1;
	osize = ((src->len + 2) / 3) * 4 + 1;
	out = malloc(osize);
	if(out == NULL)
		return -1;
	olen = base64url_enc(src->s, src->len, out, osize);
	if(olen < 0) {
		free(out);
		return -1;
	}
	while(olen > 0 && out[olen - 1] == '=')
		olen--;
	out[olen] = '\0';
	dst->s = out;
	dst->len = olen;
	return 0;
}


static int ht_file_split_line(char *line, int len, char delim, str fields[4])
{
	int begin = 0;
	int count = 0;
	int i;

	for(i = 0; i <= len; i++) {
		if(i == len || line[i] == delim) {
			if(count >= 4)
				return -1;
			fields[count].s = line + begin;
			fields[count].len = i - begin;
			count++;
			begin = i + 1;
		}
	}
	return count == 4 ? 0 : -1;
}


static int ht_file_load_line(
		ht_t *ht, char *line, int len, unsigned int lineno, time_t now)
{
	str fields[4];
	str key;
	str value;
	str dkey = STR_NULL;
	str dvalue = STR_NULL;
	unsigned int vtype;
	unsigned long uexpire;
	int_str val;
	int_str expires;
	int ret = -1;

	if(ht_file_split_line(line, len, ht->fdelim, fields) < 0) {
		LM_ERR("htable [%.*s] file [%.*s] line %u: expected four fields\n",
				ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s, lineno);
		goto done;
	}
	if(fields[1].len != 1 || (fields[1].s[0] != '0' && fields[1].s[0] != '1')) {
		LM_ERR("htable [%.*s] file [%.*s] line %u: invalid value type\n",
				ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s, lineno);
		goto done;
	}
	vtype = fields[1].s[0] - '0';
	if(fields[3].len <= 0 || str2ulong(&fields[3], &uexpire) < 0
			|| (unsigned long)(time_t)uexpire != uexpire) {
		LM_ERR("htable [%.*s] file [%.*s] line %u: invalid expiry\n",
				ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s, lineno);
		goto done;
	}
	if(uexpire > 0 && (time_t)uexpire <= now) {
		LM_DBG("htable [%.*s] file [%.*s] line %u: skipping expired entry\n",
				ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s, lineno);
		return 0;
	}

	key = fields[0];
	value = fields[2];
	if(ht->ftype == 1) {
		if(ht_file_b64_decode(&fields[0], &dkey) < 0) {
			LM_ERR("htable [%.*s] file [%.*s] line %u: invalid base64url "
				   "key\n",
					ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s,
					lineno);
			goto done;
		}
		key = dkey;
		if(vtype == 0) {
			if(ht_file_b64_decode(&fields[2], &dvalue) < 0) {
				LM_ERR("htable [%.*s] file [%.*s] line %u: invalid base64url "
					   "value\n",
						ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s,
						lineno);
				goto done;
			}
			value = dvalue;
		}
	}
	if(key.len <= 0) {
		LM_ERR("htable [%.*s] file [%.*s] line %u: empty key\n", ht->name.len,
				ht->name.s, ht->fpath.len, ht->fpath.s, lineno);
		goto done;
	}

	if(vtype == 0) {
		val.s = value;
	} else if(value.len <= 0
			  || (value.len == 1 && (value.s[0] == '+' || value.s[0] == '-'))
			  || str2slong(&value, &val.n) < 0) {
		LM_ERR("htable [%.*s] file [%.*s] line %u: invalid integer value\n",
				ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s, lineno);
		goto done;
	}
	if(ht_set_cell(ht, &key, vtype == 0 ? AVP_VAL_STR : 0, &val, 0) < 0) {
		LM_ERR("htable [%.*s] file [%.*s] line %u: cannot add entry\n",
				ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s, lineno);
		goto done;
	}
	if(ht->htexpire > 0) {
		expires.n = uexpire == 0 ? 0 : (long)((time_t)uexpire - now);
		if(ht_set_cell_expire(ht, &key, 0, &expires) < 0) {
			LM_ERR("htable [%.*s] file [%.*s] line %u: cannot set expiry\n",
					ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s,
					lineno);
			goto done;
		}
	}
	ret = 0;

done:
	free(dkey.s);
	free(dvalue.s);
	return ret;
}


int ht_file_load_table(ht_t *ht)
{
	FILE *fp = NULL;
	char *path = NULL;
	char *line = NULL;
	size_t cap = 0;
	ssize_t nread;
	unsigned int lineno = 0;
	time_t now;
	int ret = -1;

	if(ht == NULL || ht->entries == NULL || ht->fpath.len <= 0)
		return -1;
	path = ht_file_pathz(&ht->fpath);
	if(path == NULL) {
		LM_ERR("no memory for htable file path\n");
		goto done;
	}
	fp = fopen(path, "r");
	if(fp == NULL) {
		LM_ERR("cannot open htable [%.*s] file [%s]: %s\n", ht->name.len,
				ht->name.s, path, strerror(errno));
		goto done;
	}
	now = time(NULL);
	while((nread = getline(&line, &cap, fp)) >= 0) {
		lineno++;
		if(nread > INT_MAX) {
			LM_ERR("htable [%.*s] file [%s] line %u is too long\n",
					ht->name.len, ht->name.s, path, lineno);
			goto done;
		}
		if(nread > 0 && line[nread - 1] == '\n')
			nread--;
		if(nread > 0 && line[nread - 1] == '\r')
			nread--;
		if(nread == 0)
			continue;
		if(memchr(line, '\0', nread) != NULL) {
			LM_ERR("htable [%.*s] file [%s] line %u: embedded NUL byte\n",
					ht->name.len, ht->name.s, path, lineno);
			goto done;
		}
		if(ht_file_load_line(ht, line, nread, lineno, now) < 0)
			goto done;
	}
	if(ferror(fp)) {
		LM_ERR("error reading htable [%.*s] file [%s]: %s\n", ht->name.len,
				ht->name.s, path, strerror(errno));
		goto done;
	}
	ht->fload = 1;
	ret = 0;

done:
	free(line);
	if(fp != NULL)
		fclose(fp);
	free(path);
	return ret;
}


static int ht_file_has_raw_separator(str *value, char delim)
{
	return memchr(value->s, delim, value->len) != NULL
		   || memchr(value->s, '\r', value->len) != NULL
		   || memchr(value->s, '\n', value->len) != NULL
		   || memchr(value->s, '\0', value->len) != NULL;
}


static int ht_file_write_str(FILE *fp, str *value)
{
	if(value->len == 0)
		return 0;
	return fwrite(value->s, value->len, 1, fp) == 1 ? 0 : -1;
}


static int ht_file_write_cell(FILE *fp, ht_t *ht, ht_cell_t *cell)
{
	str key = cell->name;
	str value = STR_NULL;
	str ekey = STR_NULL;
	str evalue = STR_NULL;
	int ret = -1;

	if(cell->flags & AVP_VAL_STR)
		value = cell->value.s;
	if(ht->ftype == 0) {
		if(ht_file_has_raw_separator(&key, ht->fdelim)
				|| ((cell->flags & AVP_VAL_STR)
						&& ht_file_has_raw_separator(&value, ht->fdelim))) {
			LM_ERR("htable [%.*s]: key [%.*s] or its value cannot be written "
				   "with clear file encoding\n",
					ht->name.len, ht->name.s, cell->name.len, cell->name.s);
			goto done;
		}
	} else {
		if(ht_file_b64_encode(&key, &ekey) < 0)
			goto done;
		key = ekey;
		if(cell->flags & AVP_VAL_STR) {
			if(ht_file_b64_encode(&value, &evalue) < 0)
				goto done;
			value = evalue;
		}
	}

	if(ht_file_write_str(fp, &key) < 0 || fputc(ht->fdelim, fp) == EOF
			|| fprintf(fp, "%d%c", (cell->flags & AVP_VAL_STR) ? 0 : 1,
					   ht->fdelim)
					   < 0)
		goto done;
	if(cell->flags & AVP_VAL_STR) {
		if(ht_file_write_str(fp, &value) < 0)
			goto done;
	} else if(fprintf(fp, "%ld", cell->value.n) < 0) {
		goto done;
	}
	if(fprintf(fp, "%c%lld\n", ht->fdelim, (long long)cell->expire) < 0)
		goto done;
	ret = 0;

done:
	free(ekey.s);
	free(evalue.s);
	return ret;
}


int ht_file_save_table(ht_t *ht)
{
	FILE *fp = NULL;
	char *path = NULL;
	char *tmppath = NULL;
	struct stat st;
	ht_cell_t *it;
	time_t now;
	int fd = -1;
	int i;
	int ret = -1;

	if(ht == NULL || ht->entries == NULL || ht->fpath.len <= 0)
		return -1;
	path = ht_file_pathz(&ht->fpath);
	if(path == NULL)
		goto done;
	tmppath = malloc(ht->fpath.len + sizeof(".tmp.XXXXXX"));
	if(tmppath == NULL)
		goto done;
	memcpy(tmppath, path, ht->fpath.len);
	memcpy(tmppath + ht->fpath.len, ".tmp.XXXXXX", sizeof(".tmp.XXXXXX"));
	fd = mkstemp(tmppath);
	if(fd < 0) {
		LM_ERR("cannot create temporary file for htable [%.*s] at [%s]: "
			   "%s\n",
				ht->name.len, ht->name.s, path, strerror(errno));
		goto done;
	}
	if(stat(path, &st) == 0 && fchmod(fd, st.st_mode & 07777) < 0) {
		LM_ERR("cannot preserve permissions for htable file [%s]: %s\n", path,
				strerror(errno));
		goto done;
	}
	fp = fdopen(fd, "w");
	if(fp == NULL) {
		LM_ERR("cannot open temporary htable file [%s]: %s\n", tmppath,
				strerror(errno));
		goto done;
	}
	fd = -1;
	now = time(NULL);
	for(i = 0; i < ht->htsize; i++) {
		ht_slot_lock(ht, i);
		for(it = ht->entries[i].first; it != NULL; it = it->next) {
			if(ht->htexpire > 0 && it->expire > 0 && it->expire <= now)
				continue;
			if(ht_file_write_cell(fp, ht, it) < 0) {
				ht_slot_unlock(ht, i);
				LM_ERR("cannot write htable [%.*s] to [%s]\n", ht->name.len,
						ht->name.s, tmppath);
				goto done;
			}
		}
		ht_slot_unlock(ht, i);
	}
	if(fflush(fp) != 0 || fsync(fileno(fp)) != 0) {
		LM_ERR("cannot flush htable [%.*s] file [%s]: %s\n", ht->name.len,
				ht->name.s, tmppath, strerror(errno));
		goto done;
	}
	if(fclose(fp) != 0) {
		fp = NULL;
		LM_ERR("cannot close htable [%.*s] file [%s]: %s\n", ht->name.len,
				ht->name.s, tmppath, strerror(errno));
		goto done;
	}
	fp = NULL;
	if(rename(tmppath, path) < 0) {
		LM_ERR("cannot replace htable [%.*s] file [%s]: %s\n", ht->name.len,
				ht->name.s, path, strerror(errno));
		goto done;
	}
	ret = 0;

done:
	if(fp != NULL)
		fclose(fp);
	if(fd >= 0)
		close(fd);
	if(ret < 0 && tmppath != NULL)
		unlink(tmppath);
	free(tmppath);
	free(path);
	return ret;
}


int ht_file_load_tables(void)
{
	ht_t *ht;

	for(ht = ht_get_root(); ht != NULL; ht = ht->next) {
		if(ht->fpath.len > 0 && ht_file_load_table(ht) < 0)
			return -1;
	}
	return 0;
}


int ht_file_sync_tables(void)
{
	ht_t *ht;
	int ret = 0;

	for(ht = ht_get_root(); ht != NULL; ht = ht->next) {
		if(ht->fpath.len > 0 && ht->dbmode != 0 && ht->fload != 0
				&& ht_file_save_table(ht) < 0) {
			LM_ERR("failed syncing htable [%.*s] to file [%.*s]\n",
					ht->name.len, ht->name.s, ht->fpath.len, ht->fpath.s);
			ret = -1;
		}
	}
	return ret;
}
