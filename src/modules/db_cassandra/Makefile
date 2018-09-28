#
# db_cassandra module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

CXX=g++
LD=g++
auto_gen=
NAME=db_cassandra.so

INCLUDE_DIR = /usr/include/thrift
LIB_DIR = /usr/lib
DEFS += -I${INCLUDE_DIR}
LIBS += -L${LIB_DIR} dbcassa_base.o cassandra_constants.o cassandra_types.o Cassandra.o -lthrift

CXXFLAGS=$(CFLAGS:-Wno-deprecated option=)
CXXFLAGS+= -Wno-write-strings -Wno-deprecated -Wno-unused-function -Wno-sign-compare -Wno-strict-aliasing

SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1

include ../../Makefile.modules

cassandra_constants.o: cassandra_constants.cpp cassandra_constants.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -I${INCLUDE_DIR} -c $< -o $@

cassandra_types.o: cassandra_types.cpp cassandra_types.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -I${INCLUDE_DIR} -c $< -o $@

Cassandra.o: Cassandra.cpp Cassandra.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) -I${INCLUDE_DIR} -c $< -o $@

dbcassa_base.o: dbcassa_base.cpp dbcassa_base.h dbcassa_table.h
	@echo "Compiling $<"
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(C_DEFS) $(DEFS) -c $< -o $@

db_cassandra.so: dbcassa_base.o cassandra_constants.o cassandra_types.o Cassandra.o
