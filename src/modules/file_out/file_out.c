/*
 * Copyright (C) 2024 GILAWA Ltd
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

#include "../../core/sr_module.h"
#include "../../core/route_struct.h"
#include "../../core/str.h"
#include "../../core/mod_fix.h"
#include "../../core/locking.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/timer.h"
#include "../../core/rand/kam_rand.h"

#include "types.h"

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <unistd.h> /* usleep() */
#include <fcntl.h>

MODULE_VERSION


#define FO_DEFAULT_PATH "/var/log/kamailio/file_out"
#define FO_DEFAULT_INTERVAL 10 * 60
#define FO_DEFAULT_EXTENSION ".out"
#define FO_DEFAULT_PREFIX ""

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

static int fo_write_to_file(sip_msg_t *msg, char *index, char *log_message);

static FILE *fo_get_file_handle(const int index, const int worker_id);
static int fo_get_full_path(
		const int index, const int worker_id, char *full_path);
static int fo_init_file(const int index, const int worker_id);
static int fo_close_file(const int index, const int worker_id);
static int fo_check_interval(int index, int worker_id);
static int fo_rotate_file(const int index, const int worker_id);
static void fo_log_writer_process(int file_index, int worker_id);

static int fo_fixup_int_pvar(void **param, int param_no);
static int fo_fixup_str_index(void **param, int param_no);
static int fo_fixup_free_int_pvar(void **param, int param_no);
static int fo_count_assigned_files();
static int fo_add_filename(modparam_t type, void *val);
static int fo_parse_filename_params(str input);

/* Default parameters */

str fo_base_folder = str_init(FO_DEFAULT_PATH);
int fo_worker_usleep = 10000;

/* Shared variables */
int *fo_number_of_files = NULL;
fo_output_properties_t *fo_files;

time_t fo_current_timestamp = 0;

