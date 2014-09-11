/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2005 iptelorg GmbH
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
 *
 * Fifo server is a very powerful tool used to access easily
 * ser's internals via textual interface, similarly to
 * how internals of many operating systems are accessible
 * via the proc file system. This might be used for
 * making ser do things for you (such as initiating new
 * transaction from webpages) or inspect server's health.
 * 
 * FIFO server allows new functionality to be registered
 * with it -- thats what register_fifo_cmd is good for.
 * Remember, the initialization must take place before
 * forking; best in init_module functions. When a function
 * is registered, it can be always evoked by sending its
 * name prefixed by colon to the FIFO.
 *
 * There are few commands already implemented in core.
 * These are 'uptime' for looking at how long the server
 * is alive and 'print' for debugging purposes.
 *
 * Every command sent to FIFO must be sent atomically to
 * avoid intermixing with other commands and MUST be
 * terminated by empty line so that the server is to able
 * to find its end if it does not understand the command.
 *
 * File test/transaction.fifo illustrates example of use
 * of t_uac command (part of TM module).
 *
 * History:
 * --------
 *  2003-03-29  destroy pkg mem introduced (jiri)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-01-29  new built-in fifo commands: arg and pwd (jiri)
 *  2003-10-07  fifo security fixes: permissions, always delete old fifo,
 *               reply fifo checks -- added fifo_check (andrei)
 *  2003-10-13  added fifo_dir for reply fifos (andrei)
 *  2003-10-30  DB interface exported via FIFO (bogdan)
 *  2004-03-09  open_fifo_server split into init_ and start_ (andrei)
 *  2004-04-29  added chown(sock_user, sock_group)  (andrei)
 *  2004-06-06  updated to the new DB interface  & init_db_fifo (andrei)
 *  2004-09-19  fifo is deleted on exit (destroy_fifo)  (andrei)
 *  2005-03-02  meminfo fifo cmd added (andrei)
 */

#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#ifdef USE_TCP
#include <sys/socket.h>
#endif

#include "../../dprint.h"
#include "../../ut.h"
#include "../../error.h"
#include "../../config.h"
#include "../../globals.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../sr_module.h"
#include "../../pt.h"
#include "../../rpc.h"
#include "../../cfg/cfg_struct.h"
#include "fifo_server.h"


#define MAX_FIFO_COMMAND        128    /* Maximum length of a FIFO server command */
#define MAX_CONSUME_BUFFER     1024    /* Buffer dimensions for FIFO server */
#define MAX_LINE_BUFFER        2048    /* Maximum parameter line length */
#define DEFAULT_REPLY_RETRIES     4    /* Default number of reply write attempts */
#define DEFAULT_REPLY_WAIT    80000    /* How long we should wait for the client, in micro seconds */
#define DEFAULT_FIFO_DIR    "/tmp/"    /* Where reply pipes may be opened */


enum text_flags {
	CHUNK_SEEN         = (1 << 0),
	CHUNK_POSITIONAL   = (1 << 1), /* Positinal parameter, should be followed by \n */
	CHUNK_MEMBER_NAME  = (1 << 2), /* Struct member name, should be followed by : */
	CHUNK_MEMBER_VALUE = (1 << 3)  /* Struct member value, should be followed by , if
					* there is another member name and \n if not */
};
	

/*
 * Generit text chunk. Flags attribute contains arbitrary flags
 */
struct text_chunk {
	unsigned char flags;
	str s;
	struct text_chunk* next;
};


/* 
 * This is the parameter if rpc_struct_add 
 */
struct rpc_struct_out {
	struct rpc_context* ctx;
	struct text_chunk* line;
};


struct rpc_struct {
	struct rpc_context* ctx;
	struct text_chunk* names;  /* Names of elements */
	struct text_chunk* values; /* Element values as strings */
	struct rpc_struct* next;
};


/*
 * Context structure containing state of processing
 */
typedef struct rpc_context {
	char* method;               /* Request method name */
	char* reply_file;           /* Full path and name to the reply FIFO file */
	int reply_sent;             /* This flag ensures that we do not send a reply twice */
	int code;                   /* Reply code */
	char* reason;               /* Reason phrase */
	struct text_chunk* body;    /* First line to be appended as reply body */
	struct text_chunk* last;    /* Last body line */
	struct text_chunk* strs;    /* Strings to be collected at the end of processing */
	struct rpc_struct* structs; /* Structures to be collected at the end of processing */
} rpc_ctx_t;




/*
 * Module parameters
 */
char* fifo               = 0;                      /* FIFO name */
char* fifo_dir           = DEFAULT_FIFO_DIR;       /* dir where reply fifos are allowed */
char* fifo_user          = 0;                      /* Username of FIFO file owner */
char* fifo_group         = 0;                      /* Group name of FIFO file */
int   fifo_reply_retries = DEFAULT_REPLY_RETRIES;
int   fifo_reply_wait    = DEFAULT_REPLY_WAIT;
int   fifo_mode          = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP; /* Default FIFO file mode (rw-rw----) */


static pid_t     fifo_pid;          /* PID of the FIFO server process */
static int       fifo_uid    = -1;  /* User ID of owner of FIFO file */
static int       fifo_gid    = -1;  /* Group ID of FIFO file */
static int       fifo_read   = 0;   /* File descriptor for reading from FIFO file */
static int       fifo_write  = 0;   /* File descriptor for writing to FIFO reply file */
static int       line_no     = 0;   /* This is where we keep track of line numbers in the request */
static FILE*     fifo_stream = 0;   /* Convenience stream structure for reading from FIFO file */
static rpc_t     func_param;        /* Pointers to implementation of RPC funtions */
static rpc_ctx_t context;           /* Global context for processing of requests received
				     * through FIFO file */

static int  rpc_send         (rpc_ctx_t* ctx);                                 /* Send the reply to the client */
static void rpc_fault        (rpc_ctx_t* ctx,       int code, char* fmt, ...); /* Signal a failure to the client */
static int  rpc_add          (rpc_ctx_t* ctx,       char* fmt, ...);           /* Add a new piece of data to the result */
static int  rpc_scan         (rpc_ctx_t* ctx,       char* fmt, ...);           /* Retrieve request parameters */
static int  rpc_printf       (rpc_ctx_t* ctx,       char* fmt, ...);           /* Add printf-like formated data to the result set */
static int  rpc_struct_add   (struct text_chunk* s, char* fmt, ...);           /* Create a new structure */
static int  rpc_struct_scan  (struct rpc_struct* s, char* fmt, ...);           /* Scan attributes of a structure */
static int  rpc_struct_printf(struct text_chunk* s, char* name, char* fmt, ...);


/*
 * Escape string in buffer 'r' of length len. Write
 * the escaped string in buffer dst. The destination
 * buffer must exist and must be twice as big as the
 * input buffer.
 *
 * Parameter all controls the set of characters to be
 * escaped. If set to 1 then all characters, including
 * structure delimiters, will be escaped. If set to
 * 0 then only line delimiters, tab and zero will be
 * escaped.
 */
static void escape(str* dst, char* r, int len, int all)
{
	int i;
	char* w;
	if (!len) { 
		dst->len = 0;
		return;
	}

	w = dst->s;
	for(i = 0; i < len; i++) {
		switch(r[i]) {
		case '\n': *w++ = '\\'; *w++ = 'n';  break;
		case '\r': *w++ = '\\'; *w++ = 'r';  break;
		case '\t': *w++ = '\\'; *w++ = 't';  break;
		case '\\': *w++ = '\\'; *w++ = '\\'; break;
		case '\0': *w++ = '\\'; *w++ = '0';  break;
		case ':': 
			if (all) {
				*w++ = '\\';
				*w++ = 'o';
			} else *w++ = r[i];
			break;
			
		case ',':
			if (all) {
				*w++ = '\\';
				*w++ = 'c';
			} else *w++ = r[i];
			break;

		default:
			*w++ = r[i];
			break;
		}
	}
	dst->len = w - dst->s;
}


/*
 * Unescape the string in buffer 'r' of length len.
 * The resulting string will be stored in buffer dst
 * which must exist and must be at least as big as
 * the source buffer. The function will update dst->len
 * to the length of the resulting string.
 *
 * Return value 0 indicates success, -1 indicates
 * formatting error.
 */
static int unescape(str* dst, char* r, int len)
{
	char* w;
	int i;

	if (!len) {
		dst->len = 0;
		return 0;
	}

	w = dst->s;
	for(i = 0; i < len; i++) {
		switch(*r) {
		case '\\':
			r++;
			i++;
			switch(*r++) {
			case '\\': *w++ = '\\'; break;
			case 'n':  *w++ = '\n'; break;
			case 'r':  *w++ = '\r'; break;
			case 't':  *w++ = '\t'; break;
			case '0':  *w++ = '\0'; break;
			case 'c':  *w++ = ':';  break; /* Structure delimiter */
			case 'o':  *w++ = ',';  break; /* Structure delimiter */
			default:   return -1;
			}
			break;

		default: *w++ = *r++; break;
		}
	}
	dst->len = w - dst->s;
	return 0;
}


/*
 * Create a new text chunk, the input text will
 * be escaped.
 */
struct text_chunk* new_chunk_escape(str* src, int escape_all)
{
	struct text_chunk* l;
	if (!src) return 0;

        l = pkg_malloc(sizeof(struct text_chunk));
	if (!l) {
		ERR("No Memory Left\n");
		return 0;
	}
	l->s.s = pkg_malloc(src->len * 2 + 1);
	if (!l->s.s) {
		ERR("No Memory Left\n");
		pkg_free(l);
		return 0;
	}
	l->next = 0;
	l->flags = 0;
	escape(&l->s, src->s, src->len, escape_all);
	l->s.s[l->s.len] = '\0';
	return l;
}

/*
 * Create a new text chunk, the input text
 * will not be escaped. The function returns
 * 0 on an error
 */
struct text_chunk* new_chunk(str* src)
{
	struct text_chunk* l;
	if (!src) return 0;

        l = pkg_malloc(sizeof(struct text_chunk));
	if (!l) {
		ERR("No Memory Left\n");
		return 0;
	}
	l->s.s = pkg_malloc(src->len + 1);
	if (!l->s.s) {
		ERR("No Memory Left\n");
		pkg_free(l);
		return 0;
	}
	l->next = 0;
	l->flags = 0;
	memcpy(l->s.s, src->s, src->len);
	l->s.len = src->len;
	l->s.s[l->s.len] = '\0';
	return l;
}


/*
 * Create a new text chunk, the input text
 * will be unescaped first.
 */
struct text_chunk* new_chunk_unescape(str* src)
{
	struct text_chunk* l;
	if (!src) return 0;

        l = pkg_malloc(sizeof(struct text_chunk));
	if (!l) {
		ERR("No Memory Left\n");
		return 0;
	}
	l->s.s = pkg_malloc(src->len + 1);
	if (!l->s.s) {
		ERR("No Memory Left\n");
		pkg_free(l);
		return 0;
	}
	l->next = 0;
	l->flags = 0;
	if (unescape(&l->s, src->s, src->len) < 0) {
		pkg_free(l->s.s);
		pkg_free(l);
		return 0;
	}
	l->s.s[l->s.len] = '\0';
	return l;
}


