# $Id$
COREPATH=../..
include $(COREPATH)/Makefile.defs
include $(COREPATH)/config.mak

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
		sed -e "s#/usr/local/etc/kamailio#$(cfg_target)#g"  >/tmp/kamctl
		$(INSTALL_TOUCH) $(bin_prefix)/$(bin_dir)/kamctl
		$(INSTALL_BIN) /tmp/kamctl $(bin_prefix)/$(bin_dir)
		rm -fr /tmp/kamctl
		sed -e "s#/usr/local/sbin#$(bin_target)#g" \
			< kamctl.base > /tmp/kamctl.base
		mkdir -p $(modules_prefix)/$(lib_dir)/kamctl 
		$(INSTALL_TOUCH) \
			$(modules_prefix)/$(lib_dir)/kamctl
		$(INSTALL_CFG) /tmp/kamctl.base \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.base
		rm -fr /tmp/kamctl.base
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.ctlbase > /tmp/kamctl.ctlbase
		$(INSTALL_CFG) /tmp/kamctl.ctlbase \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.ctlbase
		rm -fr /tmp/kamctl.ctlbase
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.fifo > /tmp/kamctl.fifo
		$(INSTALL_CFG) /tmp/kamctl.fifo \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.fifo
		rm -fr /tmp/kamctl.fifo
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.ser > /tmp/kamctl.ser
		$(INSTALL_CFG) /tmp/kamctl.ser \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.ser
		rm -fr /tmp/kamctl.ser
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.ser_mi > /tmp/kamctl.ser_mi
		$(INSTALL_CFG) /tmp/kamctl.ser_mi \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.ser_mi
		rm -fr /tmp/kamctl.ser_mi
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.unixsock > /tmp/kamctl.unixsock
		$(INSTALL_CFG) /tmp/kamctl.unixsock \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.unixsock
		rm -fr /tmp/kamctl.unixsock
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.sqlbase > /tmp/kamctl.sqlbase
		$(INSTALL_CFG) /tmp/kamctl.sqlbase \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.sqlbase
		rm -fr /tmp/kamctl.sqlbase
		# install db setup base script
		sed -e "s#/usr/local/sbin#$(bin_target)#g" \
			-e "s#/usr/local/etc/kamailio#$(cfg_target)#g" \
			-e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbctl.base > /tmp/kamdbctl.base
		$(INSTALL_CFG) /tmp/kamdbctl.base \
			$(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.base
		rm -fr /tmp/kamdbctl.base
		cat kamdbctl | \
		sed -e "s#/usr/local/sbin#$(bin_target)#g" | \
		sed -e "s#/usr/local/lib/kamailio#$(lib_target)#g" | \
		sed -e "s#/usr/local/etc/kamailio#$(cfg_target)#g"  >/tmp/kamdbctl
		$(INSTALL_TOUCH) $(bin_prefix)/$(bin_dir)/kamdbctl
		$(INSTALL_BIN) /tmp/kamdbctl $(bin_prefix)/$(bin_dir)
		rm -fr /tmp/kamdbctl

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
				< kamctl.mysql > /tmp/kamctl.mysql ; \
			$(INSTALL_CFG) /tmp/kamctl.mysql \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.mysql ; \
			rm -fr /tmp/kamctl.mysql ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbctl.mysql > /tmp/kamdbctl.mysql ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.mysql ; \
			$(INSTALL_CFG) /tmp/kamdbctl.mysql $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr /tmp/kamdbctl.mysql ; \
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
				< kamctl.pgsql > /tmp/kamctl.pgsql ; \
			$(INSTALL_CFG) /tmp/kamctl.pgsql \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.pgsql ; \
			rm -fr /tmp/kamctl.pgsql ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.pgsql > /tmp/kamdbctl.pgsql ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.pgsql ; \
			$(INSTALL_CFG) /tmp/kamdbctl.pgsql $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr /tmp/kamdbctl.pgsql ; \
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
				< kamctl.oracle > /tmp/kamctl.oracle ; \
			$(INSTALL_CFG) /tmp/kamctl.oracle \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.oracle ; \
			rm -fr /tmp/kamctl.oracle ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbctl.oracle > /tmp/kamdbctl.oracle ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.oracle ; \
			$(INSTALL_CFG) /tmp/kamdbctl.oracle $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr /tmp/kamdbctl.oracle ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbfunc.oracle > /tmp/kamdbfunc.oracle ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbfunc.oracle ; \
			$(INSTALL_CFG) /tmp/kamdbfunc.oracle $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr /tmp/kamdbfunc.oracle ; \
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
				< kamctl.db_berkeley > /tmp/kamctl.db_berkeley ; \
			$(INSTALL_CFG) /tmp/kamctl.db_berkeley \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.db_berkeley ; \
			rm -fr /tmp/kamctl.db_berkeley ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.db_berkeley > /tmp/kamdbctl.db_berkeley ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.db_berkeley ; \
			$(INSTALL_CFG) /tmp/kamdbctl.db_berkeley $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr /tmp/kamdbctl.db_berkeley ; \
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
				< kamctl.dbtext > /tmp/kamctl.dbtext ; \
			$(INSTALL_CFG) /tmp/kamctl.dbtext \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.dbtext ; \
			rm -fr /tmp/kamctl.dbtext ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.dbtext > /tmp/kamdbctl.dbtext ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.dbtext ; \
			$(INSTALL_CFG) /tmp/kamdbctl.dbtext $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr /tmp/kamdbctl.dbtext ; \
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
				< kamctl.sqlite > /tmp/kamctl.sqlite ; \
			$(INSTALL_CFG) /tmp/kamctl.sqlite \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.sqlite ; \
			rm -fr /tmp/kamctl.sqlite ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.sqlite > /tmp/kamdbctl.sqlite ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.sqlite ; \
			$(INSTALL_CFG) /tmp/kamdbctl.sqlite $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr /tmp/kamdbctl.sqlite ; \
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
