/**
 * Copyright (C) 2014-2017 Daniel-Constantin Mierla (asipto.com)
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "../../core/ver.h"
#include "../../core/trim.h"
#include "../../core/pt.h"
#include "../../core/sr_module.h"
#include "../../core/cfg/cfg_struct.h"

#include "jsonrpcs_mod.h"

/* FIFO server parameters */
char *jsonrpc_fifo = NAME "_rpc.fifo"; /*!< FIFO file name */
char *jsonrpc_fifo_reply_dir =
		"/tmp/";			  /*!< dir where reply fifos are allowed */
int jsonrpc_fifo_uid = -1;	  /*!< Fifo default UID */
char *jsonrpc_fifo_uid_s = 0; /*!< Fifo default User ID name */
int jsonrpc_fifo_gid = -1;	  /*!< Fifo default Group ID */
char *jsonrpc_fifo_gid_s = 0; /*!< Fifo default Group ID name */
int jsonrpc_fifo_mode =
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP; /* Default file mode rw-rw---- */


static FILE *_jsonrpcs_fifo_stream = NULL;


static int jsonrpc_fifo_read = 0;
static int jsonrpc_fifo_write = 0;
#define JSONRPC_MAX_FILENAME 128
static char *jsonrpc_reply_fifo_s = NULL;
static int jsonrpc_reply_fifo_len = 0;

/*! \brief Initialize Fifo server */
FILE *jsonrpc_init_fifo_server(char *fifo_name, int fifo_mode, int fifo_uid,
		int fifo_gid, char *fifo_reply_dir)
{
	FILE *fifo_stream;
	long opt;

	/* create FIFO ... */
	if((mkfifo(fifo_name, fifo_mode) < 0)) {
		LM_ERR("Can't create FIFO: %s (mode=%d)\n", strerror(errno), fifo_mode);
		return 0;
	}

	LM_DBG("FIFO created @ %s\n", fifo_name);

	if((chmod(fifo_name, fifo_mode) < 0)) {
		LM_ERR("Can't chmod FIFO: %s (mode=%d)\n", strerror(errno), fifo_mode);
		return 0;
	}

	if((fifo_uid != -1) || (fifo_gid != -1)) {
		if(chown(fifo_name, fifo_uid, fifo_gid) < 0) {
			LM_ERR("Failed to change the owner/group for %s  to %d.%d; "
				   "%s[%d]\n",
					fifo_name, fifo_uid, fifo_gid, strerror(errno), errno);
			return 0;
		}
	}

	LM_DBG("fifo %s opened, mode=%o\n", fifo_name, fifo_mode);

	/* open it non-blocking or else wait here until someone
	 * opens it for writing */
	jsonrpc_fifo_read = open(fifo_name, O_RDONLY | O_NONBLOCK, 0);
	if(jsonrpc_fifo_read < 0) {
		LM_ERR("Can't open fifo %s for reading - fifo_read did not open: %s\n",
				fifo_name, strerror(errno));
		return 0;
	}

	fifo_stream = fdopen(jsonrpc_fifo_read, "r");
	if(fifo_stream == NULL) {
		LM_ERR("fdopen failed on %s: %s\n", fifo_name, strerror(errno));
		return 0;
	}

	/* make sure the read fifo will not close */
	jsonrpc_fifo_write = open(fifo_name, O_WRONLY | O_NONBLOCK, 0);
	if(jsonrpc_fifo_write < 0) {
		LM_ERR("fifo_write did not open: %s\n", strerror(errno));
		fclose(fifo_stream);
		return 0;
	}
	/* set read fifo blocking mode */
	if((opt = fcntl(jsonrpc_fifo_read, F_GETFL)) == -1) {
		LM_ERR("fcntl(F_GETFL) failed: %s [%d]\n", strerror(errno), errno);
		fclose(fifo_stream);
		return 0;
	}
	if(fcntl(jsonrpc_fifo_read, F_SETFL, opt & (~O_NONBLOCK)) == -1) {
		LM_ERR("cntl(F_SETFL) failed: %s [%d]\n", strerror(errno), errno);
		fclose(fifo_stream);
		return 0;
	}

	jsonrpc_reply_fifo_s = pkg_malloc(JSONRPC_MAX_FILENAME);
	if(jsonrpc_reply_fifo_s == NULL) {
		LM_ERR("no more private memory\n");
		fclose(fifo_stream);
		return 0;
	}

	/* init fifo reply dir buffer */
	jsonrpc_reply_fifo_len = strlen(fifo_reply_dir);
	memcpy(jsonrpc_reply_fifo_s, jsonrpc_fifo_reply_dir,
			jsonrpc_reply_fifo_len);
	jsonrpc_reply_fifo_s[jsonrpc_reply_fifo_len] = '\0';

	return fifo_stream;
}

