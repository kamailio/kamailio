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

static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

static int fo_write_to_file(sip_msg_t *msg, char *index, char *log_message);

static FILE *fo_get_file_handle(const int index);
static int fo_get_full_path(const int index, char *full_path);
static int fo_init_file(const int index);
static int fo_close_file(const int index);
static int fo_check_interval();
static int fo_fixup_int_pvar(void **param, int param_no);
static int fo_count_assigned_files();
static void fo_log_writer_process(int rank);
static int fo_add_filename(modparam_t type, void *val);

/* Default parameters */
char *fo_base_folder = "/var/log/kamailio/file_out/";
char *fo_base_filename[FO_MAX_FILES] = {""};
char *fo_extension = ".out";
int fo_interval_seconds = 10 * 60;

/* Shared variables */
Queue *fo_queue = NULL;
int *fo_number_of_files = NULL;

time_t fo_stored_timestamp = 0;
time_t fo_current_timestamp = 0;
FILE *fo_file_output[FO_MAX_FILES];

static cmd_export_t cmds[] = {{"file_out", (cmd_function)fo_write_to_file, 2,
									  fo_fixup_int_pvar, 0, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {
		{"base_folder", PARAM_STRING, &fo_base_folder},
		{"base_filename", PARAM_STRING | PARAM_USE_FUNC, &fo_add_filename},
		{"interval_seconds", PARAM_INT, &fo_interval_seconds},
		{"extension", PARAM_STRING, &fo_extension}, {0, 0, 0}};

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
	LM_DBG("base_filename_path= %s\n", fo_base_filename[0]);
	LM_DBG("interval_seconds = %d\n", fo_interval_seconds);
	LM_DBG("extension = %s\n", fo_extension);

	//*  Create shared variables */
	fo_queue = (Queue *)shm_malloc(sizeof(Queue));
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
	fo_stored_timestamp = time(NULL);

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

	pid = fork_process(PROC_NOCHLDINIT, "log_writ", 1);
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

			usleep(10000);
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
	LogMessage log_message;
	int result = 0;
	while(!isQueueEmpty(fo_queue)) {
		result = deQueue(fo_queue, &log_message);
		if(result < 0) {
			LM_ERR("deque error\n");
			return;
		}
		FILE *out = fo_get_file_handle(log_message.dest_file);
		if(out == NULL) {
			LM_ERR("out is NULL\n");
			return;
		}

		fprintf(out, "%s\n", log_message.message);
		fflush(out);
	}
}

/*
* fixup function for two parameters
* 1st param: int
* 2nd param: string containing PVs
*/
static int fo_fixup_int_pvar(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_igp_null(param, param_no);
	} else if(param_no == 2) {
		return fixup_var_pve_str_12(param, param_no);
	}
	return 0;
}

static int fo_add_filename(modparam_t type, void *val)
{
	if(fo_number_of_files == NULL) {
		LM_ERR("fo_number_of_files is NULL\n");
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
				FO_MAX_FILES);
		return 0;
	}
	fo_base_filename[*fo_number_of_files] = (char *)val;
	LM_DBG("fo_base_filename[%d] = %s\n", *fo_number_of_files,
			fo_base_filename[*fo_number_of_files]);
	(*fo_number_of_files)++;
	return 0;
}

/*
* Count the number of files that are assigned
* return the number of files (saved also in shared fo_number_of_files)
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
static int fo_check_interval()
{
	fo_current_timestamp = time(NULL);

	// Calculate the difference between the current timestamp and the stored timestamp
	int difference = difftime(fo_current_timestamp, fo_stored_timestamp);
	if(difference >= fo_interval_seconds) {
		LM_ERR("interval has passed\n");
		return 1;
	}
	// LM_ERR("interval has not passed\n");
	return 0;
}
/**
 * maintain file handle
 */

static FILE *fo_get_file_handle(const int index)
{
	int result = 0;
	if(fo_check_interval()) {
		/* Interval passed. Close all files and open new ones */
		for(int i = 0; i < *fo_number_of_files; i++) {
			result = fo_close_file(i);
			if(result != 1) {
				LM_ERR("Failed to close output file");
				return NULL;
			}
		}
		fo_stored_timestamp = fo_current_timestamp;

		LM_DBG("Opening new files due to interval passed\n");
		/* Make sure we know how many files we need */
		if(fo_number_of_files == NULL) {
			*fo_number_of_files = fo_count_assigned_files();
		}
		/* Initialize and open files  */
		for(int i = 0; i < *fo_number_of_files; i++) {
			result = fo_init_file(i);
			if(result != 1) {
				LM_ERR("Failed to initialize output file");
				return NULL;
			}
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
			fo_base_filename[index], difftime(fo_stored_timestamp, (time_t)0),
			fo_extension);
	LM_INFO("Path to write to: %s\n", full_path);
	return 1;
}

static int fo_write_to_file(sip_msg_t *msg, char *index, char *log_message)
{
	int result, file_index;
	if(index == NULL || log_message == NULL) {
		LM_ERR("index or log_messsage is NULL\n");
		return -1;
	}

	result = get_int_fparam(&file_index, msg, (fparam_t *)index);
	if(result < 0) {
		LM_ERR("Failed to get int from param 0: %d\n", result);
		return -1;
	}

	str value;
	result = get_str_fparam(&value, msg, (fparam_t *)log_message);
	if(result < 0) {
		LM_ERR("Failed to string from param 1: %d\n", result);
		return -1;
	}

	/* Add the logging string to the global gueue */
	LogMessage logMessage;
	logMessage.message = value.s;
	logMessage.dest_file = file_index;
	enQueue(fo_queue, logMessage);

	return 1;
}
