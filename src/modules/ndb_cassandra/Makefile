# WARNING: do not run this directly, it should be run by the master Makefile

# Based on Makefile of 'db_cassandra' module. Thanks to 'Anca Vamanu' and 'Boudewyn Ligthart'.

include ../../Makefile.defs

CXX=g++
LD=g++
auto_gen=
NAME=ndb_cassandra.so

THRIFT_DIR = /usr/local/include/thrift
LIB_DIR = /usr/local/lib

DEFS += -I${THRIFT_DIR} -DKAMAILIO_MOD_INTERFACE
LIBS += -L${LIB_DIR} thrift_wrapper.o cassandra_constants.o cassandra_types.o Cassandra.o -lthrift

SERLIBPATH=../../lib

CXXFLAGS=$(CFLAGS:-Wno-deprecated option=)
CXXFLAGS+=-Wno-write-strings -Wno-deprecated -Wno-unused-function -Wno-sign-compare -Wno-strict-aliasing

SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi

include ../../Makefile.modules

cassandra_constants.o: cassandra_constants.cpp cassandra_constants.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -I/usr/local/include/thrift -c $< -o $@

cassandra_types.o: cassandra_types.cpp cassandra_types.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -I/usr/local/include/thrift -c $< -o $@

Cassandra.o: Cassandra.cpp Cassandra.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -I/usr/local/include/thrift -c $< -o $@

thrift_wrapper.o: thrift_wrapper.cpp thrift_wrapper.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(C_DEFS) $(DEFS) -c $< -o $@

ndb_cassandra.so: thrift_wrapper.o cassandra_constants.o cassandra_types.o Cassandra.o	
