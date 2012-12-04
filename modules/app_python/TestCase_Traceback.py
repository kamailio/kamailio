# -*- coding: utf-8 -*-

"""
    Script for traceback test.
    2012.12.03: Created by: Konstantin M. <evilzluk@gmail.com>
"""

import pprint

class Loggers:

    def __init__(self):
	pass

    def __del__(self):
	pass

    def child_init(self, y):
	return 0

    def BuggyCode_lvl5(self, a):
	a / 0

    def BuggyCode_lvl4(self, a):
	return self.BuggyCode_lvl5(a)

    def BuggyCode_lvl3(self, a):
	return self.BuggyCode_lvl4(a)

    def BuggyCode_lvl2(self, a):
	return self.BuggyCode_lvl3(a)

    def BuggyCode(self, a, b=None):
	return self.BuggyCode_lvl2(a)



def mod_init():
    return Loggers()



if __name__ != "__main__":
    import Router
else:
    mod_init().BuggyCode(0)
