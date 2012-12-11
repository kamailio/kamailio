# -*- coding: utf-8 -*-

"""
    Script for logging test.
    Added:   2012-12-03: Created by: Konstantin M. <evilzluk@gmail.com>
    Changed: 2012-12-11: Changed code to use Router.Logger
    Changed: 2012-12-11: Changed code to use Router.Ranks
"""

import Router.Logger as Logger
import Router.Ranks as Ranks

"""
    Router.Logger Module Properties:
       Log Levels:
           L_ALERT
           L_BUG
           L_CRIT2
           L_CRIT
           L_ERR
           L_WARN
           L_NOTICE
           L_INFO
           L_DBG

       Log Facilities:
           DEFAULT_FACILITY

    Router.Logger Module Methods:
       LM_GEN1(self, int log_level, str msg)
       LM_GEN2(self, int log_facility, int log_level, str msg)
       LM_ALERT(self, str msg)
       LM_CRIT(self, str msg)
       LM_ERR(self, str msg)
       LM_WARN(self, str msg)
       LM_NOTICE(self, str msg)
       LM_INFO(self, str msg)
       LM_DBG(self, str msg)


    Router.Ranks Module Properties:
        PROC_MAIN
        PROC_TIMER
        PROC_RPC
        PROC_FIFO
        PROC_TCP_MAIN
        PROC_UNIXSOCK
        PROC_ATTENDANT
	PROC_INIT
	PROC_NOCHLDINIT
	PROC_SIPINIT
	PROC_SIPRPC
	PROC_MIN

"""

class Loggers:

    def __init__(self):
	pass

    def __del__(self):
	pass

    def child_init(self, rank):
	if rank == Ranks.PROC_MAIN:
	    Logger.LM_ERR("rank is PROC_MAIN")
	elif rank == Ranks.PROC_TIMER:
	    Logger.LM_ERR("rank is PROC_TIMER")
	elif rank == Ranks.PROC_RPC:
	    Logger.LM_ERR("rank is PROC_RPC")
	elif rank == Ranks.PROC_FIFO:
	    Logger.LM_ERR("rank is PROC_FIFO")
	elif rank == Ranks.PROC_TCP_MAIN:
	    Logger.LM_ERR("rank is PROC_TCP_MAIN")
	elif rank == Ranks.PROC_UNIXSOCK:
	    Logger.LM_ERR("rank is PROC_UNIXSOCK")
	elif rank == Ranks.PROC_ATTENDANT:
	    Logger.LM_ERR("rank is PROC_ATTENDANT")
	elif rank == Ranks.PROC_INIT:
	    Logger.LM_ERR("rank is PROC_INIT")
	elif rank == Ranks.PROC_NOCHLDINIT:
	    Logger.LM_ERR("rank is PROC_NOCHLDINIT")
	elif rank == Ranks.PROC_SIPINIT:
	    Logger.LM_ERR("rank is PROC_SIPINIT")
	elif rank == Ranks.PROC_SIPRPC:
	    Logger.LM_ERR("rank is PROC_SIPRPC")
	elif rank == Ranks.PROC_MIN:
	    Logger.LM_ERR("rank is PROC_MIN")

	return 0

    def TestLoggers(self, msg, args):
	Logger.LM_GEN1(Logger.L_INFO,                           	"Loggers.py:     LM_GEN1: msg: %s" % str(args))
	Logger.LM_GEN2(Logger.L_INFO, Logger.DEFAULT_FACILITY,	        "Loggers.py:     LM_GEN2: msg: %s" % str(args))
	Logger.LM_ALERT(                                        	"Loggers.py:    LM_ALERT: msg: %s" % str(args))
	Logger.LM_CRIT(                                         	"Loggers.py:     LM_CRIT: msg: %s" % str(args))
	Logger.LM_ERR(                                          	"Loggers.py:      LM_ERR: msg: %s" % str(args))
	Logger.LM_WARN(                                         	"Loggers.py:     LM_WARN: msg: %s" % str(args))
	Logger.LM_NOTICE(                                       	"Loggers.py:   LM_NOTICE: msg: %s" % str(args))
	Logger.LM_INFO(                                         	"Loggers.py:     LM_INFO: msg: %s" % str(args))
	Logger.LM_DBG(                                          	"Loggers.py:      LM_DBG: msg: %s" % str(args))
	return 1

def mod_init():
    return Loggers()

