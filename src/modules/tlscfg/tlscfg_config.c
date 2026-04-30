/*
 * tlscfg module - TLS profile management companion for the tls module
 *
 * Copyright (C) 2026 Aurora Innovation
 *
 * Author: Daniel Donoghue
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

/**
 * @file tlscfg_config.c
 * @brief Config file parser and atomic writer
 * @ingroup tlscfg
 */

#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"

#include "tlscfg_config.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define TLSCFG_LINE_MAX 1024
#define TLSCFG_PATH_MAX 512
#define TLSCFG_DISABLED_MARKER "#!tlscfg:disabled"

/* implemented in tlscfg_mod.c */
extern const char *tlscfg_get_cert_base_path(void);
extern const char *tlscfg_get_config_backup_dir(void);
extern int tlscfg_get_config_max_backups(void);

/* ---- helpers ---- */

static char *trim(char *s)
{
	char *end;

	while(isspace((unsigned char)*s))
		s++;
	if(*s == '\0')
		return s;
	end = s + strlen(s) - 1;
	while(end > s && isspace((unsigned char)*end))
		end--;
	*(end + 1) = '\0';
	return s;
}

static int is_cert_path_field(const char *key)
{
	return (strcasecmp(key, "certificate") == 0
			|| strcasecmp(key, "certificate2") == 0
			|| strcasecmp(key, "private_key") == 0
			|| strcasecmp(key, "private_key2") == 0
			|| strcasecmp(key, "ca_list") == 0 || strcasecmp(key, "crl") == 0);
}

static int is_key_field(const char *key)
{
	return (strcasecmp(key, "private_key") == 0
			|| strcasecmp(key, "private_key2") == 0);
}

static int resolve_cert_path(const char *ref, char *out, int outlen, int is_key)
{
	const char *base = tlscfg_get_cert_base_path();

	if(ref[0] == '/') {
		snprintf(out, outlen, "%s", ref);
	} else if(strncmp(ref, "certman:", 8) == 0) {
		const char *id = ref + 8;
		if(is_key) {
			snprintf(out, outlen, "%s/%s/key.pem", base, id);
		} else {
			snprintf(out, outlen, "%s/%s/cert.pem", base, id);
		}
	} else {
		snprintf(out, outlen, "%s/%s", base, ref);
	}
	return 0;
}

/**
 * @brief Parse a section header like "[server:default]" or "[client:any]"
 * @return 1 on success, 0 if not a section header
 */
static int parse_section_header(const char *line, char *out, int outlen)
{
	const char *p, *end;
	int len;

	p = line;
	while(isspace((unsigned char)*p))
		p++;
	if(*p != '[')
		return 0;

	end = strchr(p, ']');
	if(!end)
		return 0;

	len = (int)(end - p) + 1;
	if(len >= outlen)
		return 0;

	memcpy(out, p, len);
	out[len] = '\0';
	return 1;
}

/**
 * @brief Derive a profile_id from a section header
 *
 * Strips the brackets: "[server:default]" -> "server:default"
 */
static void derive_profile_id(const char *section, str *id)
{
	int len = strlen(section);

	if(len >= 2 && section[0] == '[' && section[len - 1] == ']') {
		id->s = (char *)section + 1;
		id->len = len - 2;
	} else {
		id->s = (char *)section;
		id->len = len;
	}
}

/* ---- config load ---- */

