/*******************************************************************************
**
** abyss.h
**
** This file is part of the ABYSS Web server project.
**
** Copyright (C) 2000 by Moez Mahfoudh <mmoez@bigfoot.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
*******************************************************************************/

#ifndef _ABYSS_H_
#define _ABYSS_H_


#ifdef __cplusplus
extern "C" {
#endif

/*
#ifndef HAVE_WIN32_CONFIG_H
#include "xmlrpc_config.h"
#else
#include "xmlrpc_win32_config.h"
#endif
*/


/*********************************************************************
** EDIT THE FOLLOWING LINES TO MEET YOUR CONFIGURATION NEEDS
*********************************************************************/

/*********************************************************************
** Paths and so on...
*********************************************************************/

#ifdef ABYSS_WIN32
#define DEFAULT_ROOT		"c:\\abyss"
#define DEFAULT_DOCS		DEFAULT_ROOT"\\htdocs"
#define DEFAULT_CONF_FILE	DEFAULT_ROOT"\\conf\\abyss.conf"
#define DEFAULT_LOG_FILE	DEFAULT_ROOT"\\log\\abyss.log"
#else
#ifdef __rtems__
#define DEFAULT_ROOT		"/abyss"
#else
#define DEFAULT_ROOT		"/usr/local/abyss"
#endif
#define DEFAULT_DOCS		DEFAULT_ROOT"/htdocs"
#define DEFAULT_CONF_FILE	DEFAULT_ROOT"/conf/abyss.conf"
#define DEFAULT_LOG_FILE	DEFAULT_ROOT"/log/abyss.log"
#endif

/*********************************************************************
** Maximum numer of simultaneous connections
*********************************************************************/

#define MAX_CONN	16

/*********************************************************************
** DON'T CHANGE THE FOLLOWING LINES
*********************************************************************/

/*********************************************************************
** Server Info Definitions
*********************************************************************/

#define SERVER_VERSION		"0.3"
#define SERVER_HVERSION		"ABYSS/0.3"
#define SERVER_HTML_INFO	"<p><HR><b><i><a href=\"http:\057\057abyss.linuxave.net\">ABYSS Web Server</a></i></b> version "SERVER_VERSION"<br>&copy; <a href=\"mailto:mmoez@bigfoot.com\">Moez Mahfoudh</a> - 2000</p>"
#define SERVER_PLAIN_INFO	CRLF"--------------------------------------------------------------------------------"\
							CRLF"ABYSS Web Server version "SERVER_VERSION CRLF"(C) Moez Mahfoudh - 2000"

/*********************************************************************
** General purpose definitions
*********************************************************************/

#ifdef ABYSS_WIN32
#define strcasecmp(a,b)	stricmp((a),(b))
#else
#define ioctlsocket(a,b,c)	ioctl((a),(b),(c))
#endif	/* ABYSS_WIN32 */

#ifndef NULL
#define NULL ((void *)0)
#endif	/* NULL */

#ifndef TRUE
#define TRUE	1
#endif	/* TRUE */

#ifndef FALSE
#define FALSE    0
#endif  /* FALSE */

#ifdef ABYSS_WIN32
#define LBR	"\n"
#else
#define LBR "\n"
#endif	/* ABYSS_WIN32 */

/*********************************************************************
** Typedefs
*********************************************************************/

typedef unsigned long uint64;
typedef long int64;

typedef unsigned int uint32;
typedef int int32;

typedef unsigned short uint16;
typedef short int16;

typedef unsigned char byte;
typedef unsigned char uint8;
typedef char int8;

#ifndef __cplusplus
typedef int bool;
#endif

/*********************************************************************
** Buffer
*********************************************************************/

typedef struct
{
	void *data;
	uint32 size;
	uint32 staticid;
} TBuffer;

bool BufferAlloc(TBuffer *buf,uint32 memsize);
bool BufferRealloc(TBuffer *buf,uint32 memsize);
void BufferFree(TBuffer *buf);


/*********************************************************************
** String
*********************************************************************/

typedef struct
{
	TBuffer buffer;
	uint32 size;
} TString;

bool StringAlloc(TString *s);
bool StringConcat(TString *s,char *s2);
bool StringBlockConcat(TString *s,char *s2,char **ref);
void StringFree(TString *s);
char *StringData(TString *s);


/*********************************************************************
** List
*********************************************************************/

typedef struct
{
	void **item;
	uint16 size,maxsize;
	bool autofree;
} TList;

void ListInit(TList *sl);
void ListInitAutoFree(TList *sl);
void ListFree(TList *sl);
bool ListAdd(TList *sl,void *str);
bool ListAddFromString(TList *list,char *c);
bool ListFindString(TList *sl,char *str,uint16 *index);


/*********************************************************************
** Table
*********************************************************************/

typedef struct 
{
	char *name,*value;
	uint16 hash;
} TTableItem;

typedef struct
{
	TTableItem *item;
	uint16 size,maxsize;
} TTable;

void TableInit(TTable *t);
void TableFree(TTable *t);
bool TableAdd(TTable *t,char *name,char *value);
bool TableAddReplace(TTable *t,char *name,char *value);
bool TableFindIndex(TTable *t,char *name,uint16 *index);
char *TableFind(TTable *t,char *name);


/*********************************************************************
** Thread
*********************************************************************/

#ifdef ABYSS_WIN32
#include <windows.h>
#define  THREAD_ENTRYTYPE  WINAPI
#else
#define  THREAD_ENTRYTYPE
#include <pthread.h>
#endif	/* ABYSS_WIN32 */

typedef uint32 (THREAD_ENTRYTYPE *TThreadProc)(void *);
#ifdef ABYSS_WIN32
typedef HANDLE TThread;
#else
typedef pthread_t TThread;
typedef void* (*PTHREAD_START_ROUTINE)(void *);
#endif	/* ABYSS_WIN32 */

bool ThreadCreate(TThread *t,TThreadProc func,void *arg);
bool ThreadRun(TThread *t);
bool ThreadStop(TThread *t);
bool ThreadKill(TThread *t);
void ThreadWait(uint32 ms);
void ThreadExit( TThread *t, int ret_value );
void ThreadClose( TThread *t );

/*********************************************************************
** Mutex
*********************************************************************/

#ifdef ABYSS_WIN32
typedef HANDLE TMutex;
#else
typedef pthread_mutex_t TMutex;
#endif	/* ABYSS_WIN32 */

bool MutexCreate(TMutex *m);
bool MutexLock(TMutex *m);
bool MutexUnlock(TMutex *m);
bool MutexTryLock(TMutex *m);
void MutexFree(TMutex *m);


/*********************************************************************
** Pool
*********************************************************************/

typedef struct _TPoolZone
{
	char *pos,*maxpos;
	struct _TPoolZone *next,*prev;
/*	char data[0]; */
	char data[1];
} TPoolZone;

typedef struct
{
	TPoolZone *firstzone,*currentzone;
	uint32 zonesize;
	TMutex mutex;
} TPool;

bool PoolCreate(TPool *p,uint32 zonesize);
void PoolFree(TPool *p);

void *PoolAlloc(TPool *p,uint32 size);
char *PoolStrdup(TPool *p,char *s);


/*********************************************************************
** Socket
*********************************************************************/

#ifdef ABYSS_WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#endif	/* ABYSS_WIN32 */

#define TIME_INFINITE	0xffffffff

#ifdef ABYSS_WIN32
typedef SOCKET TSocket;
#else
typedef uint32 TSocket;
#endif	/* ABYSS_WIN32 */

typedef struct in_addr TIPAddr;

#define IPB1(x)	(((unsigned char *)(&x))[0])
#define IPB2(x)	(((unsigned char *)(&x))[1])
#define IPB3(x)	(((unsigned char *)(&x))[2])
#define IPB4(x)	(((unsigned char *)(&x))[3])

bool SocketInit();

bool SocketCreate(TSocket *s);
bool SocketClose(TSocket *s);

uint32 SocketWrite(TSocket *s, char *buffer, uint32 len);
uint32 SocketRead(TSocket *s, char *buffer, uint32 len);
uint32 SocketPeek(TSocket *s, char *buffer, uint32 len);

bool SocketConnect(TSocket *s, TIPAddr *addr, uint16 port);
bool SocketBind(TSocket *s, TIPAddr *addr, uint16 port);

bool SocketListen(TSocket *s, uint32 backlog);
bool SocketAccept(TSocket *s, TSocket *ns,TIPAddr *ip);

uint32 SocketError();

uint32 SocketWait(TSocket *s,bool rd,bool wr,uint32 timems);

bool SocketBlocking(TSocket *s, bool b);
uint32 SocketAvailableReadBytes(TSocket *s);


/*********************************************************************
** File
*********************************************************************/

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#ifndef NAME_MAX
#define NAME_MAX	1024
#endif

#ifdef ABYSS_WIN32
#ifndef __BORLANDC__
#define O_APPEND	_O_APPEND
#define O_CREAT 	_O_CREAT 
#define O_EXCL		_O_EXCL
#define O_RDONLY	_O_RDONLY
#define O_RDWR		_O_RDWR 
#define O_TRUNC	_O_TRUNC
#define O_WRONLY	_O_WRONLY
#define O_TEXT		_O_TEXT
#define O_BINARY	_O_BINARY
#endif

#define A_HIDDEN	_A_HIDDEN
#define A_NORMAL	_A_NORMAL
#define A_RDONLY	_A_RDONLY
#define A_SUBDIR	_A_SUBDIR
#else
#define	A_SUBDIR	1
#define O_BINARY	0
#define O_TEXT		0
#endif	/* ABYSS_WIN32 */

#ifdef ABYSS_WIN32

#ifndef __BORLANDC__
typedef struct _stati64 TFileStat;
typedef struct _finddata_t TFileInfo;
typedef long TFileFind;

#else

typedef struct stat TFileStat;
typedef struct finddata_t
{
	char name[NAME_MAX+1];
	int attrib;
	uint64 size;
	time_t time_write;
   WIN32_FIND_DATA data;
} TFileInfo;
typedef HANDLE TFileFind;
#endif

#else

#include <unistd.h>
#include <dirent.h>

typedef struct stat TFileStat;

typedef struct finddata_t
{
	char name[NAME_MAX+1];
	int attrib;
	uint64 size;
	time_t time_write;
} TFileInfo;

typedef struct 
{
	char path[NAME_MAX+1];
	DIR *handle;
} TFileFind;

#endif

typedef int TFile;

bool FileOpen(TFile *f, char *name, uint32 attrib);
bool FileOpenCreate(TFile *f, char *name, uint32 attrib);
bool FileClose(TFile *f);

bool FileWrite(TFile *f, void *buffer, uint32 len);
int32 FileRead(TFile *f, void *buffer, uint32 len);

bool FileSeek(TFile *f, uint64 pos, uint32 attrib);
uint64 FileSize(TFile *f);

bool FileStat(char *filename,TFileStat *filestat);

bool FileFindFirst(TFileFind *filefind,char *path,TFileInfo *fileinfo);
bool FileFindNext(TFileFind *filefind,TFileInfo *fileinfo);
void FileFindClose(TFileFind *filefind);

/*********************************************************************
** Server (1/2)
*********************************************************************/

typedef struct
{
	TSocket listensock;
	TFile logfile;
	TMutex logmutex;
	char *name;
	char *filespath;
	uint16 port;
	uint32 keepalivetimeout,keepalivemaxconn,timeout;
	TList handlers;
	TList defaultfilenames;
	void *defaulthandler;
	bool advertise;
#ifdef _UNIX
	uid_t uid;
	gid_t gid;
	TFile pidfile;
#endif	
} TServer;


/*********************************************************************
** Conn
*********************************************************************/

#define BUFFER_SIZE	4096 

typedef struct _TConn
{
	TServer *server;
	uint32 buffersize,bufferpos;
	uint32 inbytes,outbytes;	
	TSocket socket;
	TIPAddr peerip;
	TThread thread;
	bool connected;
	bool inUse;
	void (*job)(struct _TConn *);
	char buffer[BUFFER_SIZE];
} TConn;

TConn *ConnAlloc();
void ConnFree(TConn *c);

bool ConnCreate(TConn *c, TSocket *s, void (*func)(TConn *));
bool ConnProcess(TConn *c);
bool ConnKill(TConn *c);

bool ConnWrite(TConn *c,void *buffer,uint32 size);
bool ConnRead(TConn *c, uint32 timems);
void ConnReadInit(TConn *c);
bool ConnReadLine(TConn *c,char **z,uint32 timems);

bool ConnWriteFromFile(TConn *c,TFile *file,uint64 start,uint64 end,
			void *buffer,uint32 buffersize,uint32 rate);


/*********************************************************************
** Range
*********************************************************************/

bool RangeDecode(char *str,uint64 filesize,uint64 *start,uint64 *end);

/*********************************************************************
** Date
*********************************************************************/

#include <time.h>

typedef struct tm TDate;

bool DateToString(TDate *tm,char *s);
bool DateToLogString(TDate *tm,char *s);

bool DateDecode(char *s,TDate *tm);

int32 DateCompare(TDate *d1,TDate *d2);

bool DateFromGMT(TDate *d,time_t t);
bool DateFromLocal(TDate *d,time_t t);

bool DateInit();

/*********************************************************************
** Base64
*********************************************************************/

void Base64Encode(char *s,char *d);

/*********************************************************************
** Session
*********************************************************************/

typedef enum
{
	m_unknown,m_get,m_put,m_head,m_post,m_delete,m_trace,m_options
} TMethod;

typedef struct
{
	TMethod method;
	uint32 nbfileds;
	char *uri;
	char *query;
	char *host;
	char *from;
	char *useragent;
	char *referer;
	char *requestline;
	char *user;
	uint16 port;
	TList cookies;
	TList ranges;

	uint16 status;
	TString header;

	bool keepalive,cankeepalive;
	bool done;

	TServer *server;
	TConn *conn;

	uint8 versionminor,versionmajor;

	TTable request_headers,response_headers;

	TDate date;

	bool chunkedwrite,chunkedwritemode;
} TSession;

/*********************************************************************
** Request
*********************************************************************/

#define CR		'\r'
#define LF		'\n'
#define CRLF	"\r\n"

bool RequestValidURI(TSession *r);
bool RequestValidURIPath(TSession *r);
bool RequestUnescapeURI(TSession *r);

char *RequestHeaderValue(TSession *r,char *name);

bool RequestRead(TSession *r);
void RequestInit(TSession *r,TConn *c);
void RequestFree(TSession *r);

bool RequestAuth(TSession *r,char *credential,char *user,char *pass);

/*********************************************************************
** Response
*********************************************************************/

bool ResponseAddField(TSession *r,char *name,char *value);
void ResponseWrite(TSession *r);

bool ResponseChunked(TSession *s);

void ResponseStatus(TSession *r,uint16 code);
void ResponseStatusErrno(TSession *r);

bool ResponseContentType(TSession *r,char *type);
bool ResponseContentLength(TSession *r,uint64 len);

void ResponseError(TSession *r);


/*********************************************************************
** HTTP
*********************************************************************/

char *HTTPReasonByStatus(uint16 status);

int32 HTTPRead(TSession *s,char *buffer,uint32 len);

bool HTTPWrite(TSession *s,char *buffer,uint32 len);
bool HTTPWriteEnd(TSession *s);

/*********************************************************************
** Server (2/2)
*********************************************************************/

typedef bool (*URIHandler) (TSession *);

bool ServerCreate(TServer *srv,char *name,uint16 port,char *filespath,
				  char *logfilename);
void ServerFree(TServer *srv);

void ServerInit(TServer *srv);
void ServerRun(TServer *srv);
void ServerRunOnce(TServer *srv);

bool ServerAddHandler(TServer *srv,URIHandler handler);
void ServerDefaultHandler(TServer *srv,URIHandler handler);

bool LogOpen(TServer *srv, char *filename);
void LogWrite(TServer *srv,char *c);
void LogClose(TServer *srv);


/*********************************************************************
** MIMEType
*********************************************************************/

void MIMETypeInit();
bool MIMETypeAdd(char *type,char *ext);
char *MIMETypeFromExt(char *ext);
char *MIMETypeFromFileName(char *filename);
char *MIMETypeGuessFromFile(char *filename);


/*********************************************************************
** Conf
*********************************************************************/

bool ConfReadMIMETypes(char *filename);
bool ConfReadServerFile(char *filename,TServer *srv);


/*********************************************************************
** Trace
*********************************************************************/

void TraceMsg(char *fmt,...);
void TraceExit(char *fmt,...);


/*********************************************************************
** Session
*********************************************************************/

bool SessionLog(TSession *s);


#ifdef __cplusplus
}
#endif

#endif	/* _ABYSS_H_ */
