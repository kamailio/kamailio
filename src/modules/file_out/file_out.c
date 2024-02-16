/*
 * Copyright (C) 2024 GILAWA Ltd
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
 */

#include "../../core/sr_module.h"
#include "../../core/route_struct.h"
#include "../../core/str.h"
#include "../../core/mod_fix.h"
#include "../../core/locking.h"
#include "../../core/cfg/cfg_struct.h"
#include "types.h"

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <unistd.h> /* usleep() */


MODULE_VERSION


#define FO_MAX_PATH_LEN 2048
#define FO_MAX_FILES 10 /* Maximum number of files */
#define FO_DEFAULT_INTERVAL 10 * 60
#define FO_DEFAULT_EXTENSION ".out"

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

static int fo_write_to_file(sip_msg_t *msg, char *index, char *log_message);

static FILE *fo_get_file_handle(const int index);
static int fo_get_full_path(const int index, char *full_path);
static int fo_init_file(const int index);
static int fo_close_file(const int index);
static int fo_check_interval(int index);
static int fo_fixup_int_pvar(void **param, int param_no);
static int fo_fixup_str_index(void **param, int param_no);
static int fo_fixup_free_int_pvar(void **param, int param_no);
static int fo_count_assigned_files();
static void fo_log_writer_process(int rank);
static int fo_add_filename(modparam_t type, void *val);
static int fo_parse_filename_params(str input);

/* Default parameters */
char *fo_base_folder = "/var/log/kamailio/file_out";
char *fo_base_filename[FO_MAX_FILES] = {""};
char *fo_extension[FO_MAX_FILES] = {".out"};
int fo_interval_seconds[FO_MAX_FILES] = {10 * 60};
int fo_worker_usleep = 10000;

/* Shared variables */
fo_queue_t *fo_queue = NULL;
int *fo_number_of_files = NULL;

time_t fo_stored_timestamp[FO_MAX_FILES] = {0};
time_t fo_current_timestamp = 0;
FILE *fo_file_output[FO_MAX_FILES];

