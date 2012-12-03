# -*- coding: utf-8 -*-

"""
    Script for logging test.
    2012.12.03: Created by: Konstantin M. <evilzluk@gmail.com>
"""

import Router

"""
    Module Properties:
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

    Module Methods:
       LM_GEN1(self, int log_level, str msg)
       LM_GEN2(self, int log_facility, int log_level, str msg)
       LM_ALERT(self, str msg)
       LM_CRIT(self, str msg)
       LM_ERR(self, str msg)
       LM_WARN(self, str msg)
       LM_NOTICE(self, str msg)
       LM_INFO(self, str msg)
       LM_DBG(self, str msg)


"""

class Loggers:

    def __init__(self):
	pass

    def __del__(self):
	pass

    def child_init(self, y):
	return 0

    def TestLoggers(self, msg, args):
	Router.LM_GEN1(Router.L_INFO,                           	"Loggers.py:     LM_GEN1: msg: %s" % str(args))
	Router.LM_GEN2(Router.L_INFO, Router.DEFAULT_FACILITY,	        "Loggers.py:     LM_GEN2: msg: %s" % str(args))
	Router.LM_ALERT(                                        	"Loggers.py:    LM_ALERT: msg: %s" % str(args))
	Router.LM_CRIT(                                         	"Loggers.py:     LM_CRIT: msg: %s" % str(args))
	Router.LM_ERR(                                          	"Loggers.py:      LM_ERR: msg: %s" % str(args))
	Router.LM_WARN(                                         	"Loggers.py:     LM_WARN: msg: %s" % str(args))
	Router.LM_NOTICE(                                       	"Loggers.py:   LM_NOTICE: msg: %s" % str(args))
	Router.LM_INFO(                                         	"Loggers.py:     LM_INFO: msg: %s" % str(args))
	Router.LM_DBG(                                          	"Loggers.py:      LM_DBG: msg: %s" % str(args))
	return 1

def mod_init():
    return Loggers()

