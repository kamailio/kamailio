#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=phonenum.so

CXX=g++
LD=g++
LIB_DIR = /opt/local/lib

LIBS= -L${LIB_DIR} cphonenumber.o -lphonenumber -lgeocoding

DEFS+= -I/opt/local/include

CXXFLAGS=$(CFLAGS:-Wno-deprecated option=)
CXXFLAGS+=-Wno-write-strings -Wno-deprecated -Wno-unused-function -Wno-sign-compare -Wno-strict-aliasing

include ../../Makefile.modules

cphonenumber.o: cphonenumber.cpp cphonenumber.h
	@echo "Compiling $<"
	$(CXX) -std=c++11 $(CXXFLAGS) $(CFLAGS) $(C_DEFS) $(DEFS) -c $< -o $@

phonenum.so: cphonenumber.o

modclean:
	-@rm -f cphonenumber.o

clean clean-modules: modclean

proper realclean distclean maintainer-clean: modclean
