#ifndef DATA_H_INCLUDED
#define DATA_H_INCLUDED

#include "abyss_thread.h"

#ifndef XMLRPC_OLD_VERSION
#define MAX_CONN        16
typedef struct
{
	void *data;
	int size;
	int staticid;
} TBuffer;

typedef struct
{
	TBuffer buffer;
	int size;
} TString;

abyss_bool StringAlloc(TString *s);
void StringFree(TString *s);
#endif

/*********************************************************************
** List
*********************************************************************/

typedef struct {
    void **item;
    uint16_t size;
    uint16_t maxsize;
    abyss_bool autofree;
} TList;

void
ListInit(TList * const listP);

void
ListInitAutoFree(TList * const listP);

void
ListFree(TList * const listP);

void
ListFreeItems(TList * const listP);

abyss_bool
ListAdd(TList * const listP,
        void *  const str);

void
ListRemove(TList * const listP);

abyss_bool
ListAddFromString(TList *      const listP,
                  const char * const c);

abyss_bool
ListFindString(TList *      const listP,
               const char * const str,
               uint16_t *   const indexP);


typedef struct 
{
    char *name,*value;
    uint16_t hash;
} TTableItem;

typedef struct
{
    TTableItem *item;
    uint16_t size,maxsize;
} TTable;

void
TableInit(TTable * const t);

void
TableFree(TTable * const t);

abyss_bool
TableAdd(TTable *     const t,
         const char * const name,
         const char * const value);

abyss_bool
TableAddReplace(TTable *     const t,
                const char * const name,
                const char * const value);

abyss_bool
TableFindIndex(TTable *     const t,
               const char * const name,
               uint16_t *   const index);

char *
TableFind(TTable *     const t,
          const char * const name);


/*********************************************************************
** Pool
*********************************************************************/

typedef struct _TPoolZone {
    char * pos;
    char * maxpos;
    struct _TPoolZone * next;
    struct _TPoolZone * prev;
/*  char data[0]; Some compilers don't accept this */
    char data[1];
} TPoolZone;

typedef struct {
    TPoolZone * firstzone;
    TPoolZone * currentzone;
    uint32_t zonesize;
    TMutex mutex;
} TPool;

abyss_bool
PoolCreate(TPool *  const poolP,
           uint32_t const zonesize);

void
PoolFree(TPool * const poolP);

void *
PoolAlloc(TPool *  const poolP,
          uint32_t const size);

void
PoolReturn(TPool *  const poolP,
           void *   const blockP);

const char *
PoolStrdup(TPool *      const poolP,
           const char * const origString);


#endif
