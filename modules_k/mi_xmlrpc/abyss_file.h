#ifndef FILE_H_INCLUDED
#define FILE_H_INCLUDED

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include <xmlrpc-c/abyss.h>

#ifndef NAME_MAX
#define NAME_MAX    1024
#endif

#ifdef WIN32
#ifndef __BORLANDC__
#define O_APPEND    _O_APPEND
#define O_CREAT     _O_CREAT 
#define O_EXCL      _O_EXCL
#define O_RDONLY    _O_RDONLY
#define O_RDWR      _O_RDWR 
#define O_TRUNC _O_TRUNC
#define O_WRONLY    _O_WRONLY
#define O_TEXT      _O_TEXT
#define O_BINARY    _O_BINARY
#endif

#define A_HIDDEN    _A_HIDDEN
#define A_NORMAL    _A_NORMAL
#define A_RDONLY    _A_RDONLY
#define A_SUBDIR    _A_SUBDIR
#else
#define A_SUBDIR    1
#define O_BINARY    0
#define O_TEXT      0
#endif  /* WIN32 */

#ifdef WIN32

#ifndef __BORLANDC__
typedef struct _stati64 TFileStat;
typedef struct _finddata_t TFileInfo;
typedef long TFileFind;

#else  /* WIN32 */

typedef struct stat TFileStat;
typedef struct finddata_t {
    char name[NAME_MAX+1];
    int attrib;
    uint64_t size;
    time_t time_write;
    WIN32_FIND_DATA data;
} TFileInfo;

typedef HANDLE TFileFind;

#endif /* WIN32 */

#else

#include <unistd.h>
#include <dirent.h>

typedef struct stat TFileStat;

typedef struct finddata_t {
    char name[NAME_MAX+1];
    int attrib;
    uint64_t size;
    time_t time_write;
} TFileInfo;

typedef struct {
    char path[NAME_MAX+1];
    DIR *handle;
} TFileFind;

#endif

typedef int TFile;

abyss_bool
FileOpen(TFile *      const f,
         const char * const name,
         uint32_t     const attrib);

abyss_bool
FileOpenCreate(TFile *      const f,
               const char * const name,
               uint32_t     const attrib);

abyss_bool
FileClose(TFile * const f);

abyss_bool
FileWrite(TFile *      const f,
          const void * const buffer,
          uint32_t     const len);

int32_t
FileRead(TFile *  const f,
         void *   const buffer,
         uint32_t const len);

abyss_bool
FileSeek(TFile *  const f,
         uint64_t const pos,
         uint32_t const attrib);

uint64_t
FileSize(TFile * const f);

abyss_bool
FileStat(const char * const filename,
         TFileStat *  const filestat);

abyss_bool
FileFindFirst(TFileFind *  const filefind,
              const char * const path,
              TFileInfo *  const fileinfo);

abyss_bool
FileFindNext(TFileFind * const filefind,
             TFileInfo * const fileinfo);

void
FileFindClose(TFileFind * const filefind);

#endif
