## Process this file with automake to produce Makefile.in

# $Id$

SUBDIRS = intl po macros src


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

man_MANS = etherape.8

full-cvs : cvs
	cd ../www && make

confdir = $(sysconfdir)/etherape
conf_DATA =					\
	services

gladedir   = $(datadir)/etherape/glade
glade_DATA = 				\
	etherape.glade
	
Developmentdir = $(datadir)/gnome/apps/Applications
Development_DATA =			\
	etherape.desktop

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA =				\
	etherape.png
	

purge:
	rm -f *~ *.bak *.tar.gz
	cd src && make purge

EXTRA_DIST = \
	README.thanks 		\
	README.help		\
	README.bugs		\
	FAQ			\
	OVERVIEW		\
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
	$(glade_DATA)		\
	$(Development_DATA)	\
	$(pixmaps_DATA)		\
	$(man_MANS)


full-dist :
	test `whoami` = toledo
	test `echo ${VERSION}` != `cat old.version`
	jed html/NEWS.html
	make NEWS
	make cvs
	cvs tag `echo rel-${VERSION} | tr . - `
	make dist
	cp -f ${PACKAGE}-${VERSION}.tar.gz /usr/src/redhat/SOURCES/
	rpm -ba etherape.spec
	ncftpput upload.sourceforge.net /incoming/ ${PACKAGE}-${VERSION}.tar.gz
	ncftpput upload.sourceforge.net /incoming/ /usr/src/redhat/RPMS/i386/${PACKAGE}-${VERSION}-1.i386.rpm
	ncftpput upload.sourceforge.net /incoming/ /usr/src/redhat/SRPMS/${PACKAGE}-${VERSION}-1.src.rpm

	scp ${PACKAGE}-${VERSION}.tar.gz etherape.sourceforge.net:ftp
	scp /usr/src/redhat/RPMS/i386/${PACKAGE}-${VERSION}-1.i386.rpm etherape.sourceforge.net:ftp
	
	echo ${VERSION} > old.version
#	cd ../www && make
#	cd ../www && cvs tag `echo rel-${VERSION} | tr . - `

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)@PACKAGE_PIXMAPS_DIR@; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(DESTDIR)@PACKAGE_PIXMAPS_DIR@; \
	    fi \
	  done \
	fi

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi

