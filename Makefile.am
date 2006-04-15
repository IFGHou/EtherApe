## Process this file with automake to produce Makefile.in

# $Id$
SUBDIRS = src doc glade m4 po

# standard props
DATADIR=.

# ignore bogus distcheck errors caused by scrollkeeper lazyness
distuninstallcheck_listfiles = find . -type f -print | grep -v '^\./var/scrollkeeper'

indent:
	cd src && make indent

NEWS: html/NEWS.html
	lynx --dump html/NEWS.html > NEWS

FAQ: html/FAQ.html
	lynx --dump html/FAQ.html > FAQ

README.bugs: html/bug-reporting.html
	lynx --dump html/bug-reporting.html > README.bugs

cvs : NEWS FAQ README.bugs indent
	test `whoami` = toledo
	cvs commit
	touch src/.indent

man_MANS = etherape.1

full-cvs : cvs
	cd ../www && make

confdir = $(sysconfdir)/etherape
conf_DATA =					\
	services

Developmentdir = $(datadir)/gnome/apps/Applications
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

EXTRA_DIST = config.rpath mkinstalldirs  \
	README.thanks 		\
	README.help		\
	README.bugs		\
	FAQ			\
	OVERVIEW		\
	ChangeLog		\
	etherape.spec		\
	etherape.spec.in	\
	html/NEWS.html		\
	html/FAQ.html		\
	html/bug-reporting.html \
	debian/README.debian	\
	debian/changelog	\
	debian/control		\
	debian/copyright	\
	debian/dirs		\
	debian/docs		\
	debian/menu		\
	debian/rules		\
	src/glade-strings	\
	$(conf_DATA)		\
	$(Development_DATA)	\
	$(pixmaps_DATA)		\
	$(man_MANS)


full-dist :
	test `whoami` = toledo
	test `echo ${VERSION}` != `cat old.version`
	jed html/NEWS.html
	make NEWS
	make cvs
	cvs2cl --accum
	cvs tag `echo rel-${VERSION} | tr . - `
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
	
#	cd ../www && make
#	cd ../www && cvs tag `echo rel-${VERSION} | tr . - `


dist-hook: etherape.spec
	cp etherape.spec $(distdir)

ACLOCAL_AMFLAGS = -I m4