/* clang-format off */
static cmd_export_t cmds[] = {
	{"file_out", (cmd_function)fo_write_to_file, 2, fo_fixup_int_pvar, fo_fixup_free_int_pvar, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {{"base_folder", PARAM_STR, &fo_base_folder},
		{"file", PARAM_STR | PARAM_USE_FUNC, &fo_add_filename},
		{"worker_usleep", PARAM_INT, &fo_worker_usleep},
		{0, 0, 0}};

struct module_exports exports = {
	"file_out",		 /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,			 /* exported functions */
	params,			 /* exported parameters */
	0,				 /* RPC method exports */
	0,				 /* exported pseudo-variables */
	0,				 /* response handling function */
	mod_init,		 /* module initialization function */
	child_init,		 /* per-child init function */
	destroy			 /* module destroy function */
};
/* clang-format on */

static int mod_init(void)
{
	int i, j = 0;
	int total_workers = 0;
	str s;
	fo_worker_file_t *sub;

	LM_DBG("initializing\n");
	LM_DBG("base_folder = %.*s\n", fo_base_folder.len, fo_base_folder.s);

	/* Count the given files */
	*fo_number_of_files = fo_count_assigned_files();

	/* Fixup the prefix */
	for(i = 0; i < *fo_number_of_files; i++) {
		s.s = fo_files[i].fo_prefix.s;
		s.len = fo_files[i].fo_prefix.len;

		if(pv_parse_format(&s, &fo_files[i].fo_prefix_pvs) < 0) {
			LM_ERR("wrong format[%s]\n", s.s);
			return -1;
		}
	}

	/* Initialize per-file workers and per-worker jitter */
	total_workers = 0;
	for(i = 0; i < *fo_number_of_files; i++) {
		if(fo_files[i].num_workers < 1) {
			fo_files[i].num_workers = 1;
		}

		if(fo_create_files(&fo_files[i], fo_files[i].num_workers) < 0) {
			LM_ERR("failed to create files for output index %d\n", i);
			return -1;
		}

		for(j = 0; j < fo_files[i].num_workers; j++) {
			sub = &fo_files[i].files[j];
			sub->fo_stored_timestamp = time(NULL);
			sub->effective_interval_seconds = fo_calculate_effective_interval(
					fo_files[i].fo_base_interval_seconds,
					fo_files[i].fo_interval_range);
		}
		fo_output_properties_print(fo_files[i]);

		total_workers += fo_files[i].num_workers;
	}

	/* Register one process per worker across all files */
	register_procs(total_workers);
	cfg_register_child(total_workers);

	LM_DBG("Initialization done\n");
	return 0;
}

/**
 * per-child init function
 */
static int child_init(int rank)
{
	int pid, res;
	int i = 0;
	int j = 0;
	char desc[16] = {0};
	if(rank != PROC_MAIN) {
		return 0;
	}

	for(i = 0; i < *fo_number_of_files; i++) {
		for(j = 0; j < fo_files[i].num_workers; j++) {
			res = snprintf(desc, sizeof(desc), "fo_writer_%d_%d", i, j);
			if(res < 0 || res >= (int)sizeof(desc)) {
				LM_ERR("Failed to create process description\n");
				return -1;
			}
			pid = fork_process(PROC_NOCHLDINIT, desc, 1);
			if(pid < 0) {
				LM_ERR("fork failed\n");
				return -1;
			}
			if(pid == 0) {
				if(cfg_child_init())
					return -1;

				if(fo_init_file(i, j) < 0)
					return -1;

				for(;;) {
					cfg_update();
					usleep(fo_worker_usleep);
					fo_log_writer_process(i, j);
				}
			}
		}
	}

	return 0;
}

/**
 * module destroy function
 */
static void destroy(void)
{
}

static int fo_rotate_file(const int index, const int worker_id)
{
	fo_worker_file_t *sub = NULL;

	if(fo_check_interval(index, worker_id) == 0) {
		return 0;
	}

	sub = &fo_files[index].files[worker_id];
	sub->fo_stored_timestamp = time(NULL);

	fo_close_file(index, worker_id);
	fo_init_file(index, worker_id);

	return 1;
}

static void fo_log_writer_process(int file_index, int worker_id)
{
	fo_log_message_t log_message;
	int result = 0;
	FILE *out = NULL;
	fo_worker_file_t *sub = &fo_files[file_index].files[worker_id];

	out = fo_get_file_handle(file_index, worker_id);

	if(out == NULL) {
		LM_ERR("file handle is NULL\n");
		return;
	}

	while(!fo_is_queue_empty(sub->queue)) {
		result = fo_dequeue(sub->queue, &log_message);
		if(result < 0) {
			LM_ERR("deque error\n");
			return;
		}
		if(log_message.message != NULL) {


			/* Get prefix for the file */
			if(log_message.prefix != NULL && log_message.prefix->len > 0) {
				if(fprintf(out, "%.*s", log_message.prefix->len,
						   log_message.prefix->s)
						< 0) {
					LM_ERR("Failed to write prefix to file with err {%s}\n",
							strerror(errno));
				}
			}
			if(fprintf(out, "%.*s\n", log_message.message->len,
					   log_message.message->s)
					< 0) {
				LM_ERR("Failed to write to file with err {%s}\n",
						strerror(errno));
			}
			if(fflush(out) < 0) {
				LM_ERR("Failed to flush file with err {%s}\n", strerror(errno));
			}
		}

		if(log_message.prefix != NULL) {
			if(log_message.prefix->s != NULL) {
				shm_free(log_message.prefix->s);
			}
			shm_free(log_message.prefix);
			log_message.prefix = NULL;
		}

		if(log_message.message != NULL) {
			if(log_message.message->s != NULL) {
				shm_free(log_message.message->s);
			}
			shm_free(log_message.message);
			log_message.message = NULL;
		}
	}
}

static int fo_fixup_str_index(void **param, int param_no)
{
	fparam_t *p;
	int index = 0;

	if(strlen((char *)*param) == 0) {
		LM_ERR("function param value is required\n");
		return -1;
	}

	p = (fparam_t *)pkg_malloc(sizeof(fparam_t));
	if(!p) {
		PKG_MEM_ERROR;
		return E_OUT_OF_MEM;
	}
	memset(p, 0, sizeof(fparam_t));
	p->orig = *param;

	/* Map string to index */
	while(index < *fo_number_of_files) {
		str s;
		s.s = fo_files[index].fo_base_filename.s;
		s.len = fo_files[index].fo_base_filename.len;
		if(strncmp(s.s, (char *)*param, s.len) == 0) {
			LM_DBG("Found index %d for %s\n", index, (char *)*param);
			p->v.i = (int)index;
			p->fixed = (void *)(long)index;
			p->type = FPARAM_INT;
			*param = (void *)p;
			return 0;
		}
		index++;
	}

	LM_ERR("Couldn't find %s\n", (char *)*param);
	LM_ERR("Make sure the file [%s] is defined as modparam and not exceeding "
		   "file limit\n",
			(char *)*param);
	pkg_free(p);
	return -1;
}
/*
* fixup function for two parameters
* 1st param: string assoicated with file (returning an index)
* 2nd param: string containing PVs
*/
static int fo_fixup_int_pvar(void **param, int param_no)
{
	if(param_no == 1) {
		return fo_fixup_str_index(param, param_no);
	} else if(param_no == 2) {
		return fixup_spve_all(param, param_no);
	}
	return 0;
}

static int fo_fixup_free_int_pvar(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_igp_null(param, param_no);
	} else if(param_no == 2) {
		return fixup_free_spve_all(param, param_no);
	}

	return 0;
}

static int fo_add_filename(modparam_t type, void *val)
{
	int i = 0;
	if(val == NULL) {
		LM_ERR("modparam value is null\n");
		return -1;
	}
	if(strlen(((str *)val)->s) == 0) {
		LM_ERR("modparam value is empty\n");
		return -1;
	}

	if(fo_number_of_files == NULL) {
		LM_DBG("fo_number_of_files is NULL\n");
		fo_number_of_files = (int *)shm_malloc(sizeof(int));
		if(!fo_number_of_files) {
			SHM_MEM_ERROR;
			return -1;
		}
		*fo_number_of_files = 0;
	}

	if(fo_files == NULL) {
		fo_files = (fo_output_properties_t *)shm_malloc(
				FO_MAX_FILES * sizeof(fo_output_properties_t));
		if(!fo_files) {
			SHM_MEM_ERROR;
			return -1;
		}
		for(i = 0; i < FO_MAX_FILES; i++) {
			fo_output_properties_init(&fo_files[i]);
		}
	}

	if((type & PARAM_STR) == 0) {
		LM_ERR("bad parameter type %d\n", type);
		return -1;
	}

	if(fo_number_of_files != NULL && *fo_number_of_files >= FO_MAX_FILES) {
		LM_ERR("Maximum number of files [%d] reached. The rest will not be "
			   "processed \n",
				*fo_number_of_files);
		return 0;
	}

	/* parse: param_name=value; ... */
	if(fo_parse_filename_params(*((str *)val)) < 0)
		return -1;

	(*fo_number_of_files)++;
	return 0;
}

/*
* Parse the filename parameters
* name, extension, interval
* return 1 if successful
* return -1 if failed
*/
static int fo_parse_filename_params(str in)
{
	LM_DBG("Parsing filename params\n");
	char *name = NULL;
	char *extension = NULL;
	char *interval = NULL;
	char *prefix = NULL;
	char *interval_range = NULL;
	char *workers = NULL;
	char *token = NULL;
	char *saveptr = NULL;
	char *input = in.s;
	long val;

	token = strtok_r(input, ";", &saveptr);
	while(token != NULL) {
		if(strstr(token, "name=") != NULL) {
			name = token + 5;
		} else if(strstr(token, "extension=") != NULL) {
			extension = token + 10;
		} else if(strstr(token, "interval=") != NULL) {
			interval = token + 9;
		} else if(strstr(token, "prefix=") != NULL) {
			prefix = token + 7;
		} else if(strstr(token, "interval_range=") != NULL) {
			interval_range = token + 15;
		} else if(strstr(token, "workers=") != NULL) {
			workers = token + 8;
		} else {
			LM_ERR("Unknown parameter %s\n", token);
			return -1;
		}
		token = strtok_r(NULL, ";", &saveptr);
	}
	if(name != NULL) {
		LM_DBG("name = %s\n", name);
		fo_files[*fo_number_of_files].fo_base_filename.s = name;
		fo_files[*fo_number_of_files].fo_base_filename.len = strlen(name);
	} else {
		LM_ERR("name is required. Make sure you provided name= in modparam "
			   "value\n");
		return -1;
	}

	if(extension != NULL) {
		LM_DBG("extension = %s\n", extension);
		fo_files[*fo_number_of_files].fo_extension.s = extension;
		fo_files[*fo_number_of_files].fo_extension.len = strlen(extension);
	} else {
		LM_DBG("no extension= provided. Using default %s\n",
				FO_DEFAULT_EXTENSION);
		fo_files[*fo_number_of_files].fo_extension.s = FO_DEFAULT_EXTENSION;
		fo_files[*fo_number_of_files].fo_extension.len =
				strlen(FO_DEFAULT_EXTENSION);
	}

	if(interval != NULL) {
		errno = 0;
		/* Reuse token to check if there are any leftover characters */
		val = strtol(interval, &token, 0);

		if(token == interval || *token != '\0'
				|| ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE)) {
			LM_ERR("Could not convert '%s' to long and leftover string is: "
				   "'%s'\n",
					interval, token);
		}
		if(val < 0) {
			LM_ERR("interval cannot be negative\n");
			return -1;
		} else if(val == 0) {
			LM_ERR("interval cannot be zero\n");
			return -1;
		} else if(val > INT_MAX || val < INT_MIN) {
			LM_ERR("interval outside of range, max value %d", INT_MAX);
			return -1;
		}
		LM_DBG("interval = %d\n", (int)val);

		fo_files[*fo_number_of_files].fo_base_interval_seconds = (int)val;
	} else {
		LM_DBG("no interval= provided. Using default %d\n",
				FO_DEFAULT_INTERVAL);
		fo_files[*fo_number_of_files].fo_base_interval_seconds =
				FO_DEFAULT_INTERVAL;
	}

	if(prefix != NULL) {
		LM_DBG("prefix = %s\n", prefix);
		fo_files[*fo_number_of_files].fo_prefix.s = prefix;
		fo_files[*fo_number_of_files].fo_prefix.len = strlen(prefix);
	} else {
		LM_DBG("no prefix= provided. Using default %s\n", FO_DEFAULT_PREFIX);
		fo_files[*fo_number_of_files].fo_prefix.s = FO_DEFAULT_PREFIX;
		fo_files[*fo_number_of_files].fo_prefix.len = strlen(FO_DEFAULT_PREFIX);
	}

	/* Parse interval_range (percentage) */
	if(interval_range != NULL) {
		errno = 0;
		val = strtol(interval_range, &token, 0);

		if(token == interval_range || *token != '\0'
				|| ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE)) {
			LM_ERR("Could not convert interval_range '%s' to long\n",
					interval_range);
			return -1;
		}
		if(val < 0 || val > 100) {
			LM_ERR("interval_range must be between 0 and 100 (%%)\n");
			return -1;
		}
		LM_DBG("interval_range = %d\n", (int)val);
		fo_files[*fo_number_of_files].fo_interval_range = (int)val;
	} else {
		LM_DBG("no interval_range= provided. Using default (no jitter)\n");
		fo_files[*fo_number_of_files].fo_interval_range = 0;
	}

	/* Parse worker count for multi-worker mode */
	if(workers != NULL) {
		errno = 0;
		val = strtol(workers, &token, 0);

		if(token == workers || *token != '\0'
				|| ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE)) {
			LM_ERR("Could not convert workers '%s' to long\n", workers);
			return -1;
		}
		if(val < 1 || val > FO_MAX_WORKERS_PER_FILE) {
			LM_ERR("worker must be between 1 and %d\n",
					FO_MAX_WORKERS_PER_FILE);
			return -1;
		}
		LM_DBG("workers = %d\n", (int)val);
		fo_files[*fo_number_of_files].num_workers = (int)val;
	} else {
		LM_DBG("no worker(s)= provided for file %.*s. Using default (1 "
			   "worker)\n",
				fo_files[*fo_number_of_files].fo_base_filename.len,
				fo_files[*fo_number_of_files].fo_base_filename.s);
		fo_files[*fo_number_of_files].num_workers = 1;
	}

	return 1;
}

