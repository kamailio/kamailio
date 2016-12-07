#!/bin/sh 
#
# $Id$
#
# utility for displaying geographical break-down of usrloc population
# it takes functional netgeo support for ser (currently, an
# experimental unavailable feature)
# 


DB_HOST=dbhost
DB_USER=ser
DB_PW=heslo

# ---

TMP=/tmp/geo_split.$$

stats()
{
	DOMAIN_CNT=`grep $1 $TMP | wc | awk ' { print $1  } '`
	if [ "$DOMAIN_CNT" -eq "0" ] ; then
		PC="0"
	else
		PC=`expr $DOMAIN_CNT \* 100 / $2`
	fi
	printf "$1: $DOMAIN_CNT $PC %%\n"
	grep -v $1 $TMP > $TMP.2
	mv $TMP.2 $TMP
}

mysql -h $DB_HOST --batch -u $DB_USER -p$DB_PW ser -e "select location from netgeo_cache" |
awk -F '/' '
	BEGIN { line=0 }
	{ line++ }
	line==1 { next; } # skip heading
	length()==0 { next; } # skip empty lines
	/^[A-Z][A-Z ]*\/[A-Z ]+/ { print $2; next;} 
	/^[A-Z]+/ { print " " $1; next;} 
	/^ [A-Z]+/ { print $1; next;} 
	{ print "error" > "/dev/stderr" }' |
sort -b > $TMP

export TOTAL_CNT=`wc $TMP|awk '{print $1}'`

printf "Total: $TOTAL_CNT\n"
(for i in AU DE US NL CZ UK RO RU TR TW SW JP CA HK IT AR BE CN FI GL IN KR SE UY; do 
	stats $i $TOTAL_CNT
done) 
#| sort
cat $TMP

rm $TMP*
