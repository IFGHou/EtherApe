## Process this file with automake to produce Makefile.in

# $Id$

SUBDIRS = intl po macros src

gladedir   = $(datadir)/etherape/glade

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

glade_DATA = 				\
	etherape.glade

purge:
	rm -f *~ *.bak
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
	src/maketcpudp.sh	\
	src/glade-strings	\
	$(glade_DATA)		\
	$(man_MANS)


full-dist :
	test `whoami` = toledo
	test `echo ${VERSION}` != `cat old.version`
	jed html/NEWS.html
	make NEWS
	make cvs
	cvs tag `echo rel-${VERSION} | tr . - `
	make dist
	ncftpput download.sourceforge.net /incoming/ ${PACKAGE}-${VERSION}.tar.gz
	scp ${PACKAGE}-${VERSION}.tar.gz etherape:ftp
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