static void free_chunk(struct text_chunk* c)
{
	if (c && c->s.s) pkg_free(c->s.s);
	if (c) pkg_free(c);
}


static void free_struct(struct rpc_struct* s)
{
	struct text_chunk* c;

	if (!s) return;
	while(s->names) {
		c = s->names;
		s->names = s->names->next;
		free_chunk(c);
	}

	while(s->values) {
		c = s->values;
		s->values = s->values->next;
		free_chunk(c);
	}

	pkg_free(s);
}


/*
 * Parse a structure
 */
static struct rpc_struct* new_struct(rpc_ctx_t* ctx, str* line)
{
	char* comma, *colon;
	struct rpc_struct* s;
	str left, right = STR_NULL, name, value;
	struct text_chunk* n, *v;

	if (!line->len) {
		rpc_fault(ctx, 400, "Line %d Empty - Structure Expected", line_no);
		return 0;
	}

	s = (struct rpc_struct*)pkg_malloc(sizeof(struct rpc_struct));
	if (!s) {
		rpc_fault(ctx, 500, "Internal Server Error (No Memory Left)");
		return 0;
	}
	memset(s, 0, sizeof(struct rpc_struct));
	s->ctx = ctx;
	
	left = *line;
	do {
		comma = q_memchr(left.s, ',', left.len);
		if (comma) {
			right.s = comma + 1;
			right.len = left.len - (comma - left.s) - 1;
			left.len = comma - left.s;
		}
		
		     /* Split the record to name and value */
		colon = q_memchr(left.s, ':', left.len);
		if (!colon) {
			rpc_fault(ctx, 400, "Colon missing in struct on line %d", line_no);
			goto err;;
		}
		name.s = left.s;
		name.len = colon - name.s;
		value.s = colon + 1;
		value.len = left.len - (colon - left.s) - 1;
		
		     /* Create name chunk */
		n = new_chunk_unescape(&name);
		if (!n) {
			rpc_fault(ctx, 400, "Error while processing struct member '%.*s' on line %d", 
				  name.len, ZSW(name.s), line_no);
			goto err;
		}
		n->next = s->names;
		s->names = n;

		     /* Create value chunk */
		v = new_chunk_unescape(&value);
		if (!v) {
			rpc_fault(ctx, 400, "Error while processing struct membeer '%.*s' on line %d",
				  name.len, ZSW(name.s), line_no);
			goto err;
		}
		v->next = s->values;
		s->values = v;

		left = right;
	} while(comma);

	return s;
 err:
	if (s) free_struct(s);
	return 0;
}


/*
 * Read a line from FIFO file and store the data in buffer 'b'
 * and the legnth of the line in variable 'read'
 *
 * Returns -1 on error, 0 on success
 */
static int read_line(char *b, int max, FILE *stream, int *read)
{
	int len, retry_cnt;
	
	retry_cnt = 0;
	
 retry:
	if (fgets(b, max, stream) == NULL) {
		ERR("fifo_server fgets failed: %s\n",
		    strerror(errno));
		     /* on Linux, fgets sometimes returns ESPIPE -- give
		      * it few more chances
		      */
		if (errno == ESPIPE) {
			retry_cnt++;
			if (retry_cnt<4) goto retry;
		}
		     /* interrupted by signal or ... */
		if ((errno == EINTR) || (errno == EAGAIN)) goto retry;
		kill(0, SIGTERM);
	}
	     /* if we did not read whole line, our buffer is too small
	      * and we cannot process the request; consume the remainder of 
	      * request
	      */
	len = strlen(b);
	if (len && !(b[len - 1] == '\n' || b[len - 1] == '\r')) {
	        ERR("Request line too long\n");
		return -1;
	}
	     /* trim from right */
	while(len) {
		if(b[len - 1] == '\n' || b[len - 1] == '\r'
		   || b[len - 1] == ' ' || b[len - 1] == '\t') {
			len--;
			b[len] = 0;
		} else break;
	}
	*read = len;
	line_no++;
	return 0;
}


/*
 * Remove directory path from filename and replace it
 * with the path configured through a module parameter.
 * 
 * The result is allocated using pkg_malloc and thus
 * has to be freed using pkg_free
 */
static char *trim_filename(char * file)
{
	int prefix_len, fn_len;
	char *new_fn;
	
	/* we only allow files in "/tmp" -- any directory
	 * changes are not welcome
	 */
	if (strchr(file, '.') || strchr(file, '/')
	    || strchr(file, '\\')) {
		ERR("Forbidden filename: %s\n"
		    , file);
		return 0;
	}
	prefix_len = strlen(fifo_dir); fn_len = strlen(file);
	new_fn = pkg_malloc(prefix_len + fn_len + 1);
	if (new_fn == 0) {
		ERR("No memory left\n");
		return 0;
	}

	memcpy(new_fn, fifo_dir, prefix_len);
	memcpy(new_fn + prefix_len, file, fn_len);
	new_fn[prefix_len + fn_len] = 0;
	return new_fn;
}


/*
 * Consume pending data in FIFO file
 */
static void consume_request(FILE *stream)
{
	int len;
	static char buffer[MAX_CONSUME_BUFFER];
	while(read_line(buffer, MAX_CONSUME_BUFFER, stream, &len) < 0);

}


/* reply fifo security checks:
 * checks if fd is a fifo, is not hardlinked and it's not a softlink
 * opened file descriptor + file name (for soft link check)
 * returns 0 if ok, <0 if not 
 */
