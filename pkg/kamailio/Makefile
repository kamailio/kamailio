# Makefile to help building packages

ifneq ("$(wildcard ../../src/config.mak)","")
include ../../src/Makefile.defs
else ifneq ($(MAKECMDGOALS),cfg)
$(error Please start "make cfg" first)
endif

RPMBUILD_TOP := /tmp/kamailio_rpmbuild
RPM_VER = ${shell echo "$(RELEASE)" | sed -e "s/-dev[0-9]//" -e "s/-rc[0-9]//"}
RPM_REL = ${shell echo "$(RELEASE)." | grep -o -P "dev[0-9]\." }
DIST_ARCHIVE = ${shell echo "kamailio-$(RPM_VER)_src.tar.gz"}
APK_VERSION = ${shell echo "$(RELEASE)" | grep -o "^[0-9.]*"}
RPMBUILD_OPT= --define "_topdir $(RPMBUILD_TOP)"
GIT_TAG = ${shell git rev-parse HEAD 2> /dev/null}

# generate config.mak and modules.lst
.PHONY: cfg
cfg:
	$(MAKE) -C ../../src cfg

# generate the tarball with source tree
.PHONY: tar
tar:
	rm -f ../../kamailio*_src.tar.gz
	$(MAKE) -C ../.. tar

# build alpine apk packages
.PHONY: apk
apk:
	echo Building alpine packages using commit ${GIT_TAG}
	sed -e "s/^_gitcommit=.*/_gitcommit=${GIT_TAG}/" \
	    -e "s/^pkgver=.*/pkgver=${APK_VERSION}/" \
	    -i alpine/APKBUILD
	cd alpine && abuild snapshot
	@echo \"abuild\" cannot be start from Makefile please start manualy \"cd alpine \&\& abuild -r\"

# build source rpm package
.PHONY: src.rpm
src.rpm: tar
	mkdir -p ${RPMBUILD_TOP}/SOURCES
	mv ../../kamailio-$(RELEASE)_src.tar.gz ${RPMBUILD_TOP}/SOURCES/${DIST_ARCHIVE}
	sed -e 's/%define ver .*/%define ver ${RPM_VER}/' \
            -e 's/%define rel .*/%define rel ${RPM_REL}0%{dist}/' \
            -e 's/setup -n %{name}-%{ver}/setup -n kamailio-$(RELEASE)/' \
            -i obs/kamailio.spec
	rpmbuild ${RPMBUILD_OPT} -bs obs/kamailio.spec
	mv ${RPMBUILD_TOP}/SRPMS/kamailio-${RPM_VER}*.src.rpm ../..
	rm -Rf ${RPMBUILD_TOP}

# build rpm packages
.PHONY: rpm
rpm: src.rpm
	mock ../../kamailio-$(RELEASE).*.src.rpm

# build debian packages
.PHONY: deb
deb:
	$(MAKE) -C ../.. deb