int tlscfg_config_load(str *path)
{
	FILE *fp;
	char line[TLSCFG_LINE_MAX];
	char section[320];
	char *trimmed, *eq, *key, *val;
	struct stat st;
	str sid, shdr, skey, sval;
	int in_disabled = 0;
	tlscfg_data_t *data;

	if(!path || !path->s || path->len == 0)
		return -1;

	data = tlscfg_data_get();
	if(!data)
		return -1;

	fp = fopen(path->s, "r");
	if(!fp) {
		LM_ERR("cannot open config file '%.*s': %s\n", path->len, path->s,
				strerror(errno));
		return -1;
	}

	/* record mtime */
	if(fstat(fileno(fp), &st) == 0) {
		data->config_mtime = st.st_mtime;
	}

	/* clear existing profiles */
	tlscfg_profile_clear();

	section[0] = '\0';

	while(fgets(line, sizeof(line), fp)) {
		line[strcspn(line, "\r\n")] = '\0';
		trimmed = trim(line);

		/* disabled section marker */
		if(strcmp(trimmed, TLSCFG_DISABLED_MARKER) == 0) {
			in_disabled = 1;
			continue;
		}

		/* disabled section header: "# [type:addr]" */
		if(in_disabled && trimmed[0] == '#') {
			char *inner = trim(trimmed + 1);
			if(inner[0] == '[') {
				if(parse_section_header(inner, section, sizeof(section))) {
					shdr.s = section;
					shdr.len = strlen(section);
					derive_profile_id(section, &sid);
					tlscfg_profile_add(&sid, &shdr);
					tlscfg_profile_set_enabled(&sid, 0);
					continue;
				}
			}
		}

		/* disabled kv: "# key = value" */
		if(in_disabled && trimmed[0] == '#' && section[0] != '\0') {
			char *inner = trim(trimmed + 1);
			if(inner[0] == '\0' || inner[0] == '#')
				continue;
			if(inner[0] == '[') {
				in_disabled = 0;
				/* fall through to normal parsing */
			} else {
				eq = strchr(inner, '=');
				if(eq) {
					*eq = '\0';
					key = trim(inner);
					val = trim(eq + 1);
					derive_profile_id(section, &sid);
					if(strcasecmp(key, "profile_id") == 0) {
						sval.s = val;
						sval.len = strlen(val);
						tlscfg_profile_set_id(&sid, &sval);
					} else if(strcasecmp(key, "enabled") == 0) {
						/* skip — handled separately */
					} else {
						skey.s = key;
						skey.len = strlen(key);
						sval.s = val;
						sval.len = strlen(val);
						tlscfg_profile_set(&sid, &skey, &sval);
					}
				}
				continue;
			}
		}

		/* end of disabled block */
		if(in_disabled && trimmed[0] != '#') {
			in_disabled = 0;
		}

		/* tlscfg structured comments: # tlscfg:key = value */
		if(strncmp(trimmed, "# tlscfg:", 9) == 0 && section[0] != '\0') {
			char *inner = trim(trimmed + 9);
			eq = strchr(inner, '=');
			if(eq) {
				*eq = '\0';
				key = trim(inner);
				val = trim(eq + 1);
				derive_profile_id(section, &sid);
				if(strcasecmp(key, "profile_id") == 0) {
					sval.s = val;
					sval.len = strlen(val);
					tlscfg_profile_set_id(&sid, &sval);
				}
			}
			continue;
		}

		/* skip empty lines and comments */
		if(trimmed[0] == '\0' || trimmed[0] == '#') {
			continue;
		}

		/* normal section header */
		if(parse_section_header(trimmed, section, sizeof(section))) {
			shdr.s = section;
			shdr.len = strlen(section);
			derive_profile_id(section, &sid);
			tlscfg_profile_add(&sid, &shdr);
			continue;
		}

		/* key = value pair */
		if(section[0] != '\0') {
			eq = strchr(trimmed, '=');
			if(eq) {
				*eq = '\0';
				key = trim(trimmed);
				val = trim(eq + 1);
				derive_profile_id(section, &sid);

				if(strcasecmp(key, "enabled") == 0) {
					int en = (strcasecmp(val, "yes") == 0
							  || strcmp(val, "1") == 0);
					tlscfg_profile_set_enabled(&sid, en);
				} else {
					skey.s = key;
					skey.len = strlen(key);
					sval.s = val;
					sval.len = strlen(val);
					tlscfg_profile_set(&sid, &skey, &sval);
				}
			}
		}
	}

	fclose(fp);

	data->dirty = 0;

	LM_INFO("loaded tls config from '%.*s'\n", path->len, path->s);
	return 0;
}