static int fifo_check(int fd, char* fname)
{
	struct stat fst;
	struct stat lst;
	
	if (fstat(fd, &fst) < 0) {
		ERR("fstat failed: %s\n",
		    strerror(errno));
		return -1;
	}
	     /* check if fifo */
	if (!S_ISFIFO(fst.st_mode)){
		ERR("%s is not a fifo\n", fname);
		return -1;
	}
	     /* check if hard-linked */
	if (fst.st_nlink > 1) {
		ERR("%s is hard-linked %d times\n",
		    fname, (unsigned)fst.st_nlink);
		return -1;
	}
	
	     /* lstat to check for soft links */
	if (lstat(fname, &lst) < 0) {
		ERR("lstat failed: %s\n",
		    strerror(errno));
		return -1;
	}
	if (S_ISLNK(lst.st_mode)) {
		ERR("%s is a soft link\n", fname);
		return -1;
	}
	     /* if this is not a symbolic link, check to see if the inode didn't
	      * change to avoid possible sym.link, rm sym.link & replace w/ fifo race
	      */
	if ((lst.st_dev != fst.st_dev) || (lst.st_ino != fst.st_ino)) {
		ERR("inode/dev number differ : %d %d (%s)\n",
		    (int)fst.st_ino, (int)lst.st_ino, fname);
		return -1;
	}
	     /* success */
	return 0;
}


/*
 * Open the FIFO reply file
 */
static FILE *open_reply_pipe(char *pipe_name)
{
	
	int fifofd;
	FILE *file_handle;
	int flags;
	
	int retries = fifo_reply_retries;
	
	if (!pipe_name || *pipe_name == 0) {
		DBG("No file to write to about missing cmd\n");
		return 0;
	}
	
 tryagain:
	     /* open non-blocking to make sure that a broken client will not 
	      * block the FIFO server forever */
	fifofd = open(pipe_name, O_WRONLY | O_NONBLOCK);
	if (fifofd == -1) {
		     /* retry several times if client is not yet ready for getting
		      * feedback via a reply pipe
		      */
		if (errno == ENXIO) {
			     /* give up on the client - we can't afford server blocking */
			if (retries == 0) {
				ERR("No client at %s\n", pipe_name);
				return 0;
			}
			     /* don't be noisy on the very first try */
			if (retries != fifo_reply_retries) {
				DBG("Retry countdown: %d\n", retries);
			}
			sleep_us(fifo_reply_wait);
			retries--;
			goto tryagain;
		}
		     /* some other opening error */
		ERR("Open error (%s): %s\n",
		    pipe_name, strerror(errno));
		return 0;
	}
	     /* security checks: is this really a fifo?, is 
	      * it hardlinked? is it a soft link? */
	if (fifo_check(fifofd, pipe_name) < 0) goto error;
	
	     /* we want server blocking for big writes */
	if ((flags = fcntl(fifofd, F_GETFL, 0)) < 0) {
		ERR("(%s): getfl failed: %s\n", pipe_name, strerror(errno));
		goto error;
	}
	flags &= ~O_NONBLOCK;
	if (fcntl(fifofd, F_SETFL, flags) < 0) {
		ERR("(%s): setfl cntl failed: %s\n",
		    pipe_name, strerror(errno));
		goto error;
	}
	
	     /* create an I/O stream */	
	file_handle = fdopen( fifofd, "w");
	if (file_handle == NULL) {
		ERR("Open error (%s): %s\n",
		    pipe_name, strerror(errno));
		goto error;
	}
	return file_handle;
 error:
	close(fifofd);
	return 0;
}


/*
 * Man FIFO server routine running in the FIFO
 * server process, processes requests received
 * through the FIFO file repeatedly, never returns
 */
static void fifo_server(FILE *fifo_stream)
{
	rpc_export_t* exp;
	static char buf[MAX_FIFO_COMMAND];
	int line_len;
	char *file_sep;
	struct text_chunk* p;
	struct rpc_struct* s;

	file_sep = 0;
	context.method = 0;
	context.reply_file = 0;
	context.body = 0;
		
	     /* register a diagnostic FIFO command */
	while(1) {
		context.code = 200;
		context.reason = "OK";
		context.reply_sent = 0;
		context.last = 0;
		line_no = 0;
		     /* commands must look this way ':<command>:[filename]' */
		if (read_line(buf, MAX_FIFO_COMMAND, fifo_stream, &line_len) < 0) {
			     /* line breaking must have failed -- consume the rest
			      * and proceed to a new request
			      */
			ERR("Command expected\n");
			goto consume;
		}
		if (line_len == 0) {
			INFO("Empty command received\n");
			continue;
		}
		if (line_len < 3) {
			ERR("Command must have at least 3 chars\n");
			goto consume;
		}
		if (*buf != CMD_SEPARATOR) {
			ERR("Command must begin with %c: %.*s\n", 
			    CMD_SEPARATOR, line_len, buf);
			goto consume;
		}

		context.method = buf + 1;
		file_sep = strchr(context.method, CMD_SEPARATOR);
		if (file_sep == NULL) {
			ERR("File separator missing\n");
			goto consume;
		}
		if (file_sep == context.method) {
			ERR("Empty command\n");
			goto consume;
		}
		if (*(file_sep + 1) == 0) context.reply_file = NULL; 
		else {
			context.reply_file = file_sep + 1;
			context.reply_file = trim_filename(context.reply_file);
			if (context.reply_file == 0) {
				ERR("Trimming filename\n");
				goto consume;
			}
		}
		     /* make command zero-terminated */
		*file_sep = 0;

		/* update the local config */
		cfg_update();

		exp = find_rpc_export(context.method, 0);
		if (!exp || !exp->function) {
			DBG("Command %s not found\n", context.method);
			rpc_fault(&context, 500, "Command '%s' not found", context.method);
			goto consume;
		}

		exp->function(&func_param, &context);

	consume:
		if (!context.reply_sent) {
			rpc_send(&context);
		}

		if (context.reply_file) { 
			pkg_free(context.reply_file); 
			context.reply_file = 0; 
		}
		
		     /* Collect garbage (unescaped strings and structures) */
		while(context.strs) {
			p = context.strs;
			context.strs = context.strs->next;
			free_chunk(p);
		}

		while(context.structs) {
			s = context.structs;
			context.structs = context.structs->next;
			free_struct(s);
		}

		consume_request(fifo_stream);
		DBG("Command consumed\n");
	}
}


