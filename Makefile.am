## Process this file with automake to produce Makefile.in

# $Id$
SUBDIRS = src doc glade m4 po

# standard props
DATADIR=.

DISTCHECK_CONFIGURE_FLAGS=--disable-scrollkeeper

indent:
	cd src && make indent

man_MANS = etherape.1

confdir = $(sysconfdir)/etherape
conf_DATA =					\
	services

Developmentdir = $(datadir)/applications
Development_DATA =			\
	etherape.desktop

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA =				\
	etherape.png

purge:
	rm -f *~ *.bak *.tar.gz
	cd src && make purge

splint:
	cd src && make splint

DISTCLEANFILES=gnome-doc-utils.make

EXTRA_DIST = config.rpath mkinstalldirs  \
	README.bugs		\
	FAQ			\
	OVERVIEW		\
	ChangeLog		\
	etherape.spec		\
	etherape.spec.in	\
	etherape.desktop.in	\
	debian/README.debian	\
	debian/changelog	\
	debian/control		\
	debian/copyright	\
	debian/dirs		\
	debian/docs		\
	debian/menu		\
	debian/rules		\
	debian/etherape.xpm	\
	src/glade-strings	\
	$(conf_DATA)		\
	$(Development_DATA)	\
	$(pixmaps_DATA)		\
	$(man_MANS)		\
	m4			\
	gnome-doc-utils.make


full-dist :
	test `whoami` = toledo
	test `echo ${VERSION}` != `cat old.version`
	jed html/NEWS.html
	make NEWS
	make dist
	cp -f ${PACKAGE}-${VERSION}.tar.gz /usr/src/redhat/SOURCES/
	rpm -ba etherape.spec

# Since the next part fails so much, I dump it to the screen so that it can
# be copied and pasted manually
	echo -e scp ${PACKAGE}-${VERSION}.tar.gz etherape.sourceforge.net:ftp \\n\
	ssh etherape.sourceforge.net ncftpput upload.sourceforge.net /incoming/ ftp/${PACKAGE}-${VERSION}.tar.gz\\n\
	scp /usr/src/redhat/RPMS/i386/${PACKAGE}-${VERSION}-1.i386.rpm etherape.sourceforge.net:ftp\\n\
	ssh etherape.sourceforge.net ncftpput upload.sourceforge.net /incoming/ ftp/${PACKAGE}-${VERSION}-1.i386.rpm\\n\
	scp /usr/src/redhat/SRPMS/${PACKAGE}-${VERSION}-1.src.rpm etherape.sourceforge.net:ftp\\n\
	ssh etherape.sourceforge.net ncftpput upload.sourceforge.net /incoming/ ftp/${PACKAGE}-${VERSION}-1.src.rpm\\n\
	"echo ${VERSION} > old.version"

	scp ${PACKAGE}-${VERSION}.tar.gz etherape.sourceforge.net:ftp
	ssh etherape.sourceforge.net ncftpput upload.sourceforge.net /incoming/ ftp/${PACKAGE}-${VERSION}.tar.gz
	scp /usr/src/redhat/RPMS/i386/${PACKAGE}-${VERSION}-1.i386.rpm etherape.sourceforge.net:ftp
	ssh etherape.sourceforge.net ncftpput upload.sourceforge.net /incoming/ ftp/${PACKAGE}-${VERSION}-1.i386.rpm		
	scp /usr/src/redhat/SRPMS/${PACKAGE}-${VERSION}-1.src.rpm etherape.sourceforge.net:ftp
	ssh etherape.sourceforge.net ncftpput upload.sourceforge.net /incoming/ ftp/${PACKAGE}-${VERSION}-1.src.rpm

	echo ${VERSION} > old.version

dist-hook: etherape.spec
	cp etherape.spec $(distdir)

ACLOCAL_AMFLAGS = -I m4
