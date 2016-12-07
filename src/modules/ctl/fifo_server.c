/*
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2005 iptelorg GmbH
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

/*! \file
 * \brief ctl module
 * \ingroup ctl
 *
 */

/*! \defgroup ctl Control binrpc socket
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
 */

#ifdef USE_FIFO

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
#include "../../tsend.h"
#include "fifo_server.h"
#include "io_listener.h"
#include "ctl.h"


#define MAX_FIFO_COMMAND        128    /* Maximum length of a FIFO server command */
#define MAX_CONSUME_BUFFER     1024    /* Buffer dimensions for FIFO server */
#define MAX_LINE_BUFFER        2048    /* Maximum parameter line length */
#define DEFAULT_REPLY_RETRIES     4    /* Default number of reply write attempts */
#define DEFAULT_REPLY_WAIT    80000    /* How long we should wait for the client, in micro seconds */
#define DEFAULT_FIFO_DIR    "/tmp/"    /* Where reply pipes may be opened */
#define MAX_MSG_CHUNKS        1024     /* maximum message pieces */
#define FIFO_TX_TIMEOUT        200     /* maximum time to block writing to
                                          the fifo */

/* readline from a buffer helper */
struct readline_handle{ 
	char* s;    /* buffer start */
	char* end;  /* end */
	char* crt;  /* crt. pos */
};

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
	void *ctx; /* context, which must be passed along */
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
	struct readline_handle read_h;
	struct send_handle* send_h;
	int line_no;
} rpc_ctx_t;




char* fifo_dir           = DEFAULT_FIFO_DIR;       /* dir where reply fifos are
													  allowed */
int   fifo_reply_retries = DEFAULT_REPLY_RETRIES;
int   fifo_reply_wait    = DEFAULT_REPLY_WAIT;


static rpc_t     func_param;        /* Pointers to implementation of RPC funtions */

static int  rpc_send         (rpc_ctx_t* ctx);                                 /* Send the reply to the client */
static void rpc_fault        (rpc_ctx_t* ctx,       int code, char* fmt, ...); /* Signal a failure to the client */
static int  rpc_add          (rpc_ctx_t* ctx,       char* fmt, ...);           /* Add a new piece of data to the result */
static int  rpc_scan         (rpc_ctx_t* ctx,       char* fmt, ...);           /* Retrieve request parameters */
static int  rpc_rpl_printf   (rpc_ctx_t* ctx,       char* fmt, ...);           /* Add printf-like formated data to the result set */
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

        l = ctl_malloc(sizeof(struct text_chunk));
	if (!l) {
		ERR("No Memory Left\n");
		return 0;
	}
	l->s.s = ctl_malloc(src->len * 2 + 1);
	if (!l->s.s) {
		ERR("No Memory Left\n");
		ctl_free(l);
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

        l = ctl_malloc(sizeof(struct text_chunk));
	if (!l) {
		ERR("No Memory Left\n");
		return 0;
	}
	l->s.s = ctl_malloc(src->len + 1);
	if (!l->s.s) {
		ERR("No Memory Left\n");
		ctl_free(l);
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

        l = ctl_malloc(sizeof(struct text_chunk));
	if (!l) {
		ERR("No Memory Left\n");
		return 0;
	}
	l->s.s = ctl_malloc(src->len + 1);
	if (!l->s.s) {
		ERR("No Memory Left\n");
		ctl_free(l);
		return 0;
	}
	l->next = 0;
	l->flags = 0;
	if (unescape(&l->s, src->s, src->len) < 0) {
		ctl_free(l->s.s);
		ctl_free(l);
		return 0;
	}
	l->s.s[l->s.len] = '\0';
	return l;
}


static void free_chunk(struct text_chunk* c)
{
	if (c && c->s.s) ctl_free(c->s.s);
	if (c) ctl_free(c);
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

	ctl_free(s);
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
		rpc_fault(ctx, 400, "Line %d Empty - Structure Expected", 
					ctx->line_no);
		return 0;
	}

	s = (struct rpc_struct*)ctl_malloc(sizeof(struct rpc_struct));
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
			rpc_fault(ctx, 400, "Colon missing in struct on line %d",
							ctx->line_no);
			goto err;;
		}
		name.s = left.s;
		name.len = colon - name.s;
		value.s = colon + 1;
		value.len = left.len - (colon - left.s) - 1;
		
		     /* Create name chunk */
		n = new_chunk_unescape(&name);
		if (!n) {
			rpc_fault(ctx, 400, "Error while processing struct member '%.*s' "
						"on line %d", name.len, ZSW(name.s), ctx->line_no);
			goto err;
		}
		n->next = s->names;
		s->names = n;

		     /* Create value chunk */
		v = new_chunk_unescape(&value);
		if (!v) {
			rpc_fault(ctx, 400, "Error while processing struct membeer '%.*s'"
						" on line %d", name.len, ZSW(name.s), ctx->line_no);
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
 * Read a line from FIFO file and store a pointer to the data in buffer 'b'
 * and the legnth of the line in variable 'read'
 *
 * Returns -1 on error, 0 on success
 */
static int read_line(char** b, int* read, struct readline_handle* rh)
{
	char* eol;
	char* trim;
	
	if (rh->crt>=rh->end){
		/* end, nothing more to read */
		return -1;
	}
	for(eol=rh->crt; (eol<rh->end) && (*eol!='\n'); eol++);
	*eol=0;
	trim=eol;
	/* trim spaces at the end */
	for(trim=eol;(trim>rh->crt) && 
			((*trim=='\r')||(*trim==' ')||(*trim=='\t')); trim--){
		*trim=0;
	}
	*b=rh->crt;
	*read = (int)(trim-rh->crt);
	rh->crt=eol+1;
	return 0;
}


/*
 * Remove directory path from filename and replace it
 * with the path configured through a module parameter.
 * 
 * The result is allocated using ctl_malloc and thus
 * has to be freed using ctl_free
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
	new_fn = ctl_malloc(prefix_len + fn_len + 1);
	if (new_fn == 0) {
		ERR("No memory left\n");
		return 0;
	}

	memcpy(new_fn, fifo_dir, prefix_len);
	memcpy(new_fn + prefix_len, file, fn_len);
	new_fn[prefix_len + fn_len] = 0;
	return new_fn;
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
 * returns fs no on success, -1 on error
 */
static int open_reply_pipe(char *pipe_name)
{
	
	int fifofd;
	int flags;
	
	int retries = fifo_reply_retries;
	
	fifofd=-1;
	if (!pipe_name || *pipe_name == 0) {
		DBG("No file to write to about missing cmd\n");
		goto error;
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
				goto error;
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
		goto error;
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
	
	return fifofd;
 error:
	if (fifofd!=-1) close(fifofd);
	return -1;
}




/*
 * Man FIFO routine running in the FIFO
 * processes requests received
 * through the FIFO file repeatedly
 */
int fifo_process(char* msg_buf, int size, int* bytes_needed, void *sh,
					void** saved_state)
{
	rpc_export_t* exp;
	char* buf;
	int line_len;
	char *file_sep;
	struct text_chunk* p;
	struct rpc_struct* s;
	int r;
	int req_size;
	static rpc_ctx_t context; 

	DBG("process_fifo: called with %d bytes, offset %d: %.*s\n",
			size, (int)(long)*saved_state, size, msg_buf);
	/* search for the end of the request (\n\r) */
	if (size < 6){ /* min fifo request */
		*bytes_needed=6-size;
		return 0; /* we want more bytes, nothing processed */
	}
	for (r=1+(int)(long)*saved_state;r<size;r++){
		if ((msg_buf[r]=='\n' || msg_buf[r]=='\r') &&
			(msg_buf[r-1]=='\n'|| msg_buf[r-1]=='\r')){
			/* found double cr, or double lf => end of request */
			req_size=r;
			goto process;
		}
	}
	/* no end of request found => ask for more bytes */
	*bytes_needed=1;
	/* save current offset, to optimize search */
	*saved_state=(void*)(long)(r-1);
	return 0; /* we want again the whole buffer */
process:
	
	DBG("process_fifo  %d bytes request: %.*s\n", 
			req_size, req_size, msg_buf);
	file_sep = 0;
	context.method = 0;
	context.reply_file = 0;
	context.body = 0;
	context.code = 200;
	context.reason = "OK";
	context.reply_sent = 0;
	context.last = 0;
	context.line_no = 0;
	context.read_h.s=msg_buf;
	context.read_h.end=msg_buf+size;
	context.read_h.crt=msg_buf;
	context.send_h=(struct send_handle*)sh;
		     /* commands must look this way ':<command>:[filename]' */
		if (read_line(&buf, &line_len, &context.read_h) < 0) {
			     /* line breaking must have failed -- consume the rest
			      * and proceed to a new request
			      */
			ERR("Command expected\n");
			goto consume;
		}
		context.line_no++;
		if (line_len == 0) {
			DBG("Empty command received\n");
			goto consume;
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
			ctl_free(context.reply_file); 
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

		*bytes_needed=0;
		DBG("Command consumed\n");
		DBG("process_fifo: returning %d, bytes_needed 0\n", req_size+1);
		return req_size+1; /* all was processed (including terminating \n)*/

}


/*
 * Initialze the FIFO fd
 * Make sure that we can create and open the FIFO file and
 * make it secure. This function must be executed from mod_init.
 * This ensures that it has sufficient privileges.
 */
int init_fifo_fd(char* fifo, int fifo_mode, int fifo_uid, int fifo_gid,
					int* fifo_write)
{
	struct stat filestat;
	int n;
	long opt;
	int fifo_read;
	
	if (fifo == NULL) {
		ERR("null fifo: no fifo will be opened\n");
		/* error null fifo */
		return -1;
	}
	if (strlen(fifo) == 0) {
		ERR("emtpy fifo: fifo disabled\n");
		return -1;
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
	/* make sure the read fifo will not close */
	*fifo_write = open(fifo, O_WRONLY | O_NONBLOCK, 0);
	if (*fifo_write < 0) {
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
	return fifo_read;
}



int fifo_rpc_init()
{
	memset(&func_param, 0, sizeof(func_param));
	func_param.send = (rpc_send_f)rpc_send;
	func_param.fault = (rpc_fault_f)rpc_fault;
	func_param.add = (rpc_add_f)rpc_add;
	func_param.scan = (rpc_scan_f)rpc_scan;
	func_param.rpl_printf = (rpc_rpl_printf_f)rpc_rpl_printf;
	func_param.struct_add = (rpc_struct_add_f)rpc_struct_add;
	/* use rpc_struct_add for array_add */
	func_param.array_add = (rpc_array_add_f)rpc_struct_add;
	func_param.struct_scan = (rpc_struct_scan_f)rpc_struct_scan;	
	func_param.struct_printf = (rpc_struct_printf_f)rpc_struct_printf;
	return 0;
}



/*
 * Close and unlink the FIFO file
 */
void destroy_fifo(int read_fd, int w_fd, char* fname)
{
	if (read_fd!=-1)
		close(read_fd);
	if(w_fd!=-1)
		close(w_fd);
	/* if  FIFO was created, delete it */
	if (fname && strlen(fname)) {
		if (unlink(fname) < 0) {
			WARN("Cannot delete fifo (%s):"
			     " %s\n", fname, strerror(errno));
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
		ERR("fifo write error: %s\n", strerror(errno));
		if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			goto retry;
		}
		va_end(ap);
		return -1;
	}
	va_end(ap);
	return 0;
}



inline static int build_iovec(rpc_ctx_t* ctx, struct iovec* v, int v_size) 
{
	struct text_chunk* p;
	int r_c_len;
	int r;
	
	
	
	/* reason code */
	v[0].iov_base=int2str(ctx->code, &r_c_len);
	v[0].iov_len=r_c_len;
	v[1].iov_base=" ";
	v[1].iov_len=1;
	/* reason txt */
	v[2].iov_base=ctx->reason;
	v[2].iov_len=strlen(ctx->reason);
	v[3].iov_base="\n";
	v[3].iov_len=1;
	r=4;
	/* Send the body */
	while(ctx->body) {
		p = ctx->body;
		ctx->body = ctx->body->next;
		if (p->s.len){
			if (r>=v_size) goto error_overflow;
			v[r].iov_base=p->s.s;
			v[r].iov_len=p->s.len;
			r++;
		}
		if (p->flags & CHUNK_POSITIONAL) {
			if (r>=v_size) goto error_overflow;
			v[r].iov_base="\n";
			v[r].iov_len=1;
			r++;
		} else if (p->flags & CHUNK_MEMBER_NAME) {
			if (r>=v_size) goto error_overflow;
			v[r].iov_base=":";
			v[r].iov_len=1;
			r++;
		} else if (p->flags & CHUNK_MEMBER_VALUE) {
			if (p->next && p->next->flags & CHUNK_MEMBER_NAME) {
				if (r>=MAX_MSG_CHUNKS) goto error_overflow;
				v[r].iov_base=",";
				v[r].iov_len=1;
				r++;
			} else {
				if (r>=v_size) goto error_overflow;
				v[r].iov_base="\n";
				v[r].iov_len=1;
				r++;
			}
		}
		free_chunk(p);
	}
	return r;
error_overflow:
	ERR("too many message chunks, iovec buffer overflow: %d/%d\n", r,
			MAX_MSG_CHUNKS);
	return -1;
}



/*
 * Send a reply, either positive or negative, to the client
 */
static int rpc_send(rpc_ctx_t* ctx) 
{
	struct iovec v[MAX_MSG_CHUNKS];
	int f;
	int n;
	int ret;
	/* Send the reply only once */
	if (ctx->reply_sent) return 1;
	else ctx->reply_sent = 1;
	
	if ((n=build_iovec(ctx, v, MAX_MSG_CHUNKS))<0)
		goto error;
	if (ctx->send_h->type==S_FIFO){
	/* Open the reply file */
		f = open_reply_pipe(ctx->reply_file);
		if (f == -1) {
			ERR("No reply pipe %s\n", ctx->reply_file);
			return -1;
		}
		ret=tsend_dgram_ev(f, v, n, FIFO_TX_TIMEOUT);
		close(f);
	}else{
		ret=sock_send_v(ctx->send_h, v, n);
	}
	return (ret>=0)?0:-1;
error:
	ERR("rpc_send fifo error\n");
	return -1;
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
			rpc_fault(ctx, 500, "Internal server error while processing"
					" line %d", ctx->line_no);
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
			rpc_fault(ctx, 500, "Internal Server Error, line %d",
						ctx->line_no);
			goto err;
		}
		break;
		
	case 'b':
		str_val.len = 1;
		str_val.s = ((va_arg(*ap, int) == 0) ? "0" : "1");
		l = new_chunk(&str_val);
		if (!l) {
			rpc_fault(ctx, 500, "Internal Server Error, line %d", 
						ctx->line_no);
			goto err;
		}
		break;
				
	case 's':
		str_val.s = va_arg(*ap, char*);
		str_val.len = strlen(str_val.s);
		l = new_chunk_escape(&str_val, 0);
		if (!l) {
			rpc_fault(ctx, 500, "Internal Server Error, line %d", 
						ctx->line_no);
			goto err;
		}
		break;
		
	case 'S':
		sp = va_arg(*ap, str*);
		l = new_chunk_escape(sp, 0);
		if (!l) {
			rpc_fault(ctx, 500, "Internal Server Error, line %d", 
							ctx->line_no);
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
		if (*fmt == '{' || *fmt == '[') {
			void_ptr = va_arg(ap, void**);
			l = new_chunk(&s);
			if (!l) {
				rpc_fault(ctx, 500, "Internal Server Error");
				goto err;
			}
			l->ctx=ctx;
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
	rpc_ctx_t* ctx;
	
	ctx=(rpc_ctx_t*)c->ctx;
	buf = (char*)ctl_malloc(RPC_BUF_SIZE);
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
			nm.s = name;
			nm.len = strlen(name);
			m = new_chunk_escape(&nm, 1); /* Escape all characters, including : and , */
			if (!m) {
				rpc_fault(ctx, 500, "Internal Server Error");
				goto err;
			}

			s.s = buf;
			s.len = n;
			l = new_chunk_escape(&s, 1);
			if (!l) {
				rpc_fault(ctx, 500, "Internal Server Error");
				free_chunk(m);
				ERR("Error while creating text_chunk structure");
				goto err;
			}
			
			l->flags |= CHUNK_MEMBER_VALUE;
			l->next = c->next;
			c->next = l;
			if (c == ctx->last) ctx->last = l;

			m->flags |= CHUNK_MEMBER_NAME;
			m->next = c->next;
			c->next = m;
			if (c == ctx->last) ctx->last = m;
			return 0;
		}
		     /* Else try again with more space. */
		if (n > -1) {   /* glibc 2.1 */
			buf_size = n + 1; /* precisely what is needed */
		} else {          /* glibc 2.0 */
			buf_size *= 2;  /* twice the old size */
		}
		if ((buf = ctl_realloc(buf, buf_size)) == 0) {
			rpc_fault(ctx, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			goto err;
		}
	}
	return 0;
 err:
	if (buf) ctl_free(buf);
	return -1;
}


static int rpc_rpl_printf(rpc_ctx_t* ctx, char* fmt, ...)
{
	int n, buf_size;
	char* buf;
	va_list ap;
	str s;
	struct text_chunk* l;

	buf = (char*)ctl_malloc(RPC_BUF_SIZE);
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
			ctl_free(buf);
			return 0;
		}
		     /* Else try again with more space. */
		if (n > -1) {   /* glibc 2.1 */
			buf_size = n + 1; /* precisely what is needed */
		} else {          /* glibc 2.0 */
			buf_size *= 2;  /* twice the old size */
		}
		if ((buf = ctl_realloc(buf, buf_size)) == 0) {
			rpc_fault(ctx, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			goto err;
		}
	}
	return 0;
 err:
	if (buf) ctl_free(buf);
	return -1;
}


static int rpc_scan(rpc_ctx_t* ctx, char* fmt, ...)
{
	struct text_chunk* l;
	struct rpc_struct* s;
	int* int_ptr;
	char** char_ptr;
	str* str_ptr;
	double* double_ptr;
	void** void_ptr;
	int read;
	str line;
	int nofault;
	int modifiers;

	va_list ap;
	va_start(ap, fmt);

	nofault = 0;
	read = 0;
	modifiers=0;
	while(*fmt) {
		if (read_line(&line.s, &line.len, &ctx->read_h) < 0) {
			va_end(ap);
			return read;
		}
		ctx->line_no++;

		switch(*fmt) {
		case '*': /* start of optional parameters */
			nofault = 1;
			modifiers++;
			break;
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			if (!line.len) {
				if(nofault==0)
					rpc_fault(ctx, 400, "Invalid parameter value on line %d", 
									ctx->line_no);
				goto error;
			}
			int_ptr = va_arg(ap, int*);
			*int_ptr = strtol(line.s, 0, 0);
			break;

		case 'f': /* double */
			if (!line.len) {
				if(nofault==0)
					rpc_fault(ctx, 400, "Invalid parameter value on line %d", 
								ctx->line_no);
				goto error;
			}
			double_ptr = va_arg(ap, double*);
			*double_ptr = strtod(line.s, 0);
			break;
			
		case 's': /* zero terminated string */
		case 'S': /* str structure */
			l = new_chunk_unescape(&line);
			if (!l) {
				if(nofault==0) {
					rpc_fault(ctx, 500, "Internal Server Error");
					ERR("Not enough memory\n");
				}
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
	return read-modifiers;

 error:
	va_end(ap);
	return -(read-modifiers);
}


static int rpc_struct_add(struct text_chunk* s, char* fmt, ...)
{
	static char buf[MAX_LINE_BUFFER];
	str st, *sp;
	void** void_ptr;
	va_list ap;
	struct text_chunk* m, *c;
	rpc_ctx_t* ctx;

	ctx=(rpc_ctx_t*)s->ctx;
	va_start(ap, fmt);
	while(*fmt) {
		     /* Member name escaped */
		st.s = va_arg(ap, char*);
		st.len = strlen(st.s);
		m = new_chunk_escape(&st, 1); /* Escape all characters, including : and , */
		if (!m) {
			rpc_fault(ctx, 500, "Internal Server Error");
			goto err;
		}
		m->flags |= CHUNK_MEMBER_NAME;
		
		if(*fmt=='{' || *fmt=='[') {
			void_ptr = va_arg(ap, void**);
			m->ctx=ctx;
			append_chunk(ctx, m);
			*void_ptr = m;
		} else {
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
					rpc_fault(ctx, 400, "Error While Converting double");
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
				rpc_fault(ctx, 500, "Bug In SER (Invalid formatting character %c)",
						*fmt);
				ERR("Invalid formatting character\n");
				goto err;
			}

			if (!c) {
				rpc_fault(ctx, 500, "Internal Server Error");
				goto err;
			}
			c->flags |= CHUNK_MEMBER_VALUE;
			c->next = s->next;
			s->next = c;
			if (s == ctx->last) ctx->last = c;

			m->next = s->next;
			s->next = m;
			if (s == ctx->last) ctx->last = m;
		}
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

#endif
