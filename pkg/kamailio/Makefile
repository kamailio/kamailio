# Makefile to help building packages

include ../../src/Makefile.defs

RPMBUILD_TOP := /tmp/rpmbuild
DIST_ARCHIVE = ${shell echo "kamailio-$(RELEASE)_src.tar.gz" | sed -e "s/-dev[0-9]//" }
RPMBUILD_OPT= --define "_topdir $(RPMBUILD_TOP)"

# generate the tarball with source tree
.PHONY: tar
tar:
	rm -f ../../kamailio*_src.tar.gz
	$(MAKE) -C ../.. tar

# build source rpm package
.PHONY: src.rpm
src.rpm: tar
	mkdir -p ${RPMBUILD_TOP}/SOURCES
	mv ../../kamailio-$(RELEASE)_src.tar.gz ${RPMBUILD_TOP}/SOURCES/${DIST_ARCHIVE}
	sed -i -e 's/setup -n %{name}-%{ver}/setup -n kamailio-$(RELEASE)/' obs/kamailio.spec
	rpmbuild ${RPMBUILD_OPT} -bs obs/kamailio.spec
	mv ${RPMBUILD_TOP}/SRPMS/kamailio-$(RELEASE).*.src.rpm ../..
	rm -Rf ${RPMBUILD_TOP}

# build rpm packages
.PHONY: rpm
rpm: src.rpm
	mock ../../kamailio-$(RELEASE).*.src.rpm

# build debian packages
.PHONY: deb
deb:
	$(MAKE) -C ../.. deb