/* ---- config backup ---- */

/**
 * @brief Compare function for qsort — oldest first (ascending)
 */
static int backup_name_cmp(const void *a, const void *b)
{
	return strcmp(*(const char **)a, *(const char **)b);
}

/**
 * @brief Create a timestamped backup of the config file before overwriting.
 *
 * Copies the current file to {backup_dir}/tls.cfg.YYYYMMDDTHHmmSS.bak
 * and prunes old backups if count exceeds max_backups.
 */
static void config_backup(str *path)
{
	const char *backup_dir;
	int max_backups;
	char bakpath[TLSCFG_PATH_MAX];
	char dirpath[TLSCFG_PATH_MAX];
	struct stat st;
	struct tm tm;
	time_t now;
	FILE *src, *dst;
	char buf[4096];
	size_t n;
	DIR *dp;
	struct dirent *de;
	char *backups[256];
	int count = 0;
	const char *basename;
	int baselen;
	int i;

	max_backups = tlscfg_get_config_max_backups();
	if(max_backups <= 0)
		return;

	if(stat(path->s, &st) != 0)
		return; /* nothing to back up */

	backup_dir = tlscfg_get_config_backup_dir();
	if(!backup_dir || backup_dir[0] == '\0') {
		/* default: same directory as the config file */
		snprintf(dirpath, sizeof(dirpath), "%.*s", path->len, path->s);
		/* strip filename to get directory */
		char *slash = strrchr(dirpath, '/');
		if(slash) {
			*slash = '\0';
		} else {
			strcpy(dirpath, ".");
		}
		backup_dir = dirpath;
	}

	/* extract basename from path for backup naming */
	basename = strrchr(path->s, '/');
	basename = basename ? basename + 1 : path->s;
	baselen = strlen(basename);

	/* generate timestamped backup name */
	now = time(NULL);
	gmtime_r(&now, &tm);
	snprintf(bakpath, sizeof(bakpath), "%s/%s.%04d%02d%02dT%02d%02d%02d.bak",
			backup_dir, basename, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);

	/* copy current config to backup */
	src = fopen(path->s, "r");
	if(!src)
		return;
	dst = fopen(bakpath, "w");
	if(!dst) {
		fclose(src);
		LM_ERR("cannot create backup '%s': %s\n", bakpath, strerror(errno));
		return;
	}
	while((n = fread(buf, 1, sizeof(buf), src)) > 0) {
		if(fwrite(buf, 1, n, dst) != n) {
			LM_ERR("failed writing backup '%s'\n", bakpath);
			break;
		}
	}
	fclose(src);
	fclose(dst);
	LM_INFO("config backup created: %s\n", bakpath);

	/* prune old backups beyond max_backups */
	dp = opendir(backup_dir);
	if(!dp)
		return;

	count = 0;
	while((de = readdir(dp)) != NULL && count < 256) {
		/* match: {basename}.YYYYMMDDTHHmmSS.bak */
		int namelen = strlen(de->d_name);
		if(namelen == baselen + 20 /* .YYYYMMDDTHHmmSS.bak */
				&& strncmp(de->d_name, basename, baselen) == 0
				&& de->d_name[baselen] == '.'
				&& strcmp(de->d_name + namelen - 4, ".bak") == 0) {
			backups[count] = strdup(de->d_name);
			if(backups[count])
				count++;
		}
	}
	closedir(dp);

	if(count > max_backups) {
		qsort(backups, count, sizeof(char *), backup_name_cmp);
		for(i = 0; i < count - max_backups; i++) {
			char rmpath[TLSCFG_PATH_MAX];
			snprintf(rmpath, sizeof(rmpath), "%s/%s", backup_dir, backups[i]);
			if(unlink(rmpath) == 0) {
				LM_INFO("pruned old backup: %s\n", rmpath);
			}
		}
	}

	for(i = 0; i < count; i++) {
		free(backups[i]);
	}
}