static cmd_export_t cmds[] = {
		{"file_out", (cmd_function)fo_write_to_file, 2, fo_fixup_int_pvar,
				fo_fixup_free_int_pvar, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {
		{"base_folder", PARAM_STRING, &fo_base_folder},
		{"file", PARAM_STRING | PARAM_USE_FUNC, &fo_add_filename},
		{"worker_usleep", PARAM_INT, &fo_worker_usleep}, {0, 0, 0}};

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


static int mod_init(void)
{
	LM_DBG("initializing\n");
	LM_DBG("base_folder = %s\n", fo_base_folder);

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

	/* Count the given files */
	*fo_number_of_files = fo_count_assigned_files();

	/* Initialize per process vars */
	for(int i = 0; i < *fo_number_of_files; i++) {
		fo_stored_timestamp[i] = time(NULL);
	}

	/* Register worker process */
	register_procs(1);
	cfg_register_child(1);
	LM_DBG("Initialization done\n");
	return 0;
}

/**
 * per-child init function
 */
static int child_init(int rank)
{
	int pid;
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
		for(int i = 0; i < *fo_number_of_files; i++) {
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
	if(fo_file_output[0] != NULL) {
		result = fclose(fo_file_output[0]);
		if(result != 0) {
			ERR("Failed to close output file");
		}
	}
}

static void fo_log_writer_process(int rank)
{
	fo_log_message_t log_message;
	int result = 0;
	while(!fo_is_queue_empty(fo_queue)) {
		result = fo_dequeue(fo_queue, &log_message);
		if(result < 0) {
			LM_ERR("deque error\n");
			return;
		}
		FILE *out = fo_get_file_handle(log_message.dest_file);
		if(out == NULL) {
			LM_ERR("file handle is NULL\n");
			return;
		}

		if(fprintf(out, "%.*s\n", log_message.message->len,
				   log_message.message->s)
				< 0) {
			LM_ERR("Failed to write to file with err {%s}\n", strerror(errno));
		}
		if(fflush(out) < 0) {
			LM_ERR("Failed to flush file with err {%s}\n", strerror(errno));
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
		if(strcmp(fo_base_filename[index], (char *)*param) == 0) {
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
	str in;

	if(val != NULL && strlen((char *)val) == 0) {
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

	if((type & PARAM_STRING) == 0) {
		LM_ERR("bad parameter type %d\n", type);
		return -1;
	}

	if(fo_number_of_files != NULL && *fo_number_of_files >= FO_MAX_FILES) {
		LM_ERR("Maximum number of files [%d] reached. The rest will not be "
			   "processed \n",
				*fo_number_of_files);
		return 0;
	}

	/* parse: name=missed_calls;extension=.txt;interval=600 */
	in.s = (char *)val;
	in.len = strlen(in.s);

	if(fo_parse_filename_params(in) < 0)
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
	char *token = NULL;
	char *saveptr = NULL;
	char *input = in.s;
	token = strtok_r(input, ";", &saveptr);
	while(token != NULL) {
		if(strstr(token, "name=") != NULL) {
			name = token + 5;
		} else if(strstr(token, "extension=") != NULL) {
			extension = token + 10;
		} else if(strstr(token, "interval=") != NULL) {
			interval = token + 9;
		}
		token = strtok_r(NULL, ";", &saveptr);
	}
	if(name != NULL) {
		LM_DBG("name = %s\n", name);
		fo_base_filename[*fo_number_of_files] = name;
	} else {
		LM_ERR("name is required. Make sure you provided name= in modparam "
			   "value\n");
		return -1;
	}

	if(extension != NULL) {
		LM_DBG("extension = %s\n", extension);
		fo_extension[*fo_number_of_files] = extension;
	} else {
		LM_DBG("no extension= provided. Using default %s\n",
				FO_DEFAULT_EXTENSION);
		fo_extension[*fo_number_of_files] = FO_DEFAULT_EXTENSION;
	}

	if(interval != NULL) {
		LM_DBG("interval = %s\n", interval);
		fo_interval_seconds[*fo_number_of_files] = atoi(interval);
	} else {
		LM_DBG("no interval= provided. Using default %d\n",
				FO_DEFAULT_INTERVAL);
		fo_interval_seconds[*fo_number_of_files] = FO_DEFAULT_INTERVAL;
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
	fo_file_output[index] = fopen(full_path, "a");
	if(fo_file_output[index] == NULL) {
		LM_ERR("Couldn't open file %s\n", strerror(errno));
		return -1;
	}
	return 1;
}

static int fo_close_file(const int index)
{
	int result = 0;
	if(fo_file_output[index] != NULL) {
		result = fclose(fo_file_output[index]);
		if(result != 0) {
			ERR("Failed to close output file");
			return -1;
		}
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

	// Calculate the difference between the current timestamp and the stored timestamp
	int difference = difftime(fo_current_timestamp, fo_stored_timestamp[index]);
	if(difference >= fo_interval_seconds[index]) {
		return 1;
	}
	return 0;
}

/**
 * Get file handle for file at index
 */
static FILE *fo_get_file_handle(const int index)
{
	int result = 0;
	if(fo_check_interval(index)) {
		/* Interval passed. Close all files and open new ones */
		result = fo_close_file(index);
		if(result != 1) {
			LM_ERR("Failed to close output file");
			return NULL;
		}
		fo_stored_timestamp[index] = fo_current_timestamp;

		LM_DBG("Opening new file due to interval passed\n");
		/* Initialize and open files  */
		result = fo_init_file(index);
		if(result != 1) {
			LM_ERR("Failed to initialize output file");
			return NULL;
		}
		return fo_file_output[index];
	} else {
		/* Interval has not passed */
		/* Assume files are correct */
		return fo_file_output[index];
	}
}

/**
 * Determine full file path
 */
static int fo_get_full_path(const int index, char *full_path)
{
	snprintf(full_path, FO_MAX_PATH_LEN, "%s/%s_%.f%s", fo_base_folder,
			fo_base_filename[index],
			difftime(fo_stored_timestamp[index], (time_t)0),
			fo_extension[index]);
	LM_INFO("Path to write to: %s\n", full_path);
	return 1;
}

static int fo_write_to_file(sip_msg_t *msg, char *index, char *log_message)
{
	int result, file_index;
	if(index == NULL || log_message == NULL) {
		LM_ERR("filename or log_messsage is NULL\n");
		return -1;
	}

	result = get_int_fparam(&file_index, msg, (fparam_t *)index);
	if(result < 0) {
		LM_ERR("Failed to get int from param 0: %d\n", result);
		return -1;
	}

	str value = str_init("");
	result = get_str_fparam(&value, msg, (fparam_t *)log_message);
	if(result < 0) {
		LM_ERR("Failed to get string from param 1: %d\n", result);
		return -1;
	}

	/* Add the logging string to the global gueue */
	fo_log_message_t logMessage = {0, 0};
	logMessage.message = &value;
	logMessage.dest_file = file_index;
	fo_enqueue(fo_queue, logMessage);

	return 1;
}