/*
* return the number of files assigned
*/
static int fo_count_assigned_files()
{
	return *fo_number_of_files;
}

static int fo_init_file(const int index, const int worker_id)
{
	char full_path[FO_MAX_PATH_LEN];
	fo_worker_file_t *sub = &fo_files[index].files[worker_id];

	if(fo_get_full_path(index, worker_id, full_path) < 0) {
		LM_ERR("failed to generate file path\n");
		return -1;
	}

	sub->file_output = fopen(full_path, "a");

	LM_INFO("Opening file %s\n", full_path);
	if(sub->file_output == NULL) {
		LM_ERR("Couldn't open file %s\n", strerror(errno));
		return -1;
	}

	return 1;
}

static int fo_close_file(const int index, const int worker_id)
{
	int result = 0;
	fo_worker_file_t *sub = &fo_files[index].files[worker_id];

	if(sub->file_output != NULL) {
		result = fclose(sub->file_output);
		if(result != 0) {
			LM_ERR("Failed to close output file");
			return -1;
		}
		sub->file_output = NULL;
		LM_DBG("Closed file %d worker %d\n", index, worker_id);
	}
	return 1;
}

/*
* Check if the interval has passed
* return 1 if interval has passed
* return 0 if interval has not passed
*/
static int fo_check_interval(int index, int worker_id)
{
	fo_worker_file_t *sub = &fo_files[index].files[worker_id];

	fo_current_timestamp = time(NULL);

	/* Check against this worker's own jittered interval */
	if((int)difftime(fo_current_timestamp, sub->fo_stored_timestamp)
			>= sub->effective_interval_seconds) {
		return 1;
	}
	return 0;
}

