## Process this file with automake to produce Makefile.in

SUBDIRS = intl po macros src

NEWS: NEWS.html
	lynx --dump NEWS.html > NEWS

cvs : NEWS
	test `whoami` = toledo
	cvs commit
	cd ../www && make

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

