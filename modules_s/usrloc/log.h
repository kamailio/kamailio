#ifndef LOG_H
#define LOG_H

/* $Id$ */

#include "../../dprint.h"


#ifdef __FILE__
#define _F __FILE__
#else
#define _F
#endif

#ifdef __LINE__
#define _L __LINE__
#else
#define _L
#endif

#ifdef __func__
#define _FU __func__
#elif defined __FUNCTION__
#define _FU __FUNCTION__
#else
#define _FU
#endif


/*
 * CRIT macro: Log a critical condition
 */
#ifdef CRIT
#undef CRIT
#endif

#ifdef __SUNPRO_C
#define CRIT(...) LOG(L_CRIT, __VA_ARGS__)
#else
#define CRIT(fmt, args...) LOG(L_CRIT, fmt, ## args)
#endif

/*
 * ALERT macro: Log an alert, action must be taken immediately 
 */
#ifdef ALERT
#undef ALERT
#endif

#ifdef __SUNPRO_C
#define ALERT(...) LOG(L_ALERT, __VA_ARGS__)
#else
#define ALERT(fmt, args...) LOG(L_ALERT, fmt, ## args)
#endif

/*
 * ERR macro: Log an error condition
 */
#ifdef ERR
#undef ERR
#endif

#ifdef __SUNPRO_C
#define ERR(...) LOG(L_ERR, __VA_ARGS__)
#else
#define ERR(fmt, args...) LOG(L_ERR, fmt, ## args)
#endif

/*
 * WARN macro: Log a warning condition
 */
#ifdef WARN
#undef WARN
#endif

#ifdef __SUNPRO_C
#define WARN(...) LOG(L_WARN, __VA_ARGS__)
#else
#define WARN(fmt, args...) LOG(L_WARN, fmt, ## args)
#endif

/*
 * NOTICE macro: Normal, but significant condition
 */
#ifdef NOTICE
#undef NOTICE
#endif

#ifdef __SUNPRO_C
#define NOTICE(...) LOG(L_NOTICE, __VA_ARGS__)
#else
#define NOTICE(fmt, args...) LOG(L_NOTICE, fmt, ## args)
#endif

/*
 * INFO macro: Informational
 */
#ifdef INFO
#undef INFO
#endif

#ifdef __SUNPRO_C
#define INFO(...) LOG(L_INFO, __VA_ARGS__)
#else
#define INFO(fmt, args...) LOG(L_INFO, fmt, ## args)
#endif

/*
 * DBG macro: debug-level messages
 */
#ifdef DBG
#undef DBG
#endif

#ifdef __SUNPRO_C
#define DBG(...) LOG(L_DBG, __VA_ARGS__)
#else
#define DBG(fmt, args...) LOG(L_DBG, fmt, ## args)
#endif

#endif
