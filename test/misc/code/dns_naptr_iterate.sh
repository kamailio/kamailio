#!/bin/sh
#
# Build and run the NAPTR failover state regression test (GH #4911).
#
# Kamailio has no unit test harness for the core, so the test is linked
# against the core objects of a normal build.  Run it from the top of the
# source tree:
#
#   sh test/misc/code/dns_naptr_iterate.sh
#
# It builds the tree first ("make all") if the objects are not there yet.

set -e

top=$(pwd)
if [ ! -f "$top/src/core/dns_cache.c" ]; then
	echo "run this script from the top of the kamailio source tree" >&2
	exit 1
fi

CC=${CC:-cc}
OBJCOPY=${OBJCOPY:-objcopy}

if [ ! -f "$top/src/main.o" ] || [ ! -f "$top/src/core/dns_cache.o" ]; then
	echo "* building the core objects ..."
	make cfg >/dev/null
	make all >/dev/null
fi

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

defs=$(sed -n 's/^C_DEFS *= *//p' "$top/src/config.mak" | head -n 1)

# main.o holds the core global variables (log_stderr, tcp_disable, ...) but
# also main() - rename it out of the way
$OBJCOPY --redefine-sym main=ksr_main_unused "$top/src/main.o" "$out/main.o"

extra=""
if [ -f "$top/src/cfg.tab.o" ] && [ -f "$top/src/lex.yy.o" ]; then
	extra="$top/src/cfg.tab.o $top/src/lex.yy.o"
else
	# no bison/flex available: the config parser is not needed by the test,
	# stub the few symbols it exports
	cat >"$out/pp_stubs.c" <<'STUBS'
#include <stdio.h>
FILE *yyin = 0;
int yyparse(void)
{
	return -1;
}
int pp_ifexp_state = 0;
void ksr_cfg_print_initial_state(void) {}
int pp_define(int len, const char *text)
{
	return -1;
}
int pp_define_set(int len, char *text, int mode)
{
	return -1;
}
int pp_define_set_type(int type)
{
	return -1;
}
int pp_define_get(int len, const char *text)
{
	return -1;
}
int pp_lookup(int len, const char *text)
{
	return -1;
}
char *pp_get_define_name(int idx)
{
	return 0;
}
int pp_get_define(int len, const char *text)
{
	return -1;
}
STUBS
	eval $CC -c -o "$out/pp_stubs.o" $defs -I"$top/src" "$out/pp_stubs.c"
	extra="$out/pp_stubs.o"
fi

objs=$(find "$top/src/core" "$top/src/lib" -name '*.o' | sort)

eval $CC -o "$out/dns_naptr_iterate" -I"$top/src" $defs \
	"$top/test/misc/code/dns_naptr_iterate.c" "$out/main.o" $extra \
	$objs -lresolv -lpthread -ldl -lm

"$out/dns_naptr_iterate"
