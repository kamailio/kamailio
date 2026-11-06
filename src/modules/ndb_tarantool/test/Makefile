KAMAILIO ?= ../../../../src/kamailio
MODULES_DIR ?= ../../../../src/modules

.PHONY: all test run clean

all: run

test:
	@echo "Checking syntax of test.cfg..."
	$(KAMAILIO) -c -m 64 -M 64 -L $(MODULES_DIR) -f test.cfg

run:
	@chmod +x run-tests.sh
	@./run-tests.sh

clean:
	rm -f *.log /tmp/kam_mod_test.log