/**
 * Get file handle for file at index
 */
static FILE *fo_get_file_handle(const int index, const int worker_id)
{
	fo_worker_file_t *sub = &fo_files[index].files[worker_id];

	fo_rotate_file(index, worker_id);

	return sub->file_output;
}

/**
 * Determine full file path
 */
static int fo_get_full_path(
		const int index, const int worker_id, char *full_path)
{
	int res = 0;
	fo_output_properties_t fp = fo_files[index];
	fo_worker_file_t *sub = &fo_files[index].files[worker_id];


	if(fp.num_workers > 1) {
		res = snprintf(full_path, FO_MAX_PATH_LEN, "%.*s/%.*s%.*s_%.f%.*s",
				fo_base_folder.len, fo_base_folder.s, fp.fo_base_filename.len,
				fp.fo_base_filename.s, sub->filename_suffix.len,
				sub->filename_suffix.s,
				difftime(sub->fo_stored_timestamp, (time_t)0),
				fp.fo_extension.len, fp.fo_extension.s);
	} else {
		/* Keep single-worker filename (no _0 suffix) */
		res = snprintf(full_path, FO_MAX_PATH_LEN, "%.*s/%.*s_%.f%.*s",
				fo_base_folder.len, fo_base_folder.s, fp.fo_base_filename.len,
				fp.fo_base_filename.s,
				difftime(sub->fo_stored_timestamp, (time_t)0),
				fp.fo_extension.len, fp.fo_extension.s);
	}
	if(res < 0 || res >= FO_MAX_PATH_LEN) {
		LM_ERR("Failed to generate full path for file index %d worker %d\n",
				index, worker_id);
		return -1;
	}
	return 1;
}