/*! \brief Initialize fifo transport */
int jsonrpc_init_fifo_file(void)
{
	int n;
	struct stat filestat;

	/* checking the jsonrpc_fifo module param */
	if(jsonrpc_fifo == NULL || *jsonrpc_fifo == 0) {
		jsonrpc_fifo = NULL;
		LM_DBG("No fifo configured\n");
		return 0;
	}

	LM_DBG("testing if fifo file exists ...\n");
	n = stat(jsonrpc_fifo, &filestat);
	if(n == 0) {
		/* FIFO exist, delete it (safer) if no config check */
		if(config_check == 0) {
			if(unlink(jsonrpc_fifo) < 0) {
				LM_ERR("Cannot delete old fifo (%s): %s\n", jsonrpc_fifo,
						strerror(errno));
				return -1;
			}
		}
	} else if(n < 0 && errno != ENOENT) {
		LM_ERR("MI FIFO stat failed: %s\n", strerror(errno));
		return -1;
	}

	/* checking the fifo_reply_dir param */
	if(!jsonrpc_fifo_reply_dir || *jsonrpc_fifo_reply_dir == 0) {
		LM_ERR("fifo_reply_dir parameter is empty\n");
		return -1;
	}

	/* Check if the directory for the reply fifo exists */
	n = stat(jsonrpc_fifo_reply_dir, &filestat);
	if(n < 0) {
		LM_ERR("Directory stat for MI Fifo reply failed: %s\n",
				strerror(errno));
		return -1;
	}

	if(S_ISDIR(filestat.st_mode) == 0) {
		LM_ERR("fifo_reply_dir parameter is not a directory\n");
		return -1;
	}

	/* check fifo_mode */
	if(!jsonrpc_fifo_mode) {
		LM_WARN("cannot specify fifo_mode = 0, forcing it to rw-------\n");
		jsonrpc_fifo_mode = S_IRUSR | S_IWUSR;
	}

	if(jsonrpc_fifo_uid_s) {
		if(user2uid(&jsonrpc_fifo_uid, &jsonrpc_fifo_gid, jsonrpc_fifo_uid_s)
				< 0) {
			LM_ERR("Bad user name %s\n", jsonrpc_fifo_uid_s);
			return -1;
		}
	}

	if(jsonrpc_fifo_gid_s) {
		if(group2gid(&jsonrpc_fifo_gid, jsonrpc_fifo_gid_s) < 0) {
			LM_ERR("Bad group name %s\n", jsonrpc_fifo_gid_s);
			return -1;
		}
	}


	_jsonrpcs_fifo_stream =
			jsonrpc_init_fifo_server(jsonrpc_fifo, jsonrpc_fifo_mode,
					jsonrpc_fifo_uid, jsonrpc_fifo_gid, jsonrpc_fifo_reply_dir);
	if(_jsonrpcs_fifo_stream == NULL) {
		LM_CRIT("failed to init jsonrpc fifo server file stream\n");
		return -1;
	}

	/* add space for one extra process */
	register_procs(1);

	/* add child to update local config framework structures */
	cfg_register_child(1);

	return 0;
}

