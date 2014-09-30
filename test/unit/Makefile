# Makefile for running test unit
#

all:
	-@for FILE in $(wildcard *.sh) ; do \
		if [ -f $$FILE ] ; then \
			if [ -x $$FILE ] ; then \
				echo "run test `basename $$FILE .sh`:" `head -n 2 "$$FILE" | tail -n 1 | cut -c 3-` ; \
				./$$FILE ; \
				ret=$$? ; \
					if [ ! "$$ret" -eq 0 ] ; then \
						echo "failed" ; \
					fi ; \
			fi ; \
		fi ; \
	done ;

run:
	-@if [ -f $(UNIT) ] ; then \
		if [ -x $(UNIT) ] ; then \
			echo "Run test `basename $(UNIT) .sh`:" `head -n 2 "$(UNIT)" | tail -n 1 | cut -c 3-` ; \
			./$(UNIT) ; \
			ret=$$? ; \
				if [ ! "$$ret" -eq 0 ] ; then \
					echo "Test unit file $(UNIT): failed" ; \
				else \
					echo "Test unit file $(UNIT): ok" ; \
				fi ; \
		fi ; \
	else \
		echo "Test unit file $(UNIT): not found" ; \
	fi ;

