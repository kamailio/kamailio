#include "Thrift.h"
#include "transport/TSocket.h"
#include "transport/TTransport.h"
#include "transport/TBufferTransports.h"
#include "protocol/TProtocol.h"
#include "protocol/TBinaryProtocol.h"
#include "Cassandra.h"

#include "thrift_wrapper.h"

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::transport;
using namespace apache::thrift::protocol;
using namespace org::apache::cassandra;

extern "C" int insert_wrap(char* host, int port, char* keyspace, char* column_family, char* key, char* column, char** value)
{
  int ret = -1;
  try{
    boost::shared_ptr<TTransport> socket = boost::shared_ptr<TSocket>(new TSocket(host, port));
    boost::shared_ptr<TTransport> tr = boost::shared_ptr<TFramedTransport>(new TFramedTransport (socket));
    boost::shared_ptr<TProtocol> p = boost::shared_ptr<TBinaryProtocol>(new TBinaryProtocol(tr));
    CassandraClient cass(p);
    tr->open();

    cass.set_keyspace(keyspace);

    ColumnParent cparent;
    cparent.column_family = column_family;

    string key_str = key;

    Column c;
    c.__isset.value = true;

    // have to go through all of this just to get the timestamp in ms
    struct timeval td;
    gettimeofday(&td, NULL);
    int64_t ms = td.tv_sec;
    ms = ms * 1000;
    int64_t usec = td.tv_usec;
    usec = usec / 1000;
    ms += usec;
    c.timestamp = ms;
    c.__isset.timestamp = true;

    // insert the "name" column
    c.name = column;
    c.value = *value;
    cass.insert(key_str, cparent, c, ConsistencyLevel::ONE);

    //Closing connection
    tr->flush();
    tr->close();

    //Success
    ret = 1;
  }catch(TTransportException te){
    printf("Exception: %s  [%d]\n", te.what(), te.getType());
  }catch(InvalidRequestException ire){
    printf("Exception: %s  [%s]\n", ire.what(), ire.why.c_str());
  }catch(NotFoundException nfe){
    printf("Exception: %s\n", nfe.what());
  }catch (...) {
    printf("Unknown exception occurred\n");
  }
  return ret;
}

extern "C" int retrieve_wrap(char* host, int port, char* keyspace, char* column_family, char* key, char* column, char** value)
{
  int ret = -1;
  try{
    boost::shared_ptr<TTransport> socket = boost::shared_ptr<TSocket>(new TSocket(host, port));
    boost::shared_ptr<TTransport> tr = boost::shared_ptr<TFramedTransport>(new TFramedTransport (socket));
    boost::shared_ptr<TProtocol> p = boost::shared_ptr<TBinaryProtocol>(new TBinaryProtocol(tr));
    CassandraClient cass(p);
    tr->open();

    cass.set_keyspace(keyspace);

    string key_str = key;

    Column c;
    c.__isset.value = true;

    // have to go through all of this just to get the timestamp in ms
    struct timeval td;
    gettimeofday(&td, NULL);
    int64_t ms = td.tv_sec;
    ms = ms * 1000;
    int64_t usec = td.tv_usec;
    usec = usec / 1000;
    ms += usec;
    c.timestamp = ms;
    c.__isset.timestamp = true;

    // get a single cell
    ColumnPath cp;
    cp.__isset.column = true;           // this must be set of you'll get an error re: Padraig O'Sullivan
    cp.column = column;
    cp.column_family = column_family;
    cp.super_column = "";
    ColumnOrSuperColumn sc;

    cass.get(sc, key, cp, ConsistencyLevel::ONE);

    // Copying the value back. Caller needs to free it.
    string value_str (sc.column.value.c_str());
    *value = strdup(sc.column.value.c_str());

    //Closing connection
    tr->flush();
    tr->close();

    //Success
    ret = 1;
  }catch(TTransportException te){
    printf("Exception: %s  [%d]\n", te.what(), te.getType());
  }catch(InvalidRequestException ire){
    printf("Exception: %s  [%s]\n", ire.what(), ire.why.c_str());
  }catch(NotFoundException nfe){
    printf("Exception: %s\n", nfe.what());
  }catch (...) {
    printf("Unknown exception occurred\n");
  }
  return ret;
}
