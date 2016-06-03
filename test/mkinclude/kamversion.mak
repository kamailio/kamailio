# Log the latest test version
ifeq ($(KAMBIN),)
	$(error NO KAMBIN??? )
endif
KAMVER=$(shell $(KAMBIN)/kamailio -v|head -n1)
$(shell echo "$(KAMVER)" > test.log)
