#
# $Id$
#
# (c) 2003 iptel.org
#
# Rules to process ser.cfg templates
#

define(`FROM_GW', `(_FROM_GW(1))')
define(`_FROM_GW', `ifdef(`GW_IP_$1', `_FROM_GW(incr($1))(src_ip == GW_IP_$1)ifelse($1, 1, , ` || ')')')

define(`TO_GW', `(@(_TO_GW(1))([;:].*)*)')
define(`_TO_GW', `ifdef(`GW_IP_$1', `_TO_GW(incr($1))(patsubst(GW_IP_$1, `\.', `\\.'))ifelse($1, 1, , `|')')')

define(`DIGEST_REALM', `SER_HOSTNAME')
define(`SER_IP_REGEX', `patsubst(SER_IP, `\.', `\\.')')
define(`SER_HOSTNAME_REGEX', `patsubst(SER_HOSTNAME, `\.', `\\.')')
define(`SER_HOST_REGEX', `((SER_IP_REGEX)|(SER_HOSTNAME_REGEX))')

define(`FROM_MYSELF', `(src_ip == SER_IP)')

define(`ACC_FLAG', 1)
define(`MISSED_FLAG', 3)
define(`VM_FLAG', 4)
define(`NAT_FLAG', 6)

define(`PSTN', 3)
define(`NAT', 1)
define(`VOICEMAIL', 4)
