# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_python3.so

# If you have multiple Python versions installed make sure to modify the
# the following to point to the correct instance. Module has been tested
# to work with 2.6 and 2.5. Python 2.4 has been only confirmed to compile,
# but no testing has been done with that.
PYTHON3?=python3

LIBS=${shell \
	tmp_py3_libs=$$(${PYTHON3}-config --ldflags --embed 2>/dev/null) || \
	tmp_py3_libs=$$(${PYTHON3}-config --ldflags); \
	echo $$tmp_py3_libs}

ifeq ($(OS), freebsd)
LIBS+=-pthread
endif

DEFS+=${shell ${PYTHON3}-config --includes}

include ../../Makefile.modules