/*! \brief Read input on fifo */
int jsonrpc_read_stream(char *b, int max, FILE *stream, int *lread)
{
	int retry_cnt;
	int len;
	char *p;
	int sstate;
	int pcount;
	int pfound;
	int stype;

	sstate = 0;
	retry_cnt = 0;

	*lread = 0;
	p = b;
	pcount = 0;
	pfound = 0;
	stype = 0;

	while(1) {
		len = fread(p, 1, 1, stream);
		if(len == 0) {
			LM_ERR("fifo server fread failed: %s\n", strerror(errno));
			/* on Linux, sometimes returns ESPIPE -- give
			   it few more chances
			*/
			if(errno == ESPIPE) {
				retry_cnt++;
				if(retry_cnt > 4)
					return -1;
				continue;
			}
			/* interrupted by signal or ... */
			if((errno == EINTR) || (errno == EAGAIN))
				continue;
			return -1;
		}
		if(*p == '"' && (sstate == 0 || stype == 1)) {
			if(*lread > 0) {
				if(*(p - 1) != '\\') {
					sstate = (sstate + 1) % 2;
					stype = 1;
				}
			} else {
				sstate = (sstate + 1) % 2;
				stype = 1;
			}
		} else if(*p == '\'' && (sstate == 0 || stype == 2)) {
			if(*lread > 0) {
				if(*(p - 1) != '\\') {
					sstate = (sstate + 1) % 2;
					stype = 2;
				}
			} else {
				sstate = (sstate + 1) % 2;
				stype = 2;
			}
		} else if(*p == '{') {
			if(sstate == 0) {
				pfound = 1;
				pcount++;
			}
		} else if(*p == '}') {
			if(sstate == 0)
				pcount--;
		}
		*lread = *lread + 1;
		if(*lread >= max - 1) {
			LM_WARN("input data too large (%d)\n", *lread);
			return -1;
		}
		p++;
		if(pfound == 1 && pcount == 0) {
			*p = 0;
			return 0;
		}
	}

	return -1;
}

/*! \brief reply fifo security checks:
 *
 * checks if fd is a fifo, is not hardlinked and it's not a softlink
 * opened file descriptor + file name (for soft link check)
 * \return 0 if ok, <0 if not */
static int jsonrpc_fifo_check(int fd, char *fname)
{
	struct stat fst;
	struct stat lst;

	if(fstat(fd, &fst) < 0) {
		LM_ERR("security: fstat on %s failed: %s\n", fname, strerror(errno));
		return -1;
	}
	/* check if fifo */
	if(!S_ISFIFO(fst.st_mode)) {
		LM_ERR("security: %s is not a fifo\n", fname);
		return -1;
	}
	/* check if hard-linked */
	if(fst.st_nlink > 1) {
		LM_ERR("security: fifo_check: %s is hard-linked %d times\n", fname,
				(unsigned)fst.st_nlink);
		return -1;
	}

	/* lstat to check for soft links */
	if(lstat(fname, &lst) < 0) {
		LM_ERR("security: lstat on %s failed: %s\n", fname, strerror(errno));
		return -1;
	}
	if(S_ISLNK(lst.st_mode)) {
		LM_ERR("security: fifo_check: %s is a soft link\n", fname);
		return -1;
	}
	/* if this is not a symbolic link, check to see if the inode didn't
	 * change to avoid possible sym.link, rm sym.link & replace w/ fifo race
	 */
	if((lst.st_dev != fst.st_dev) || (lst.st_ino != fst.st_ino)) {
		LM_ERR("security: fifo_check: inode/dev number differ: %d %d (%s)\n",
				(int)fst.st_ino, (int)lst.st_ino, fname);
		return -1;
	}
	/* success */
	return 0;
}

