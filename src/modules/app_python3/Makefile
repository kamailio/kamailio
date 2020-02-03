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

PYTHON3_VERSION=${shell ${PYTHON3} -c "import distutils.sysconfig;print(distutils.sysconfig.get_config_var('VERSION'))"}
PYTHON3_LIBDIR=${shell ${PYTHON3} -c "import distutils.sysconfig;print(distutils.sysconfig.get_config_var('LIBDIR'))"}
PYTHON3_LDFLAGS=${shell ${PYTHON3} -c "import distutils.sysconfig;print(distutils.sysconfig.get_config_var('LINKFORSHARED'))"}
PYTHON3_INCDIR=${shell ${PYTHON3} -c "import distutils.sysconfig;print(distutils.sysconfig.get_python_inc())"}

LIBS=${shell ${PYTHON3}-config --ldflags}

ifeq ($(OS), freebsd)
LIBS+=-pthread
endif

DEFS+=-I${PYTHON3_INCDIR}

include ../../Makefile.modules