/* ---- config write ---- */

int tlscfg_config_write(str *path)
{
	FILE *fp;
	char tmppath[TLSCFG_PATH_MAX];
	char resolved[TLSCFG_PATH_MAX];
	struct stat st;
	tlscfg_data_t *data;
	tlscfg_profile_t *p;
	tlscfg_kv_t *kv;

	if(!path || !path->s || path->len == 0)
		return -1;

	data = tlscfg_data_get();
	if(!data)
		return -1;

	/* create timestamped backup before overwriting */
	config_backup(path);

	snprintf(tmppath, sizeof(tmppath), "%.*s.tmp.%d", path->len, path->s,
			(int)getpid());

	fp = fopen(tmppath, "w");
	if(!fp) {
		LM_ERR("cannot open tmp file '%s': %s\n", tmppath, strerror(errno));
		return -1;
	}

	fprintf(fp, "# TLS configuration - managed by tlscfg\n");
	fprintf(fp, "# Do not edit while kamailio is running\n\n");

	for(p = data->profiles; p; p = p->next) {
		if(!p->enabled) {
			fprintf(fp, "%s\n", TLSCFG_DISABLED_MARKER);
			fprintf(fp, "# %.*s\n", p->section_header.len, p->section_header.s);
			fprintf(fp, "# profile_id = %.*s\n", p->profile_id.len,
					p->profile_id.s);
			fprintf(fp, "# enabled = no\n");
			for(kv = p->kvs; kv; kv = kv->next) {
				if(is_cert_path_field(kv->key.s) && kv->value.len > 0
						&& kv->value.s[0] != '/') {
					resolve_cert_path(kv->value.s, resolved, sizeof(resolved),
							is_key_field(kv->key.s));
					fprintf(fp, "# %.*s = %s\n", kv->key.len, kv->key.s,
							resolved);
				} else {
					fprintf(fp, "# %.*s = %.*s\n", kv->key.len, kv->key.s,
							kv->value.len, kv->value.s);
				}
			}
			fprintf(fp, "\n");
		} else {
			fprintf(fp, "%.*s\n", p->section_header.len, p->section_header.s);
			fprintf(fp, "# tlscfg:profile_id = %.*s\n", p->profile_id.len,
					p->profile_id.s);
			fprintf(fp, "# enabled = yes\n");
			for(kv = p->kvs; kv; kv = kv->next) {
				if(is_cert_path_field(kv->key.s) && kv->value.len > 0
						&& kv->value.s[0] != '/') {
					resolve_cert_path(kv->value.s, resolved, sizeof(resolved),
							is_key_field(kv->key.s));
					fprintf(fp, "%.*s = %s\n", kv->key.len, kv->key.s,
							resolved);
				} else {
					fprintf(fp, "%.*s = %.*s\n", kv->key.len, kv->key.s,
							kv->value.len, kv->value.s);
				}
			}
			fprintf(fp, "\n");
		}
	}

	fclose(fp);

	if(rename(tmppath, path->s) != 0) {
		LM_ERR("rename '%s' -> '%.*s' failed: %s\n", tmppath, path->len,
				path->s, strerror(errno));
		unlink(tmppath);
		return -1;
	}

	if(stat(path->s, &st) == 0) {
		data->config_mtime = st.st_mtime;
	}
	data->dirty = 0;

	LM_INFO("wrote tls config to '%.*s'\n", path->len, path->s);
	return 0;
}

time_t tlscfg_config_get_mtime(void)
{
	tlscfg_data_t *data = tlscfg_data_get();

	if(!data)
		return 0;
	return data->config_mtime;
}
