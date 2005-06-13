#!/bin/sh -x

# profile filename
PREF=$1
# number of cycles
NROFCYCLES=4
# functions to go into report
MYFN="eat_line|eat_space|eat_token"
# set to non-zero if only a report is to be generated
#REPONLY=tru

CONFIG=profile.cfg

SRD=${HOME}/sip_router
PRO=${SRD}/profile
REP=$PRO/$PREF.txt
EXEC=ser


function usage()
{
cat << EOF
usage: $0 profile_name

currently, this script starts a remote sender at alioth.
The sender iterates through all messages available in its
directory and send them many times (should be configurable).
Sip-router is configured to relay requests to benetnash:sink
and responses are built to be routed there too.
This repeats NROFCYCLES-times. Results of all cycles are then
dumped into <profile_name>-<cycle number>.txt and a
report <profile_name>.txt is generated.

EOF

exit 1
}

if [ "$#" -ne 1 ] ; then
	usage
fi

cd $SRD

function run()
{
j=0
while [ $j -lt "$NROFCYCLES" ] ; do
i=`printf "%.3d" $j`
j=`expr $j + 1`

echo "*** Entering cycle $j"

/usr/bin/time --output="$PRO/${PREF}-${i}-time.txt" ${SRD}/$EXEC -l 192.168.99.100 -D -E -d -f ${PRO}/$CONFIG &

#rsh -l jku benetnash.fokus.gmd.de 'nohup bin/sock -s -u 5060 '

rsh -l jku alioth.fokus.gmd.de 'nohup tmp/sipp/start.sh '

killall -INT $EXEC

gprof $EXEC > $PRO/${PREF}-${i}.txt

done
}

function report()
{
cat > $REP << EOF
first line ... time spent in tested procedure
second line (yyrestart) ... total time
third line (receive_msg) ... numer of calls
   %   cumulative   self              self     total
 time   seconds   seconds    calls  ms/call  ms/call  name
EOF

j=0
while [ $j -lt "$NROFCYCLES" ] ; do
i=`printf "%.3d" $j`
j=`expr $j + 1`
  FN="${PRO}/${PREF}-${i}.txt"
  echo >> $REP
  echo >> $REP
  echo $FN >> $REP
  egrep "($MYFN|yyrestart)" $FN | grep -v '\[' >> $REP
  grep 'receive_msg \[.\]' $FN | grep -v '\/' >> $REP
  echo >> $REP
  cat $PRO/${PREF}-${i}-time.txt >> $REP

done

cat >> $REP << EOF


Execution environment:
EOF
cat /proc/cpuinfo /proc/meminfo >> $REP
}

if [ -z "$REPONLY" ] ; then
	run
fi
report

echo '*** finished ***'
echo
