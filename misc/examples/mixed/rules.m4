#
# $Id$
#
# (c) 2003 iptel.org
#
# Rules to process sip-router.cfg templates
#

#id generator, usage: gen_id(`name'))
define(`gen_id', `ifdef(`$1',,`define(`$1', 0)')define(`$1',eval($1 + 1))')

#declare enum constants, usage: declare(route, R_MAIN, R_NAT, ...)  declare(route, R_REGISTER)
define(`declare', `ifelse($#, 1, , `gen_id(`$1'_cnt) define(`$2', indir(`$1'_cnt)) ifelse($#, 2, ,`declare(`$1', shift(shift($@)))')')')

define(`FROM_GW', `(_FROM_GW(1))')
define(`_FROM_GW', `ifdef(`GW_IP_$1', `_FROM_GW(incr($1))(src_ip == GW_IP_$1)ifelse($1, 1, , ` || ')')')

define(`TO_GW', `(@(_TO_GW(1))([;:].*)*)')
define(`_TO_GW', `ifdef(`GW_IP_$1', `_TO_GW(incr($1))(patsubst(GW_IP_$1, `\.', `\\.'))ifelse($1, 1, , `|')')')

define(`DIGEST_REALM', `SER_HOSTNAME')
define(`SER_IP_REGEX', `patsubst(SER_IP, `\.', `\\.')')
define(`SER_HOSTNAME_REGEX', `patsubst(SER_HOSTNAME, `\.', `\\.')')
define(`SER_HOST_REGEX', `((SER_IP_REGEX)|(SER_HOSTNAME_REGEX))')

define(`FROM_MYSELF', `(src_ip == SER_IP)')