#define JSONRPC_REPLY_RETRIES 4
FILE *jsonrpc_open_reply_fifo(str *srpath)
{
	int retries = JSONRPC_REPLY_RETRIES;
	int fifofd;
	FILE *file_handle;
	int flags;

	if(memchr(srpath->s, '.', srpath->len)
			|| memchr(srpath->s, '/', srpath->len)
			|| memchr(srpath->s, '\\', srpath->len)) {
		LM_ERR("Forbidden reply fifo filename: %.*s\n", srpath->len, srpath->s);
		return 0;
	}

	if(jsonrpc_reply_fifo_len + srpath->len + 1 > JSONRPC_MAX_FILENAME) {
		LM_ERR("Reply fifo filename too long %d\n",
				jsonrpc_reply_fifo_len + srpath->len);
		return 0;
	}

	memcpy(jsonrpc_reply_fifo_s + jsonrpc_reply_fifo_len, srpath->s,
			srpath->len);
	jsonrpc_reply_fifo_s[jsonrpc_reply_fifo_len + srpath->len] = 0;


tryagain:
	/* open non-blocking to make sure that a broken client will not
	 * block the FIFO server forever */
	fifofd = open(jsonrpc_reply_fifo_s, O_WRONLY | O_NONBLOCK);
	if(fifofd == -1) {
		/* retry several times if client is not yet ready for getting
		   feedback via a reply pipe
		*/
		if(errno == ENXIO) {
			/* give up on the client - we can't afford server blocking */
			if(retries == 0) {
				LM_ERR("no client at %s\n", jsonrpc_reply_fifo_s);
				return 0;
			}
			/* don't be noisy on the very first try */
			if(retries != JSONRPC_REPLY_RETRIES)
				LM_DBG("mi_fifo retry countdown: %d\n", retries);
			sleep_us(80000);
			retries--;
			goto tryagain;
		}
		/* some other opening error */
		LM_ERR("open error (%s): %s\n", jsonrpc_reply_fifo_s, strerror(errno));
		return 0;
	}

	/* security checks: is this really a fifo?, is
	 * it hardlinked? is it a soft link? */
	if(jsonrpc_fifo_check(fifofd, jsonrpc_reply_fifo_s) < 0)
		goto error;

	/* we want server blocking for big writes */
	if((flags = fcntl(fifofd, F_GETFL, 0)) < 0) {
		LM_ERR("pipe (%s): getfl failed: %s\n", jsonrpc_reply_fifo_s,
				strerror(errno));
		goto error;
	}
	flags &= ~O_NONBLOCK;
	if(fcntl(fifofd, F_SETFL, flags) < 0) {
		LM_ERR("pipe (%s): setfl cntl failed: %s\n", jsonrpc_reply_fifo_s,
				strerror(errno));
		goto error;
	}

	/* create an I/O stream */
	file_handle = fdopen(fifofd, "w");
	if(file_handle == NULL) {
		LM_ERR("open error (%s): %s\n", jsonrpc_reply_fifo_s, strerror(errno));
		goto error;
	}
	return file_handle;
error:
	close(fifofd);
	return 0;
}

#define JSONRPC_BUF_IN_SIZE 8192
static void jsonrpc_fifo_server(FILE *fifo_stream)
{
	FILE *reply_stream;
	char buf_in[JSONRPC_BUF_IN_SIZE];
	char buf_rpath[128];
	int lread;
	str scmd;
	str srpath;
	int nw;
	jsonrpc_plain_reply_t *jr = NULL;
	char buf_spath[128];
	char resbuf[JSONRPC_RESPONSE_STORING_BUFSIZE];
	str spath = STR_NULL;
	FILE *f = NULL;
	char *sid = NULL;

	while(1) {
		/* update the local config framework structures */
		cfg_update();

		reply_stream = NULL;
		lread = 0;
		if(jsonrpc_read_stream(buf_in, JSONRPC_BUF_IN_SIZE, fifo_stream, &lread)
						< 0
				|| lread <= 0) {
			LM_DBG("failed to get the json document from fifo stream\n");
			continue;
		}
		scmd.s = buf_in;
		scmd.len = lread;
		trim(&scmd);
		LM_DBG("preparing to execute fifo jsonrpc [%.*s]\n", scmd.len, scmd.s);
		srpath.s = buf_rpath;
		srpath.len = 128;
		spath.s = buf_spath;
		spath.len = 128;
		if(jsonrpc_exec_ex(&scmd, &srpath, &spath) < 0) {
			LM_ERR("failed to execute the json document from fifo stream\n");
			continue;
		}

		jr = jsonrpc_plain_reply_get();
		LM_DBG("command executed - result: [%.*s] [%d] [%p] [%.*s]\n",
				srpath.len, srpath.s, jr->rcode, jr->rbody.s, jr->rbody.len,
				jr->rbody.s);
		if(srpath.len > 0) {
			reply_stream = jsonrpc_open_reply_fifo(&srpath);
			if(reply_stream == NULL) {
				LM_ERR("cannot open reply fifo: %.*s\n", srpath.len, srpath.s);
				continue;
			}
			if(spath.len > 0) {
				sid = jsonrpcs_stored_id_get();
				f = fopen(spath.s, "w");
				if(f == NULL) {
					LM_ERR("cannot write to file: %.*s\n", spath.len, spath.s);
					snprintf(resbuf, JSONRPC_RESPONSE_STORING_BUFSIZE,
							JSONRPC_RESPONSE_STORING_FAILED, sid);
				} else {
					fwrite(jr->rbody.s, 1, jr->rbody.len, f);
					fclose(f);
					snprintf(resbuf, JSONRPC_RESPONSE_STORING_BUFSIZE,
							JSONRPC_RESPONSE_STORING_DONE, sid);
				}
				fwrite(resbuf, 1, strlen(resbuf), reply_stream);
				fclose(reply_stream);
				continue;
			}

			nw = fwrite(jr->rbody.s, 1, jr->rbody.len, reply_stream);
			if(nw < jr->rbody.len) {
				LM_ERR("failed to write the reply to fifo: %d out of %d\n", nw,
						jr->rbody.len);
			}
			fclose(reply_stream);
		}
	}
	return;
}

