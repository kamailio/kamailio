#ifndef DATE_H_INCLUDED
#define DATE_H_INCLUDED

#include <time.h>

#include <xmlrpc-c/abyss.h>

typedef struct tm TDate;

abyss_bool
DateToString(TDate * const tmP,
             char *  const s);

abyss_bool
DateToLogString(TDate * const tmP,
                char *  const s);

abyss_bool
DateDecode(const char *  const dateString,
           TDate *       const tmP);

int32_t
DateCompare(TDate * const d1,
            TDate * const d2);

abyss_bool
DateFromGMT(TDate * const d,
            time_t  const t);

abyss_bool
DateFromLocal(TDate * const d,
              time_t const t);

#endif
