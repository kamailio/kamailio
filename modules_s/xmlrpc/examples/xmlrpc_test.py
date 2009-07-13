import xmlrpclib, httplib, sys

# Usage:  python xmlrpc_test.py command [params...]
#
# python script for sending an xmlrpc command to ser's xmlrpc module.
# Note: it uses a re-defined transport class that does not depend on the
# server side closing the connection (it can be used to send multiple
# commands without closing the connections and it will work without any
# special workarounds in the ser.cfg xmlrpc route).
#
# Credits: the transport class comes from ser_ctl, heavily trimmed to a very
# basic version (the ser_ctl version is much more complex, supports
# authentication, ssl a.s.o). See
# http://git.sip-router.org/cgi-bin/gitweb.cgi?p=ser;a=blob;f=ser_ctl/serctl/serxmlrpc.py#l73 for the original (better) version.
#
#
# History:
# --------
#  2009-07-13  initial version (andrei)
#


XMLRPC_SERVER = "127.0.0.1"
XMLRPC_PORT = 5060

class Transport:
	def __init__(self):
		self.conn=httplib.HTTPConnection(XMLRPC_SERVER, str(XMLRPC_PORT));

	def _http_request(self, uripath, body, host):
		self.conn.request("POST", uripath, body, {})

	def request(self, host, uripath, body, verbose=0):
		self._http_request(uripath, body, host)
		response=self.conn.getresponse()
		if response.status != 200:
			raise xmlrpclib.ProtocolError(host+uripath, response.status,
							response.reason, response.msg)
		data=response.read()
		parser, unmarshaller=xmlrpclib.getparser()
		parser.feed(data)
		parser.close()
		return unmarshaller.close()

if len(sys.argv) < 2:
	sys.exit("Usage: "+sys.argv[0]+" rpc_command [args...]");
transport=Transport()
c=xmlrpclib.ServerProxy("http://" + XMLRPC_SERVER+ ":" + str(XMLRPC_PORT),
						transport)
res=getattr(c, sys.argv[1])(*sys.argv[2:])
print res;
