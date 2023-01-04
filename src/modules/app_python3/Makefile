#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_python3.so

# If you have multiple Python versions installed make sure to modify the
# the following to point to the correct instance.
PYTHON3?=$(shell which python3)
ifeq ($(PYTHON3),)
PYTHON3=python
endif

PYTHON3CONFIG=$(shell which ${PYTHON3}-config)
ifeq ($(PYTHON3CONFIG),)
python_version_full := $(wordlist 2,4,$(subst ., ,$(shell ${PYTHON3} --version 2>&1)))
python_version_major := $(word 1,${python_version_full})
python_version_minor := $(word 2,${python_version_full})
PYTHON3 = python${python_version_major}.${python_version_minor}
PYTHON3CONFIG=$(shell which ${PYTHON3}-config)
ifeq ($(PYTHON3CONFIG),)
PYTHON3CONFIG=${PYTHON3}-config
endif
endif

LIBS=${shell \
	tmp_py3_libs=$$(${PYTHON3CONFIG} --ldflags --embed 2>/dev/null) || \
	tmp_py3_libs=$$(${PYTHON3CONFIG} --ldflags); \
	echo $$tmp_py3_libs}

ifeq ($(OS), freebsd)
LIBS+=-pthread
endif

DEFS+=${shell ${PYTHON3CONFIG} --includes}

include ../../Makefile.modules

