#!/bin/sh 

for i in `find . -name '*.[hc]'` ; do
	mv $i $i.pregpl
	awk -f test/gplize.awk $i.pregpl > $i
done
