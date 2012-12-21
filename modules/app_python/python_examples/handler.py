import sys
from Router import LM_ERR

class test:
    def __init__(self):
        LM_ERR('test.__init__\n')

    def child_init(self, y):
        LM_ERR('test.child_init(%d)\n' % y)
        return 0

    def handler(self, msg, args):
        LM_ERR('test.handler(%s, %s)\n' % (msg.Type, str(arg)))
        if msg.Type == 'SIP_REQUEST':
            if msg.Method == 'INVITE':
                msg.rewrite_ruri('sip:0022@192.168.2.24:5073')
            LM_ERR('SIP request, method = %s, RURI = %s, From = %s\n' % (msg.Method, msg.RURI, msg.getHeader('from')))
            LM_ERR('received from %s:%d\n' % msg.src_address)
        else:
            LM_ERR('SIP reply, status = %s\n' % msg.Status)
            LM_ERR('received from %s:%d\n' % msg.src_address)
        msg.call_function('append_hf', 'This-is: test\r\n')
        return 1

def mod_init():
    return test()
