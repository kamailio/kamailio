# $Id$
COREPATH=../..
include $(COREPATH)/Makefile.defs
include $(COREPATH)/config.mak

ifeq ($(basedir),)
kamctltmpdir = /tmp
else
kamctltmpdir = $(basedir)
endif

all:
		@echo "No compilation needed for kamctl"

install-if-newer: install

install: install-cfg install-bin install-man install-modules

install-cfg: $(cfg_prefix)/$(cfg_dir)
		# kamctl config
		$(INSTALL_TOUCH)   $(cfg_prefix)/$(cfg_dir)/kamctlrc.sample
		$(INSTALL_CFG) kamctlrc \
			$(cfg_prefix)/$(cfg_dir)/kamctlrc.sample
		if [ ! -f $(cfg_prefix)/$(cfg_dir)/kamctlrc ]; then \
			mv -f $(cfg_prefix)/$(cfg_dir)/kamctlrc.sample \
				$(cfg_prefix)/$(cfg_dir)/kamctlrc; \
		fi


install-bin: $(bin_prefix)/$(bin_dir)
		cat kamctl | \
		sed -e "s#/usr/local/sbin#$(bin_target)#g" | \
		sed -e "s#/usr/local/lib/kamailio#$(lib_target)#g" | \
		sed -e "s#/usr/local/etc/kamailio#$(cfg_target)#g"  >$(kamctltmpdir)/kamctl
		$(INSTALL_TOUCH) $(bin_prefix)/$(bin_dir)/kamctl
		$(INSTALL_BIN) $(kamctltmpdir)/kamctl $(bin_prefix)/$(bin_dir)
		rm -fr $(kamctltmpdir)/kamctl
		sed -e "s#/usr/local/sbin#$(bin_target)#g" \
			< kamctl.base > $(kamctltmpdir)/kamctl.base
		mkdir -p $(modules_prefix)/$(lib_dir)/kamctl 
		$(INSTALL_TOUCH) \
			$(modules_prefix)/$(lib_dir)/kamctl
		$(INSTALL_CFG) $(kamctltmpdir)/kamctl.base \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.base
		rm -fr $(kamctltmpdir)/kamctl.base
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.ctlbase > $(kamctltmpdir)/kamctl.ctlbase
		$(INSTALL_CFG) $(kamctltmpdir)/kamctl.ctlbase \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.ctlbase
		rm -fr $(kamctltmpdir)/kamctl.ctlbase
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.fifo > $(kamctltmpdir)/kamctl.fifo
		$(INSTALL_CFG) $(kamctltmpdir)/kamctl.fifo \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.fifo
		rm -fr $(kamctltmpdir)/kamctl.fifo
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.ser > $(kamctltmpdir)/kamctl.ser
		$(INSTALL_CFG) $(kamctltmpdir)/kamctl.ser \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.ser
		rm -fr $(kamctltmpdir)/kamctl.ser
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.ser_mi > $(kamctltmpdir)/kamctl.ser_mi
		$(INSTALL_CFG) $(kamctltmpdir)/kamctl.ser_mi \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.ser_mi
		rm -fr $(kamctltmpdir)/kamctl.ser_mi
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.unixsock > $(kamctltmpdir)/kamctl.unixsock
		$(INSTALL_CFG) $(kamctltmpdir)/kamctl.unixsock \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.unixsock
		rm -fr $(kamctltmpdir)/kamctl.unixsock
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.sqlbase > $(kamctltmpdir)/kamctl.sqlbase
		$(INSTALL_CFG) $(kamctltmpdir)/kamctl.sqlbase \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.sqlbase
		rm -fr $(kamctltmpdir)/kamctl.sqlbase
		# install db setup base script
		sed -e "s#/usr/local/sbin#$(bin_target)#g" \
			-e "s#/usr/local/etc/kamailio#$(cfg_target)#g" \
			-e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbctl.base > $(kamctltmpdir)/kamdbctl.base
		$(INSTALL_CFG) $(kamctltmpdir)/kamdbctl.base \
			$(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.base
		rm -fr $(kamctltmpdir)/kamdbctl.base
		cat kamdbctl | \
		sed -e "s#/usr/local/sbin#$(bin_target)#g" | \
		sed -e "s#/usr/local/lib/kamailio#$(lib_target)#g" | \
		sed -e "s#/usr/local/etc/kamailio#$(cfg_target)#g"  >$(kamctltmpdir)/kamdbctl
		$(INSTALL_TOUCH) $(bin_prefix)/$(bin_dir)/kamdbctl
		$(INSTALL_BIN) $(kamctltmpdir)/kamdbctl $(bin_prefix)/$(bin_dir)
		rm -fr $(kamctltmpdir)/kamdbctl

install-man: $(man_prefix)/$(man_dir)/man8 $(man_prefix)/$(man_dir)/man5
		sed -e "s#/etc/$(NAME)/$(NAME)\.cfg#$(cfg_target)$(NAME).cfg#g" \
			-e "s#/usr/sbin/#$(bin_target)#g" \
			-e "s#/usr/lib/$(NAME)/modules/#$(modules_target)#g" \
			-e "s#/usr/share/doc/$(NAME)/#$(doc_target)#g" \
			< kamctl.8 > $(man_prefix)/$(man_dir)/man8/kamctl.8
		chmod 644  $(man_prefix)/$(man_dir)/man8/kamctl.8
		sed -e "s#/etc/$(NAME)/$(NAME)\.cfg#$(cfg_target)$(NAME).cfg#g" \
			-e "s#/usr/sbin/#$(bin_target)#g" \
			-e "s#/usr/lib/$(NAME)/modules/#$(modules_target)#g" \
			-e "s#/usr/share/doc/$(NAME)/#$(doc_target)#g" \
			< kamdbctl.8 > $(man_prefix)/$(man_dir)/man8/kamdbctl.8
		chmod 644  $(man_prefix)/$(man_dir)/man8/kamdbctl.8


# MYSQLON?=yes

install-modules: $(bin_prefix)/$(bin_dir)
		# install MySQL stuff
		if [ "$(MYSQLON)" = "yes" ]; then \
			mkdir -p $(modules_prefix)/$(lib_dir)/kamctl ; \
			sed -e "s#/usr/local/sbin#$(bin_target)#g" \
				< kamctl.mysql > $(kamctltmpdir)/kamctl.mysql ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamctl.mysql \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.mysql ; \
			rm -fr $(kamctltmpdir)/kamctl.mysql ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbctl.mysql > $(kamctltmpdir)/kamdbctl.mysql ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.mysql ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamdbctl.mysql $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(kamctltmpdir)/kamdbctl.mysql ; \
			mkdir -p $(data_prefix)/$(data_dir)/mysql ; \
			for FILE in $(wildcard mysql/*) ; do \
				if [ -f $$FILE ] ; then \
				$(INSTALL_TOUCH) $$FILE \
					$(data_prefix)/$(data_dir)/mysql/`basename "$$FILE"` ; \
				$(INSTALL_CFG) $$FILE \
					$(data_prefix)/$(data_dir)/mysql/`basename "$$FILE"` ; \
				fi ;\
			done ; \
		fi
		# install PostgreSQL stuff
		if [ "$(PGSQLON)" = "yes" ]; then \
			mkdir -p $(modules_prefix)/$(lib_dir)/kamctl ; \
			sed -e "s#/usr/local/sbin#$(bin_target)#g" \
				< kamctl.pgsql > $(kamctltmpdir)/kamctl.pgsql ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamctl.pgsql \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.pgsql ; \
			rm -fr $(kamctltmpdir)/kamctl.pgsql ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.pgsql > $(kamctltmpdir)/kamdbctl.pgsql ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.pgsql ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamdbctl.pgsql $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(kamctltmpdir)/kamdbctl.pgsql ; \
			mkdir -p $(data_prefix)/$(data_dir)/postgres ; \
			for FILE in $(wildcard postgres/*) ; do \
				if [ -f $$FILE ] ; then \
				$(INSTALL_TOUCH) $$FILE \
					$(data_prefix)/$(data_dir)/postgres/`basename "$$FILE"` ; \
				$(INSTALL_CFG) $$FILE \
					$(data_prefix)/$(data_dir)/postgres/`basename "$$FILE"` ; \
				fi ;\
			done ; \
		fi
		# install Oracle stuff
		if [ "$(ORACLEON)" = "yes" ]; then \
			mkdir -p $(modules_prefix)/$(lib_dir)/kamctl ; \
			sed -e "s#/usr/local/sbin#$(bin_target)#g" \
				< kamctl.oracle > $(kamctltmpdir)/kamctl.oracle ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamctl.oracle \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.oracle ; \
			rm -fr $(kamctltmpdir)/kamctl.oracle ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbctl.oracle > $(kamctltmpdir)/kamdbctl.oracle ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.oracle ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamdbctl.oracle $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(kamctltmpdir)/kamdbctl.oracle ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbfunc.oracle > $(kamctltmpdir)/kamdbfunc.oracle ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbfunc.oracle ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamdbfunc.oracle $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(kamctltmpdir)/kamdbfunc.oracle ; \
			mkdir -p $(data_prefix)/$(data_dir)/oracle ; \
			for FILE in $(wildcard oracle/*) ; do \
				if [ -f $$FILE ] ; then \
				$(INSTALL_TOUCH) $$FILE \
					$(data_prefix)/$(data_dir)/oracle/`basename "$$FILE"` ; \
				$(INSTALL_CFG) $$FILE \
					$(data_prefix)/$(data_dir)/oracle/`basename "$$FILE"` ; \
				fi ;\
			done ; \
			mkdir -p $(data_prefix)/$(data_dir)/oracle/inc ; \
			for FILE in $(wildcard oracle/inc/*) ; do \
				if [ -f $$FILE ] ; then \
				$(INSTALL_TOUCH) $$FILE \
					$(data_prefix)/$(data_dir)/oracle/inc/`basename "$$FILE"` ; \
				$(INSTALL_CFG) $$FILE \
					$(data_prefix)/$(data_dir)/oracle/inc/`basename "$$FILE"` ; \
				fi ;\
			done ; \
			mkdir -p $(data_prefix)/$(data_dir)/oracle/admin ; \
			for FILE in $(wildcard oracle/admin/*) ; do \
				if [ -f $$FILE ] ; then \
				$(INSTALL_TOUCH) $$FILE \
					$(data_prefix)/$(data_dir)/oracle/admin/`basename "$$FILE"` ; \
				$(INSTALL_CFG) $$FILE \
					$(data_prefix)/$(data_dir)/oracle/admin/`basename "$$FILE"` ; \
				fi ;\
			done ; \
			$(MAKE) -C ../db_oracle/ ; \
			$(INSTALL_BIN) ../db_oracle/kamailio_orasel $(bin_prefix)/$(bin_dir) ; \
		fi
		# install Berkeley database stuff
		if [ "$(BERKELEYDBON)" = "yes" ]; then \
			mkdir -p $(modules_prefix)/$(lib_dir)/kamctl ; \
			sed -e "s#/usr/local/share/kamailio/#$(data_target)#g" \
				< kamctl.db_berkeley > $(kamctltmpdir)/kamctl.db_berkeley ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamctl.db_berkeley \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.db_berkeley ; \
			rm -fr $(kamctltmpdir)/kamctl.db_berkeley ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.db_berkeley > $(kamctltmpdir)/kamdbctl.db_berkeley ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.db_berkeley ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamdbctl.db_berkeley $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(kamctltmpdir)/kamdbctl.db_berkeley ; \
			mkdir -p $(data_prefix)/$(data_dir)/db_berkeley/kamailio ; \
			for FILE in $(wildcard db_berkeley/kamailio/*) ; do \
				if [ -f $$FILE ] ; then \
				$(INSTALL_TOUCH) $$FILE \
					$(data_prefix)/$(data_dir)/db_berkeley/kamailio/`basename "$$FILE"` ; \
				$(INSTALL_CFG) $$FILE \
					$(data_prefix)/$(data_dir)/db_berkeley/kamailio/`basename "$$FILE"` ; \
				fi ;\
			done ; \
			$(MAKE) -C ../db_berkeley/ ; \
			$(INSTALL_BIN) ../db_berkeley/kambdb_recover $(bin_prefix)/$(bin_dir) ; \
		fi
		# install dbtext stuff
		if [ "$(DBTEXTON)" = "yes" ]; then \
			mkdir -p $(modules_prefix)/$(lib_dir)/kamctl ; \
			sed -e "s#/usr/local/share/kamailio/#$(data_target)#g" \
				< kamctl.dbtext > $(kamctltmpdir)/kamctl.dbtext ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamctl.dbtext \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.dbtext ; \
			rm -fr $(kamctltmpdir)/kamctl.dbtext ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.dbtext > $(kamctltmpdir)/kamdbctl.dbtext ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.dbtext ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamdbctl.dbtext $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(kamctltmpdir)/kamdbctl.dbtext ; \
			mkdir -p $(modules_prefix)/$(lib_dir)/kamctl/dbtextdb ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/dbtextdb/dbtextdb.py ; \
			$(INSTALL_BIN) dbtextdb/dbtextdb.py $(modules_prefix)/$(lib_dir)/kamctl/dbtextdb/ ; \
			mkdir -p $(data_prefix)/$(data_dir)/dbtext/kamailio ; \
			for FILE in $(wildcard dbtext/kamailio/*) ; do \
				if [ -f $$FILE ] ; then \
					$(INSTALL_TOUCH) $$FILE \
						$(data_prefix)/$(data_dir)/dbtext/kamailio/`basename "$$FILE"` ; \
					$(INSTALL_CFG) $$FILE \
						$(data_prefix)/$(data_dir)/dbtext/kamailio/`basename "$$FILE"` ; \
				fi ;\
			done ;\
		fi
		# install sqlite stuff
		if [ "$(SQLITEON)" = "yes" ]; then \
			mkdir -p $(modules_prefix)/$(lib_dir)/kamctl ; \
			sed -e "s#/usr/local/sbin#$(bin_target)#g" \
				< kamctl.sqlite > $(kamctltmpdir)/kamctl.sqlite ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamctl.sqlite \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.sqlite ; \
			rm -fr $(kamctltmpdir)/kamctl.sqlite ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.sqlite > $(kamctltmpdir)/kamdbctl.sqlite ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.sqlite ; \
			$(INSTALL_CFG) $(kamctltmpdir)/kamdbctl.sqlite $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(kamctltmpdir)/kamdbctl.sqlite ; \
			mkdir -p $(data_prefix)/$(data_dir)/db_sqlite ; \
			for FILE in $(wildcard db_sqlite/*) ; do \
				if [ -f $$FILE ] ; then \
				$(INSTALL_TOUCH) $$FILE \
					$(data_prefix)/$(data_dir)/db_sqlite/`basename "$$FILE"` ; \
				$(INSTALL_CFG) $$FILE \
					$(data_prefix)/$(data_dir)/db_sqlite/`basename "$$FILE"` ; \
				fi ;\
			done ; \
		fi


$(cfg_prefix)/$(cfg_dir):
		mkdir -p $(cfg_prefix)/$(cfg_dir)

$(bin_prefix)/$(bin_dir):
		mkdir -p $(bin_prefix)/$(bin_dir)

$(man_prefix)/$(man_dir)/man8:
		mkdir -p $(man_prefix)/$(man_dir)/man8

$(man_prefix)/$(man_dir)/man5:
		mkdir -p $(man_prefix)/$(man_dir)/man5
