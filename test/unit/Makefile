all:
	-@for FILE in $(wildcard *.sh) ; do \
		if [ -f $$FILE ] ; then \
			if [ -x $$FILE ] ; then \
				echo "run test `basename $$FILE .sh`:" `head "$$FILE" -n 2 | tail -n 1 | cut -c 3-` ; \
				./$$FILE ; \
				ret=$$? ; \
					if [ ! "$$ret" -eq 0 ] ; then \
						echo "failed" ; \
					fi ; \
			fi ; \
		fi ; \
	done ;
