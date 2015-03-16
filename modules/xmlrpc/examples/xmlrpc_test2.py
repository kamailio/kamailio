import sys
import xmlrpclib

# Usage:  python xmlrpc_test2.py command [params...]
#
# python script for sending an xmlrpc command to ser's xmlrpc module.
# This script uses python xmlrpclib directly and expects the remote side to
# immediately close the connection after answering (broken xmlrpclib
# behaviour).
# There are 2 way to make it work with ser xmlrpc module: define a
# better transport class (that's what the xmlrpc_test.py script is doing) or
# change ser xmlrpc route so that it will close the connection after each
# processes xmlrpc request (e.g. add a drop -1 at the end).
#
# See also: xmlrpc_test.py (better version, using a redefined transport class).
#
# History:
# --------
#  2009-07-13  initial version (andrei)
#

XMLRPC_SERVER = "127.0.0.1"
XMLRPC_PORT = 5060


if len(sys.argv) < 2:
    sys.exit("Usage: " + sys.argv[0] + " rpc_command [args...]")

client = xmlrpclib.ServerProxy(
    "http://{0}:{1}".format(XMLRPC_SERVER, XMLRPC_PORT))
res = getattr(client, sys.argv[1])(*sys.argv[2:])
print res

#####
# You can use this simple way.
#client =xmlrpclib.ServerProxy("http://127.0.0.1:5060")
#res = client.htable.listTables()
#print res
