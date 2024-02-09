#!/usr/bin/python3

import KSR

# this assumes you have your mock KSR.py in the local directory e.g. /test/
# and your kemi code is in ../conf/kamailio.py

sys.path.insert(0, "../conf/")
import kamailio as kamailio


#return sip:hello@world only if $ru is passed to pv.get
KSR._mock_data['pv']['get'] = {}
KSR._mock_data['pv']['get']['$ru'] = "sip:hello@world"
print("Got a value of: " + KSR.pv.get("$ru"))

#return maxfwd.process_maxfwd return 2 regardless of value passed
KSR._mock_data['maxfwd']['process_maxfwd'] = 2
KSR.maxfwd.process_maxfwd(10)
print("Got a value of: " + str(KSR.maxfwd.process_maxfwd(10)))

#set a function pointer to see if hdr.append is called
appendCalled = False
def appendHeader(param0: str):
    global appendCalled
    if param0.startswith("X-HDR:"):
        appendCalled = True
    return 1

KSR._mock_data['hdr']['append'] = appendHeader
KSR.hdr.append("X-HDR: my-header")

k = kamailio.kamailio()
k.ksr_request_route(None) # Call the kemi script, the mock implementations will be called

# Validate the results
if appendCalled:
    print("hdr.append successfully called!")
else:
    print("hdr.append failed to be called")