static int fo_write_to_file(sip_msg_t *msg, char *index, char *log_message)
{
	int result, file_index;
	int worker_id;
	str fo_prefix_str = str_init("");
	str fo_prefix_val = str_init("");
	str value = str_init("");
	fo_log_message_t logMessage = {0, 0, 0};

	if(index == NULL || log_message == NULL) {
		LM_ERR("filename or log_messsage is NULL\n");
		return -1;
	}

	result = get_int_fparam(&file_index, msg, (fparam_t *)index);
	if(result < 0) {
		LM_ERR("Failed to get int from param 0: %d\n", result);
		return -1;
	}

	result = get_str_fparam(&value, msg, (fparam_t *)log_message);
	if(result < 0) {
		LM_ERR("Failed to get string from param 1: %d\n", result);
		return -1;
	}

	if(pv_printf_s(msg, fo_files[file_index].fo_prefix_pvs, &fo_prefix_str)
			== 0) {
		fo_prefix_val.s = fo_prefix_str.s;
		fo_prefix_val.len = fo_prefix_str.len;
	}

	/* Allocate memory */
	logMessage.prefix = (str *)shm_malloc(sizeof(str));
	if(logMessage.prefix == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}

	logMessage.message = (str *)shm_malloc(sizeof(str));
	if(logMessage.message == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}

	/* Copy the value */
	if(shm_str_dup(logMessage.prefix, &fo_prefix_val) < 0) {
		LM_ERR("Failed to copy prefix\n");
		return -1;
	}

	if(shm_str_dup(logMessage.message, &value) < 0) {
		LM_ERR("Failed to copy message\n");
		return -1;
	}

	/* Route to a worker queue (single-worker uses files[0]) */
	fo_output_properties_t *fp = &fo_files[file_index];
	worker_id = 0;

	if(fp->num_workers > 1) {
		worker_id = (kam_rand() % fp->num_workers);
	}

	if(fp->files == NULL || fp->files[worker_id].queue == NULL) {
		LM_ERR("worker queue is not initialized for file %d worker %d\n",
				file_index, worker_id);
		return -1;
	}

	logMessage.dest_file = file_index;
	fo_enqueue(fp->files[worker_id].queue, logMessage);

	return 1;
}
