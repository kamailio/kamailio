# WARNING: do not run this directly, it should be run by the main Makefile

# Based on Makefile of 'db_cassandra' module. Thanks to 'Anca Vamanu' and 'Boudewyn Ligthart'.

include ../../Makefile.defs

CXX=g++
LD=g++
auto_gen=
NAME=ndb_cassandra.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
ifneq ($(BUILDER),)
	PKGLIBTHRIFT = $(shell $(BUILDER) --exists thrift > /dev/null 2>&1 ; echo $$? )
ifneq ($(PKGLIBTHRIFT),0)
	BUILDER =
endif
endif
endif

ifneq ($(BUILDER),)
	THRIFT_INC = $(shell $(BUILDER) --cflags thrift)
	DEFS += $(THRIFT_INC)
	LIBS += $(shell $(BUILDER) --libs thrift)
else
	THRIFT_INC = -I/usr/local/include
	DEFS += $(THRIFT_INC)
	LIBS += -L/usr/local/lib thrift_wrapper.o cassandra_constants.o cassandra_types.o Cassandra.o -lthrift
endif

CXXFLAGS=$(CFLAGS:-Wno-deprecated option=)
CXXFLAGS+=-Wno-write-strings -Wno-deprecated -Wno-unused-function -Wno-sign-compare -Wno-strict-aliasing

include ../../Makefile.modules

cassandra_constants.o: cassandra_constants.cpp cassandra_constants.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) $(THRIFT_INC) -c $< -o $@

cassandra_types.o: cassandra_types.cpp cassandra_types.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) $(THRIFT_INC) -c $< -o $@

Cassandra.o: Cassandra.cpp Cassandra.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) $(THRIFT_INC) -c $< -o $@

thrift_wrapper.o: thrift_wrapper.cpp thrift_wrapper.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(C_DEFS) $(DEFS) -c $< -o $@

ndb_cassandra.so: thrift_wrapper.o cassandra_constants.o cassandra_types.o Cassandra.o