/*
 * Initialze the FIFO server, translated usernames to uid,
 * Make sure that we can create and open the FIFO file and
 * make it secure. This function must be executed from mod_init.
 * This ensures that it has sufficient privileges.
 */
int init_fifo_server(void)
{
	struct stat filestat;
	int n;
	long opt;
	
	if (fifo == NULL) {
		DBG("No fifo will be opened\n");
		/* everything is ok, we just do not want to start */
		return 1;
	}
	if (strlen(fifo) == 0) {
		DBG("Fifo disabled\n");
		return 1;
	}
	
	     /* fix sock/fifo uid/gid */
	if (fifo_user) {
		if (user2uid(&fifo_uid, 0, fifo_user) < 0) {
			ERR("Bad socket user name/uid number %s\n", fifo_user);
			return -1;
		}
	}
	if (fifo_group) {
		if (group2gid(&fifo_gid, fifo_group) < 0) {
			ERR("Bad group name/gid number: -u %s\n", fifo_group);
			return -1;
		}
	}
	
	DBG("Opening fifo...\n");
	n = stat(fifo, &filestat);
	if (n == 0) {
		     /* FIFO exist, delete it (safer) */
		if (unlink(fifo) < 0) {
			ERR("Cannot delete old fifo (%s):"
			    " %s\n", fifo, strerror(errno));
			return -1;
		}
	} else if (n < 0 && errno != ENOENT) {
		ERR("FIFO stat failed: %s\n",
		    strerror(errno));
	}
	     /* create FIFO ... */
	if ((mkfifo(fifo, fifo_mode) < 0)) {
		ERR("Can't create FIFO: "
		    "%s (mode=%d)\n",
		    strerror(errno), fifo_mode);
		return -1;
	} 
	DBG("FIFO created @ %s\n", fifo );
	if ((chmod(fifo, fifo_mode) < 0)) {
		ERR("Can't chmod FIFO: %s (mode=%d)\n",
		    strerror(errno), fifo_mode);
		return -1;
	}
	if ((fifo_uid != -1) || (fifo_gid != -1)) {
		if (chown(fifo, fifo_uid, fifo_gid) < 0) {
			ERR("Failed to change the owner/group for %s  to %d.%d; %s[%d]\n",
			    fifo, fifo_uid, fifo_gid, strerror(errno), errno);
			return -1;
		}
	}
	
	DBG("fifo %s opened, mode=%d\n", fifo, fifo_mode);
	
	fifo_read = open(fifo, O_RDONLY | O_NONBLOCK, 0);
	if (fifo_read < 0) {
		ERR("fifo_read did not open: %s\n",
		    strerror(errno));
		return -1;
	}
	fifo_stream = fdopen(fifo_read, "r");
	if (fifo_stream == NULL) {
		ERR("fdopen failed: %s\n",
		    strerror(errno));
		return -1;
	}
	     /* make sure the read fifo will not close */
	fifo_write = open(fifo, O_WRONLY | O_NONBLOCK, 0);
	if (fifo_write < 0) {
		ERR("fifo_write did not open: %s\n",
		    strerror(errno));
		return -1;
	}
	     /* set read fifo blocking mode */
	if ((opt = fcntl(fifo_read, F_GETFL)) == -1) {
		ERR("fcntl(F_GETFL) failed: %s [%d]\n",
		    strerror(errno), errno);
		return -1;
	}
	if (fcntl(fifo_read, F_SETFL, opt & (~O_NONBLOCK)) == -1) {
		ERR("fcntl(F_SETFL) failed: %s [%d]\n",
		    strerror(errno), errno);
		return -1;
	}

	func_param.send = (rpc_send_f)rpc_send;
	func_param.fault = (rpc_fault_f)rpc_fault;
	func_param.add = (rpc_add_f)rpc_add;
	func_param.scan = (rpc_scan_f)rpc_scan;
	func_param.printf = (rpc_printf_f)rpc_printf;
	func_param.struct_add = (rpc_struct_add_f)rpc_struct_add;
	func_param.struct_scan = (rpc_struct_scan_f)rpc_struct_scan;	
	func_param.struct_printf = (rpc_struct_printf_f)rpc_struct_printf;
	return 0;
}


/*
 * Create a new process that will be processing requests received
 * through the FIFO file. This must be executed from child_init
 * with rank PROC_MAIN.
 */
int start_fifo_server(void)
{
	if (fifo_stream == 0) return 1; /* no error, we just don't start it */

	fifo_pid = fork_process(PROC_FIFO,"fifo server",1);
	if (fifo_pid < 0) {
		ERR("Failed to fork: %s\n", strerror(errno));
		return -1;
	}
	if (fifo_pid == 0) { /* child == FIFO server */
		is_main = 0;
		INFO("fifo process starting: %d\n", getpid());
		/* a real server doesn't die if writing to reply fifo fails */
    	signal(SIGPIPE, SIG_IGN);
		INFO("fifo server up at %s...\n",
		     fifo);

		/* initialize the config framework */
		if (cfg_child_init()) return -1;

		fifo_server(fifo_stream); /* never returns */
	}
	     /* dad process */
	snprintf(pt[process_no].desc, MAX_PT_DESC, 
		 "fifo server @ %s", fifo);
	return 1;
}


/*
 * Close and unlink the FIFO file
 */
void destroy_fifo(void)
{
	if (!fifo_stream) return;
	fclose(fifo_stream);
	fifo_stream = 0;
	     /* if  FIFO was created, delete it */
	if (fifo && strlen(fifo)) {
		if (unlink(fifo) < 0) {
			WARN("Cannot delete fifo (%s):"
			     " %s\n", fifo, strerror(errno));
		}
	}
}


#define REASON_BUF_LEN 1024


/*
 * An error occurred, signal it to the client
 */
static void rpc_fault(rpc_ctx_t* ctx, int code, char* fmt, ...)
{
	static char buf[REASON_BUF_LEN];
	va_list ap;
	ctx->code = code;
	va_start(ap, fmt);
	vsnprintf(buf, REASON_BUF_LEN, fmt, ap);
	va_end(ap);
	ctx->reason = buf;
}


static inline int safe_write(FILE* f, char* fmt, ...)
{
	va_list ap;
	
	if (!*fmt) return 0;
	va_start(ap, fmt);

 retry:
	     /* First line containing code and reason phrase */
	if (vfprintf(f, fmt, ap) <= 0) {
		ERR("Write error (%s): %s\n", fifo, strerror(errno));
		if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			goto retry;
		}
		va_end(ap);
		return -1;
	}
	va_end(ap);
	return 0;
}


/*
 * Send a reply, either positive or negative, to the client
 */
static int rpc_send(rpc_ctx_t* ctx) 
{
	struct text_chunk* p;
	FILE *f;

	     /* Send the reply only once */
	if (ctx->reply_sent) return 1;
	else ctx->reply_sent = 1;

	     /* Open the reply file */
	f = open_reply_pipe(ctx->reply_file);
	if (f == 0) {
		ERR("No reply pipe %s\n", ctx->reply_file);
		return -1;
	}
	
	     /* First line containing code and reason phrase */
	safe_write(f, "%d %s\n", ctx->code, ctx->reason);

	     /* Send the body */
	while(ctx->body) {
		p = ctx->body;
		ctx->body = ctx->body->next;
		if (p->s.len) safe_write(f, "%.*s", p->s.len, ZSW(p->s.s));
		if (p->flags & CHUNK_POSITIONAL) {
			safe_write(f, "\n");
		} else if (p->flags & CHUNK_MEMBER_NAME) {
			safe_write(f, ":");
		} else if (p->flags & CHUNK_MEMBER_VALUE) {
			if (p->next && p->next->flags & CHUNK_MEMBER_NAME) {
				safe_write(f, ",");
			} else {
				safe_write(f, "\n");
			}
		}
		free_chunk(p);
	}

	fclose(f);
	return 0;
}


/*
 * Add a chunk to reply
 */
static void append_chunk(rpc_ctx_t* ctx, struct text_chunk* l)
{
	if (!ctx->last) {
		ctx->body = l;
		ctx->last = l;
	} else {
		ctx->last->next = l;
		ctx->last = l;
	}
}


/*
 * Convert a value to data chunk and add it to reply
 */
static int print_value(rpc_ctx_t* ctx, char fmt, va_list* ap)
{
	struct text_chunk* l;
	str str_val;
	str* sp;
	char buf[256];

	switch(fmt) {
	case 'd':
	case 't':
		str_val.s = int2str(va_arg(*ap, int), &str_val.len);
		l = new_chunk(&str_val);
		if (!l) {
			rpc_fault(ctx, 500, "Internal server error while processing line %d", line_no);
			goto err;
		}
		break;
		
	case 'f':
		str_val.s = buf;
		str_val.len = snprintf(buf, 256, "%f", va_arg(*ap, double));
		if (str_val.len < 0) {
			rpc_fault(ctx, 400, "Error While Converting double");
			ERR("Error while converting double\n");
			goto err;
		}
		l = new_chunk(&str_val);
		if (!l) {
			rpc_fault(ctx, 500, "Internal Server Error, line %d", line_no);
			goto err;
		}
		break;
		
	case 'b':
		str_val.len = 1;
		str_val.s = ((va_arg(*ap, int) == 0) ? "0" : "1");
		l = new_chunk(&str_val);
		if (!l) {
			rpc_fault(ctx, 500, "Internal Server Error, line %d", line_no);
			goto err;
		}
		break;
				
	case 's':
		str_val.s = va_arg(*ap, char*);
		str_val.len = strlen(str_val.s);
		l = new_chunk_escape(&str_val, 0);
		if (!l) {
			rpc_fault(ctx, 500, "Internal Server Error, line %d", line_no);
			goto err;
		}
		break;
		
	case 'S':
		sp = va_arg(*ap, str*);
		l = new_chunk_escape(sp, 0);
		if (!l) {
			rpc_fault(ctx, 500, "Internal Server Error, line %d", line_no);
			goto err;
		}
		break;
		
	default:
		rpc_fault(ctx, 500, "Bug In SER (Invalid formatting character %c)", fmt);
		ERR("Invalid formatting character\n");
		goto err;
	}

	l->flags |= CHUNK_POSITIONAL;
	append_chunk(ctx, l);
	return 0;
 err:
	return -1;
}