static void jsonrpc_fifo_process(int rank)
{
	LM_DBG("new process with pid = %d created\n", getpid());

	if(_jsonrpcs_fifo_stream == NULL) {
		LM_CRIT("fifo server stream not initialized\n");
		exit(-1);
	}

	jsonrpc_fifo_server(_jsonrpcs_fifo_stream);

	LM_CRIT("failed to run jsonrpc fifo server\n");
	exit(-1);
}

/**
 *
 */
int jsonrpc_fifo_mod_init(void)
{
	int len;
	int sep;
	char *p;

	if(jsonrpc_fifo == NULL || *jsonrpc_fifo == 0) {
		LM_ERR("no fifo file path provided\n");
		return -1;
	}

	if(*jsonrpc_fifo != '/') {
		if(runtime_dir != NULL && *runtime_dir != 0) {
			len = strlen(runtime_dir);
			sep = 0;
			if(runtime_dir[len - 1] != '/') {
				sep = 1;
			}
			len += sep + strlen(jsonrpc_fifo);
			p = pkg_malloc(len + 1);
			if(p == NULL) {
				LM_ERR("no more pkg\n");
				return -1;
			}
			strcpy(p, runtime_dir);
			if(sep)
				strcat(p, "/");
			strcat(p, jsonrpc_fifo);
			jsonrpc_fifo = p;
			LM_DBG("fifo path is [%s]\n", jsonrpc_fifo);
		}
	}

	if(jsonrpc_init_fifo_file() < 0) {
		LM_ERR("cannot initialize fifo transport\n");
		return -1;
	}

	return 0;
}

/**
 *
 */
int jsonrpc_fifo_child_init(int rank)
{
	int pid;

	if(jsonrpc_fifo == NULL) {
		LM_ERR("invalid fifo file path\n");
	}

	pid = fork_process(PROC_RPC, "JSONRPCS FIFO", 1);
	if(pid < 0) {
		return -1; /* error */
	}

	if(pid == 0) {
		/* child */

		/* initialize the config framework */
		if(cfg_child_init())
			return -1;

		jsonrpc_fifo_process(1);
	}

	return 0;
}

/**
 *
 */
int jsonrpc_fifo_destroy(void)
{
	int n;
	struct stat filestat;

	if(jsonrpc_fifo == NULL) {
		return 0;
	}

	/* destroying the fifo file */
	n = stat(jsonrpc_fifo, &filestat);
	if(n == 0) {
		/* FIFO exist, delete it (safer) if not config check */
		if(config_check == 0) {
			if(unlink(jsonrpc_fifo) < 0) {
				LM_ERR("cannot delete the fifo (%s): %s\n", jsonrpc_fifo,
						strerror(errno));
				goto error;
			}
		}
	} else if(n < 0 && errno != ENOENT) {
		LM_ERR("FIFO stat failed: %s\n", strerror(errno));
		goto error;
	}

	return 0;

error:
	return -1;
}
