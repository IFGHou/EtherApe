## Process this file with automake to produce Makefile.in

SUBDIRS = intl po macros src

NEWS: html/NEWS.html
	lynx --dump html/NEWS.html > NEWS

FAQ: html/FAQ.html
	lynx --dump html/FAQ.html > FAQ

cvs : NEWS FAQ
	test `whoami` = toledo
	cvs commit
	cd ../www && make

purge:
	rm -f *~ *.bak
	cd src && make purge

EXTRA_DIST = \
	README.thanks 		\
	README.help		\
	FAQ			\
	etherape.glade		\
	html/NEWS.html		\
	html/FAQ.html		\
	debian/README.debian	\
	debian/changelog	\
	debian/control		\
	debian/copyright	\
	debian/dirs		\
	debian/docs		\
	debian/menu		\
	debian/rules		

full-dist :
	test `whoami` = toledo
	test `echo ${VERSION}` != `cat old.version`
	jed NEWS.html
	make NEWS
	make dist
	make cvs
	cvs tag `echo rel-${VERSION} | tr . - `
	cd ../www && cvs tag `echo rel-${VERSION} | tr . - `
	ncftpput download.sourceforge.net /incoming/ ${PACKAGE}-${VERSION}.tar.gz
	scp ${PACKAGE}-${VERSION}.tar.gz etherape:ftp
	echo ${VERSION} > old.version


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

