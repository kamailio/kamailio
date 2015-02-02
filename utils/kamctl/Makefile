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
		sed -e "s#/usr/local/etc/kamailio#$(cfg_target)#g"  >$(basedir)/kamctl
		$(INSTALL_TOUCH) $(bin_prefix)/$(bin_dir)/kamctl
		$(INSTALL_BIN) $(basedir)/kamctl $(bin_prefix)/$(bin_dir)
		rm -fr $(basedir)/kamctl
		sed -e "s#/usr/local/sbin#$(bin_target)#g" \
			< kamctl.base > $(basedir)/kamctl.base
		mkdir -p $(modules_prefix)/$(lib_dir)/kamctl 
		$(INSTALL_TOUCH) \
			$(modules_prefix)/$(lib_dir)/kamctl
		$(INSTALL_CFG) $(basedir)/kamctl.base \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.base
		rm -fr $(basedir)/kamctl.base
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.ctlbase > $(basedir)/kamctl.ctlbase
		$(INSTALL_CFG) $(basedir)/kamctl.ctlbase \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.ctlbase
		rm -fr $(basedir)/kamctl.ctlbase
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.fifo > $(basedir)/kamctl.fifo
		$(INSTALL_CFG) $(basedir)/kamctl.fifo \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.fifo
		rm -fr $(basedir)/kamctl.fifo
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.ser > $(basedir)/kamctl.ser
		$(INSTALL_CFG) $(basedir)/kamctl.ser \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.ser
		rm -fr $(basedir)/kamctl.ser
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.ser_mi > $(basedir)/kamctl.ser_mi
		$(INSTALL_CFG) $(basedir)/kamctl.ser_mi \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.ser_mi
		rm -fr $(basedir)/kamctl.ser_mi
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.unixsock > $(basedir)/kamctl.unixsock
		$(INSTALL_CFG) $(basedir)/kamctl.unixsock \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.unixsock
		rm -fr $(basedir)/kamctl.unixsock
		sed -e "s#/usr/local#$(bin_target)#g" \
			< kamctl.sqlbase > $(basedir)/kamctl.sqlbase
		$(INSTALL_CFG) $(basedir)/kamctl.sqlbase \
			$(modules_prefix)/$(lib_dir)/kamctl/kamctl.sqlbase
		rm -fr $(basedir)/kamctl.sqlbase
		# install db setup base script
		sed -e "s#/usr/local/sbin#$(bin_target)#g" \
			-e "s#/usr/local/etc/kamailio#$(cfg_target)#g" \
			-e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbctl.base > $(basedir)/kamdbctl.base
		$(INSTALL_CFG) $(basedir)/kamdbctl.base \
			$(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.base
		rm -fr $(basedir)/kamdbctl.base
		cat kamdbctl | \
		sed -e "s#/usr/local/sbin#$(bin_target)#g" | \
		sed -e "s#/usr/local/lib/kamailio#$(lib_target)#g" | \
		sed -e "s#/usr/local/etc/kamailio#$(cfg_target)#g"  >$(basedir)/kamdbctl
		$(INSTALL_TOUCH) $(bin_prefix)/$(bin_dir)/kamdbctl
		$(INSTALL_BIN) $(basedir)/kamdbctl $(bin_prefix)/$(bin_dir)
		rm -fr $(basedir)/kamdbctl

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
				< kamctl.mysql > $(basedir)/kamctl.mysql ; \
			$(INSTALL_CFG) $(basedir)/kamctl.mysql \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.mysql ; \
			rm -fr $(basedir)/kamctl.mysql ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbctl.mysql > $(basedir)/kamdbctl.mysql ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.mysql ; \
			$(INSTALL_CFG) $(basedir)/kamdbctl.mysql $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(basedir)/kamdbctl.mysql ; \
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
				< kamctl.pgsql > $(basedir)/kamctl.pgsql ; \
			$(INSTALL_CFG) $(basedir)/kamctl.pgsql \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.pgsql ; \
			rm -fr $(basedir)/kamctl.pgsql ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.pgsql > $(basedir)/kamdbctl.pgsql ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.pgsql ; \
			$(INSTALL_CFG) $(basedir)/kamdbctl.pgsql $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(basedir)/kamdbctl.pgsql ; \
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
				< kamctl.oracle > $(basedir)/kamctl.oracle ; \
			$(INSTALL_CFG) $(basedir)/kamctl.oracle \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.oracle ; \
			rm -fr $(basedir)/kamctl.oracle ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbctl.oracle > $(basedir)/kamdbctl.oracle ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.oracle ; \
			$(INSTALL_CFG) $(basedir)/kamdbctl.oracle $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(basedir)/kamdbctl.oracle ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
			< kamdbfunc.oracle > $(basedir)/kamdbfunc.oracle ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbfunc.oracle ; \
			$(INSTALL_CFG) $(basedir)/kamdbfunc.oracle $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(basedir)/kamdbfunc.oracle ; \
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
				< kamctl.db_berkeley > $(basedir)/kamctl.db_berkeley ; \
			$(INSTALL_CFG) $(basedir)/kamctl.db_berkeley \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.db_berkeley ; \
			rm -fr $(basedir)/kamctl.db_berkeley ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.db_berkeley > $(basedir)/kamdbctl.db_berkeley ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.db_berkeley ; \
			$(INSTALL_CFG) $(basedir)/kamdbctl.db_berkeley $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(basedir)/kamdbctl.db_berkeley ; \
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
				< kamctl.dbtext > $(basedir)/kamctl.dbtext ; \
			$(INSTALL_CFG) $(basedir)/kamctl.dbtext \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.dbtext ; \
			rm -fr $(basedir)/kamctl.dbtext ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.dbtext > $(basedir)/kamdbctl.dbtext ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.dbtext ; \
			$(INSTALL_CFG) $(basedir)/kamdbctl.dbtext $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(basedir)/kamdbctl.dbtext ; \
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
				< kamctl.sqlite > $(basedir)/kamctl.sqlite ; \
			$(INSTALL_CFG) $(basedir)/kamctl.sqlite \
				$(modules_prefix)/$(lib_dir)/kamctl/kamctl.sqlite ; \
			rm -fr $(basedir)/kamctl.sqlite ; \
			sed -e "s#/usr/local/share/kamailio#$(data_target)#g" \
				< kamdbctl.sqlite > $(basedir)/kamdbctl.sqlite ; \
			$(INSTALL_TOUCH) $(modules_prefix)/$(lib_dir)/kamctl/kamdbctl.sqlite ; \
			$(INSTALL_CFG) $(basedir)/kamdbctl.sqlite $(modules_prefix)/$(lib_dir)/kamctl/ ; \
			rm -fr $(basedir)/kamdbctl.sqlite ; \
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
