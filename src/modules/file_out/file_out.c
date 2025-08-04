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

#include "types.h"

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <unistd.h> /* usleep() */
#include <fcntl.h>

MODULE_VERSION


#define FO_DEFAULT_PATH "/var/log/kamailio/file_out"
#define FO_DEFAULT_ROTATE_CHECK_INTERVAL 10
#define FO_DEFAULT_INTERVAL 10 * 60
#define FO_DEFAULT_EXTENSION ".out"
#define FO_DEFAULT_PREFIX ""

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

static int fo_write_to_file(sip_msg_t *msg, char *index, char *log_message);

static FILE *fo_get_file_handle(const int index);
static int fo_get_full_path(const int index, char *full_path);
static int fo_init_file(const int index);
static int fo_close_file(const int index);
static int fo_check_interval(int index);
static int fo_rotate_file(const int index);
static int fo_fixup_int_pvar(void **param, int param_no);
static int fo_fixup_str_index(void **param, int param_no);
static int fo_fixup_free_int_pvar(void **param, int param_no);
static int fo_count_assigned_files();
static void fo_log_writer_process(int rank);
static int fo_add_filename(modparam_t type, void *val);
static int fo_parse_filename_params(str input);

static void fo_timer_check_interval(unsigned int ticks, void *);

/* Default parameters */
int fo_check_time = FO_DEFAULT_ROTATE_CHECK_INTERVAL;

str fo_base_folder = str_init(FO_DEFAULT_PATH);
int fo_worker_usleep = 10000;

/* Shared variables */
fo_queue_t *fo_queue = NULL;
int *fo_number_of_files = NULL;
fo_file_properties_t *fo_files;
gen_lock_t *fo_properties_lock;

time_t fo_current_timestamp = 0;

/* clang-format off */
static cmd_export_t cmds[] = {
	{"file_out", (cmd_function)fo_write_to_file, 2, fo_fixup_int_pvar, fo_fixup_free_int_pvar, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {{"base_folder", PARAM_STR, &fo_base_folder},
		{"file", PARAM_STR | PARAM_USE_FUNC, &fo_add_filename},
		{"worker_usleep", PARAM_INT, &fo_worker_usleep},
		{"timer_interval", PARAM_INT, &fo_check_time}, {0, 0, 0}};

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
	int i = 0;
	LM_DBG("initializing\n");
	LM_DBG("base_folder = %.*s\n", fo_base_folder.len, fo_base_folder.s);

	//*  Create shared variables */
	fo_queue = (fo_queue_t *)shm_malloc(sizeof(fo_queue_t));
	if(!fo_queue) {
		SHM_MEM_ERROR;
		return -1;
	}
	fo_queue->front = NULL;
	fo_queue->rear = NULL;
	if(lock_init(&fo_queue->lock) == 0) {
		/* error initializing the lock */
		LM_ERR("error initializing the lock\n");
		return -1;
	}

	fo_properties_lock = lock_alloc();
	if(fo_properties_lock == NULL) {
		LM_ERR("cannot allocate the lock\n");
		return -1;
	}
	if(lock_init(fo_properties_lock) == NULL) {
		LM_ERR("cannot init the lock\n");
		lock_dealloc(fo_properties_lock);
		return -1;
	}

	/* Count the given files */
	*fo_number_of_files = fo_count_assigned_files();

	/* Fixup the prefix */
	for(i = 0; i < *fo_number_of_files; i++) {
		str s;
		s.s = fo_files[i].fo_prefix.s;
		s.len = fo_files[i].fo_prefix.len;

		if(pv_parse_format(&s, &fo_files[i].fo_prefix_pvs) < 0) {
			LM_ERR("wrong format[%s]\n", s.s);
			return -1;
		}
	}

	/* Initialize per process vars */
	for(i = 0; i < *fo_number_of_files; i++) {
		fo_files[i].fo_stored_timestamp = time(NULL);
	}

	/* Register worker process */
	register_procs(1);
	cfg_register_child(1);

	/* Register the timer */
	register_timer(fo_timer_check_interval, 0, fo_check_time);

	LM_DBG("Initialization done\n");
	return 0;
}

/**
 * per-child init function
 */
static int child_init(int rank)
{
	int pid;
	int i = 0;
	if(rank != PROC_MAIN) {
		return 0;
	}

	pid = fork_process(PROC_NOCHLDINIT, "fo_writer", 1);
	if(pid < 0) {
		LM_ERR("fork failed\n");
		return -1; /* error */
	}
	if(pid == 0) {
		/* child */
		/* initialize the config framework */
		if(cfg_child_init())
			return -1;

		/* Initialize and open files  */
		for(i = 0; i < *fo_number_of_files; i++) {
			fo_init_file(i);
		}

		for(;;) {
			/* update the local config framework structures */
			cfg_update();

			usleep(fo_worker_usleep);
			fo_log_writer_process(rank);
		}
		// return 0;
	}
	return 0;
}

/**
 * module destroy function
 */
static void destroy(void)
{
	int result = 0;
	int i = 0;
	for(i = 0; i < *fo_number_of_files; i++) {
		result = fo_file_properties_destroy(&fo_files[i]);
		if(result < 0) {
			LM_ERR("Failed to destroy file properties\n");
		}
	}

	/* Free allocated mem */
	if(fo_number_of_files != NULL) {
		shm_free(fo_number_of_files);
		fo_number_of_files = NULL;
	}

	if(fo_queue != NULL) {
		fo_free_queue(fo_queue);
	}
}

static int fo_rotate_file(int index)
{
	if(fo_check_interval(index) == 0) {
		return 0;
	}
	lock_get(fo_properties_lock);
	fo_files[index].fo_stored_timestamp = time(NULL);
	fo_files[index].fo_requires_rotation = 1;
	lock_release(fo_properties_lock);
	return 1;
}

static void fo_timer_check_interval(unsigned int ticks, void *param)
{
	int index = 0;
	for(index = 0; index < *fo_number_of_files; index++) {
		if(fo_rotate_file(index) == 1) {
			LM_DBG("Next write will rotate the file %d\n", index);
		}
	}
}

static void fo_log_writer_process(int rank)
{
	fo_log_message_t log_message;
	int result = 0;
	FILE *out = NULL;
	while(!fo_is_queue_empty(fo_queue)) {
		result = fo_dequeue(fo_queue, &log_message);
		if(result < 0) {
			LM_ERR("deque error\n");
			return;
		}
		if(log_message.message != NULL) {
			out = fo_get_file_handle(log_message.dest_file);

			if(out == NULL) {
				LM_ERR("file handle is NULL\n");
				return;
			}

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
		fo_files = (fo_file_properties_t *)shm_malloc(
				FO_MAX_FILES * sizeof(fo_file_properties_t));
		if(!fo_files) {
			SHM_MEM_ERROR;
			return -1;
		}
		for(int i = 0; i < FO_MAX_FILES; i++) {
			fo_file_properties_init(&fo_files[i]);
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
			LM_ERR("interval outside of range of int\n");
			return -1;
		}
		LM_DBG("interval = %d\n", (int)val);

		fo_files[*fo_number_of_files].fo_interval_seconds = (int)val;
	} else {
		LM_DBG("no interval= provided. Using default %d\n",
				FO_DEFAULT_INTERVAL);
		fo_files[*fo_number_of_files].fo_interval_seconds = FO_DEFAULT_INTERVAL;
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
	return 1;
}

/*
* return the number of files assigned
*/
static int fo_count_assigned_files()
{
	return *fo_number_of_files;
}

static int fo_init_file(const int index)
{
	char full_path[FO_MAX_PATH_LEN];

	fo_get_full_path(index, full_path);
	fo_files[index].fo_file_output = fopen(full_path, "a");

	LM_INFO("Opening file %s\n", full_path);
	if(fo_files[index].fo_file_output == NULL) {
		LM_ERR("Couldn't open file %s\n", strerror(errno));
		return -1;
	}

	return 1;
}

static int fo_close_file(const int index)
{
	int result = 0;
	if(fo_files[index].fo_file_output != NULL) {
		result = fclose(fo_files[index].fo_file_output);
		if(result != 0) {
			LM_ERR("Failed to close output file");
			return -1;
		}
		fo_files[index].fo_file_output = NULL;
		LM_DBG("Closed file %d\n", index);
	}
	return 1;
}

/*
* Check if the interval has passed
* return 1 if interval has passed
* return 0 if interval has not passed
*/
static int fo_check_interval(int index)
{
	fo_current_timestamp = time(NULL);

	/* Calculate the difference between the current timestamp and the stored timestamp */
	int difference =
			difftime(fo_current_timestamp, fo_files[index].fo_stored_timestamp);
	if(difference >= fo_files[index].fo_interval_seconds) {
		return 1;
	}
	return 0;
}

/**
 * Get file handle for file at index
 */
static FILE *fo_get_file_handle(const int index)
{
	/*
	Lock the properties lock to prevent race conditions when checking
	if the file needs rotation. Find out if the file needs rotation is
	by another timer process that sets the flag. If the flag is set, close
	the file and reopen it.
	*/
	lock_get(fo_properties_lock);
	if(fo_files[index].fo_requires_rotation == 1) {
		fo_close_file(index);
		fo_init_file(index);
		fo_files[index].fo_requires_rotation = 0;
	}
	lock_release(fo_properties_lock);
	return fo_files[index].fo_file_output;
}

/**
 * Determine full file path
 */
static int fo_get_full_path(const int index, char *full_path)
{
	fo_file_properties_t fp = fo_files[index];
	snprintf(full_path, FO_MAX_PATH_LEN, "%.*s/%.*s_%.f%.*s",
			fo_base_folder.len, fo_base_folder.s, fp.fo_base_filename.len,
			fp.fo_base_filename.s, difftime(fp.fo_stored_timestamp, (time_t)0),
			fp.fo_extension.len, fp.fo_extension.s);
	return 1;
}

static int fo_write_to_file(sip_msg_t *msg, char *index, char *log_message)
{
	int result, file_index;
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

	/* Add the logging string to the global gueue */
	logMessage.dest_file = file_index;
	fo_enqueue(fo_queue, logMessage);

	return 1;
}
