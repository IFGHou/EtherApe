## Process this file with automake to produce Makefile.in
etherape_helpdir = $(datadir)/gnome/help/etherape/C

SGML_FILES = \
	etherape.sgml

HTML_FILES = index.html

EXTRA_DIST = \
	topic.dat etherape $(SGML_FILES)

etherape_help_DATA = ${HTML_FILES} topic.dat

#
# This is icky but insures that the doc files are generated when the sgml 
# file changes, plus it only generates em once. The distclean target 
# will take care of clobbering the generated html and the directory that
# db2html makes, while the dist-hook will make sure that the html files 
# and directory gets copied into a dist (so mere mortals w/o docbook can 
# build this) The install-data-local target copies all the bits and pieces
# where the runtime help system can find it.
#
${HTML_FILES} : $(SGML_FILES)
	if [ ! -d etherape ] ; then cd $(srcdir) && db2html etherape.sgml ; fi
	-cd $(srcdir) && cp etherape/*.html .
	if [ -d etherape.junk ] ; then cd $(srcdir) && rm -rf etherape.junk ; fi

dist-hook:
	$(mkinstalldirs) $(distdir)/etherape 
	$(mkinstalldirs) $(distdir)/etherape/stylesheet-images
	-cp $(srcdir)/etherape/*.html etherape/*.css $(distdir)/etherape
	-cp $(srcdir)/etherape/stylesheet-images/*.gif \
	 $(distdir)/etherape/stylesheet-images
	-cp $(srcdir)/*.html $(distdir)
#	-cp $(srcdir)/*.png $(distdir)

distclean-generic:
	-rm -f Makefile
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*
	-rm -f *.html
	-rm -rf etherape

install-data-local: index.html
	$(mkinstalldirs) $(DESTDIR)$(etherape_helpdir)/stylesheet-images
	-for file in $(srcdir)/etherape/*.html $(srcdir)/etherape/*.css; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(etherape_helpdir)/$$basefile; \
	done
	-for file in $(srcdir)/*.png ; do \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(etherape_helpdir); \
	done
	-for file in $(srcdir)/etherape/stylesheet-images/*.gif; do \
	  basefile=`basename $$file`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(etherape_helpdir)/stylesheet-images/$$basefile; \
	done

uninstall-local:
	echo "rm -rf $(DESTDIR)$(etherape_helpdir)"
	-rm -rf $(DESTDIR)$(etherape_helpdir)	