static int rpc_add(rpc_ctx_t* ctx, char* fmt, ...)
{
	void** void_ptr;
	va_list ap;
	str s = {"", 0};
	struct text_chunk* l;

	va_start(ap, fmt);
	while(*fmt) {
		if (*fmt == '{') {
			void_ptr = va_arg(ap, void**);
			l = new_chunk(&s);
			if (!l) {
				rpc_fault(ctx, 500, "Internal Server Error");
				goto err;
			}
			append_chunk(ctx, l);
			*void_ptr = l;
		} else {
			if (print_value(ctx, *fmt, &ap) < 0) goto err;
		}
		fmt++;
	}
	va_end(ap);
	return 0;
 err:
	va_end(ap);
	return -1;
}

#define RPC_BUF_SIZE 1024

static int rpc_struct_printf(struct text_chunk* c, char* name, char* fmt, ...)
{
	int n, buf_size;
	char* buf;
	va_list ap;
	str s, nm;
	struct text_chunk* l, *m;

	buf = (char*)pkg_malloc(RPC_BUF_SIZE);
	if (!buf) {
		rpc_fault(&context,  500, "Internal Server Error (No memory left)");
		ERR("No memory left\n");
		return -1;
	}
	
	buf_size = RPC_BUF_SIZE;
	while (1) {
		     /* Try to print in the allocated space. */
		va_start(ap, fmt);
		n = vsnprintf(buf, buf_size, fmt, ap);
		va_end(ap);
		     /* If that worked, return the string. */
		if (n > -1 && n < buf_size) {
			nm.s = name;
			nm.len = strlen(name);
			m = new_chunk_escape(&nm, 1); /* Escape all characters, including : and , */
			if (!m) {
				rpc_fault(&context, 500, "Internal Server Error");
				goto err;
			}

			s.s = buf;
			s.len = n;
			l = new_chunk_escape(&s, 1);
			if (!l) {
				rpc_fault(&context, 500, "Internal Server Error");
				free_chunk(m);
				ERR("Error while creating text_chunk structure");
				goto err;
			}
			
			l->flags |= CHUNK_MEMBER_VALUE;
			l->next = c->next;
			c->next = l;
			if (c == context.last) context.last = l;

			m->flags |= CHUNK_MEMBER_NAME;
			m->next = c->next;
			c->next = m;
			if (c == context.last) context.last = m;
			return 0;
		}
		     /* Else try again with more space. */
		if (n > -1) {   /* glibc 2.1 */
			buf_size = n + 1; /* precisely what is needed */
		} else {          /* glibc 2.0 */
			buf_size *= 2;  /* twice the old size */
		}
		if ((buf = pkg_realloc(buf, buf_size)) == 0) {
			rpc_fault(&context, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			goto err;
		}
	}
	return 0;
 err:
	if (buf) pkg_free(buf);
	return -1;
}


static int rpc_printf(rpc_ctx_t* ctx, char* fmt, ...)
{
	int n, buf_size;
	char* buf;
	va_list ap;
	str s;
	struct text_chunk* l;

	buf = (char*)pkg_malloc(RPC_BUF_SIZE);
	if (!buf) {
		rpc_fault(ctx,  500, "Internal Server Error (No memory left)");
		ERR("No memory left\n");
		return -1;
	}
	
	buf_size = RPC_BUF_SIZE;
	while (1) {
		     /* Try to print in the allocated space. */
		va_start(ap, fmt);
		n = vsnprintf(buf, buf_size, fmt, ap);
		va_end(ap);
		     /* If that worked, return the string. */
		if (n > -1 && n < buf_size) {
			s.s = buf;
			s.len = n;
			l = new_chunk_escape(&s, 0);
			if (!l) {
				rpc_fault(ctx, 500, "Internal Server Error");
				ERR("Error while creating text_chunk structure");
				goto err;
			}
			append_chunk(ctx, l);
			pkg_free(buf);
			return 0;
		}
		     /* Else try again with more space. */
		if (n > -1) {   /* glibc 2.1 */
			buf_size = n + 1; /* precisely what is needed */
		} else {          /* glibc 2.0 */
			buf_size *= 2;  /* twice the old size */
		}
		if ((buf = pkg_realloc(buf, buf_size)) == 0) {
			rpc_fault(ctx, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			goto err;
		}
	}
	return 0;
 err:
	if (buf) pkg_free(buf);
	return -1;
}


static int rpc_scan(rpc_ctx_t* ctx, char* fmt, ...)
{
	static char buf[MAX_LINE_BUFFER];
	struct text_chunk* l;
	struct rpc_struct* s;
	int* int_ptr;
	char** char_ptr;
	str* str_ptr;
	double* double_ptr;
	void** void_ptr;
	int read;
	str line;

	va_list ap;
	va_start(ap, fmt);
	line.s = buf;

	read = 0;
	while(*fmt) {
		if (read_line(line.s, MAX_LINE_BUFFER, fifo_stream, &line.len) < 0) {
			va_end(ap);
			return read;
		}

		switch(*fmt) {
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			if (!line.len) {
				rpc_fault(ctx, 400, "Invalid parameter value on line %d", line_no);
				goto error;
			}
			int_ptr = va_arg(ap, int*);
			*int_ptr = strtol(line.s, 0, 0);
			break;

		case 'f': /* double */
			if (!line.len) {
				rpc_fault(ctx, 400, "Invalid parameter value on line %d", line_no);
				goto error;
			}
			double_ptr = va_arg(ap, double*);
			*double_ptr = strtod(line.s, 0);
			break;
			
		case 's': /* zero terminated string */
		case 'S': /* str structure */
			l = new_chunk_unescape(&line);
			if (!l) {
				rpc_fault(ctx, 500, "Internal Server Error");
				ERR("Not enough memory\n");
				goto error;
			}
			     /* Make sure it gets released at the end */
			l->next = ctx->strs;
			ctx->strs = l;

			if (*fmt == 's') {
				char_ptr = va_arg(ap, char**);
				*char_ptr = l->s.s;
			} else {
				str_ptr = va_arg(ap, str*);
				*str_ptr = l->s;
			}
			break;

		case '{':
			void_ptr = va_arg(ap, void**);
			s = new_struct(ctx, &line);
			if (!s) goto error;
			s->next = ctx->structs;
			ctx->structs = s;
			*void_ptr = s;
			break;

		default:
			ERR("Invalid parameter type in formatting string: %c\n", *fmt);
			rpc_fault(ctx, 500, "Server Internal Error (Invalid Formatting Character '%c')", *fmt);
			goto error;
		}
		fmt++;
		read++;
	}
	va_end(ap);
	return read;

 error:
	va_end(ap);
	return -read;
}



static int rpc_struct_add(struct text_chunk* s, char* fmt, ...)
{
	static char buf[MAX_LINE_BUFFER];
	str st, *sp;
	va_list ap;
	struct text_chunk* m, *c;

	va_start(ap, fmt);
	while(*fmt) {
		     /* Member name escaped */
		st.s = va_arg(ap, char*);
		st.len = strlen(st.s);
		m = new_chunk_escape(&st, 1); /* Escape all characters, including : and , */
		if (!m) {
			rpc_fault(&context, 500, "Internal Server Error");
			goto err;
		}
		m->flags |= CHUNK_MEMBER_NAME;
		
		switch(*fmt) {
		case 'd':
		case 't':
			st.s = int2str(va_arg(ap, int), &st.len);
			c = new_chunk(&st);
			break;
			
		case 'f':
			st.s = buf;
			st.len = snprintf(buf, 256, "%f", va_arg(ap, double));
			if (st.len < 0) {
				rpc_fault(&context, 400, "Error While Converting double");
				ERR("Error while converting double\n");
				goto err;
			}
			c = new_chunk(&st);
			break;
			
		case 'b':
			st.len = 1;
			st.s = ((va_arg(ap, int) == 0) ? "0" : "1");
			c = new_chunk(&st);
			break;
			
		case 's':
			st.s = va_arg(ap, char*);
			st.len = strlen(st.s);
			c = new_chunk_escape(&st, 1);
			break;
			
		case 'S':
			sp = va_arg(ap, str*);
			c = new_chunk_escape(sp, 1);
			break;
			
		default:
			rpc_fault(&context, 500, "Bug In SER (Invalid formatting character %c)", *fmt);
			ERR("Invalid formatting character\n");
			goto err;
		}

		if (!c) {
			rpc_fault(&context, 500, "Internal Server Error");
			goto err;
		}
		c->flags |= CHUNK_MEMBER_VALUE;
		c->next = s->next;
		s->next = c;
		if (s == context.last) context.last = c;

		m->next = s->next;
		s->next = m;
		if (s == context.last) context.last = m;

		fmt++;
	}
	va_end(ap);
	return 0;
 err:
	if (m) free_chunk(m);
	va_end(ap);
	return -1;
}


static int find_member(struct text_chunk** value, struct rpc_struct* s, str* member_name)
{
	struct text_chunk* n, *v;

	n = s->names;
	v = s->values;
	while(n) {
		if (member_name->len == n->s.len &&
		    !strncasecmp(member_name->s, n->s.s, n->s.len)) {
			if (n->flags & CHUNK_SEEN) goto skip;
			else {
				*value = v;
				n->flags |= CHUNK_SEEN;
				return 0;
			}
		}

	skip:
		n = n->next;
		v = v->next;
	}
	return 1;
}


static int rpc_struct_scan(struct rpc_struct* s, char* fmt, ...)
{
	struct text_chunk* val;
	va_list ap;
	int* int_ptr;
	double* double_ptr;
	char** char_ptr;
	str* str_ptr;
	str member_name;
	int ret, read;

	read = 0;
	va_start(ap, fmt);
	while(*fmt) {
		member_name.s = va_arg(ap, char*);
		member_name.len = strlen(member_name.s);
		ret = find_member(&val, s, &member_name);
		if (ret > 0) {
			va_end(ap);
			return read;
		}
		
		switch(*fmt) {
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int*);
			if (!val->s.len) {
				rpc_fault(s->ctx, 400, "Invalid Parameter Value");
				goto error;
			}
			     /* String in text_chunk is always zero terminated */
			*int_ptr = strtol(val->s.s, 0, 0);
			break;

		case 'f': /* double */
			double_ptr = va_arg(ap, double*);
			if (!val->s.len) {
				rpc_fault(s->ctx, 400, "Invalid Parameter Value");
				goto error;
			}
			     /* String in text_chunk is always zero terminated */
			*double_ptr = strtod(val->s.s, 0);
			break;
			
		case 's': /* zero terminated string */
			char_ptr = va_arg(ap, char**);
			     /* String in text_chunk is always zero terminated */
			*char_ptr = val->s.s;
			break;

		case 'S': /* str structure */
			str_ptr = va_arg(ap, str*);
			str_ptr->len = strlen(str_ptr->s);
			*str_ptr = val->s;
			break;
		default:
			rpc_fault(s->ctx, 500, "Invalid character in formatting string '%c'", *fmt);
			ERR("Invalid parameter type in formatting string: %c\n", *fmt);
			goto error;
		}
		fmt++;
		read++;
	}
	va_end(ap);
	return read;
 error:
	va_end(ap);
	return -read;
}
