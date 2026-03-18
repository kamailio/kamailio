# Makefile for running test unit
#
TESTS_FILES ?= $(wildcard *.sh)
TESTS_EXCLUDE ?=
TESTS ?= $(filter-out $(patsubst %,%.sh,$(TESTS_EXCLUDE)), $(TESTS_FILES))

# run all tests
all:
	@for FILE in $(TESTS) ; do \
		if [ -f $$FILE ] ; then \
			if [ -x $$FILE ] ; then \
				echo "Run test `basename $$FILE .sh`:" `head -n 2 "$$FILE" | tail -n 1 | cut -c 3-` ; \
				./$$FILE ; \
				ret=$$? ; \
				if [ ! "$$ret" -eq 0 ] ; then \
					echo "Test unit file $$FILE: failed" ; \
				else \
					echo "Test unit file $$FILE: ok" ; \
				fi ; \
			fi ; \
		fi ; \
	done ; \
	exit $$RES;

# run one test specified in variable UNIT
# example: make UNIT=1.sh run
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
