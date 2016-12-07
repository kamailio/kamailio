# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_python.so

# If you have multiple Python versions installed make sure to modify the
# the following to point to the correct instance. Module has been tested
# to work with 2.6 and 2.5. Python 2.4 has been only confirmed to compile,
# but no testing has been done with that.
PYTHON?=python

PYTHON_VERSION=${shell ${PYTHON} -c "import distutils.sysconfig;print distutils.sysconfig.get_config_var('VERSION')"}
PYTHON_LIBDIR=${shell ${PYTHON} -c "import distutils.sysconfig;print distutils.sysconfig.get_config_var('LIBDIR')"}
PYTHON_LDFLAGS=${shell ${PYTHON} -c "import distutils.sysconfig;print distutils.sysconfig.get_config_var('LINKFORSHARED')"}
PYTHON_INCDIR=${shell ${PYTHON} -c "import distutils.sysconfig;print distutils.sysconfig.get_python_inc()"}

LIBS=-L${PYTHON_LIBDIR} ${PYTHON_LDFLAGS} -lpython${PYTHON_VERSION}

ifeq ($(OS), freebsd)
LIBS+=-pthread
endif

DEFS+=-I${PYTHON_INCDIR}
DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules

