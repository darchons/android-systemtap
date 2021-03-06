# Makefile.am --- automake input file for systemtap tapset reference manual
## process this file with automake to produce Makefile.in

DOC_INSTALL_DIR = $(DESTDIR)$(datadir)/doc/systemtap
MAN_INSTALL_DIR = $(DESTDIR)$(mandir)/man3
HTML_INSTALL_DIR = $(DESTDIR)$(datadir)/doc/systemtap/tapsets


###
# The build process is as follows (targets):
#              (xmldocs) [by docproc]
# file.tmpl --> file.xml +--> file.ps   (psdocs)   [by xmlto]
#                        +--> file.pdf  (pdfdocs)  [by xmlto]
#                        +--> DIR=file  (htmldocs) [by xmlto]
#                        +--> man/      (mandocs)  [by xmlto]

if BUILD_PDFREFDOCS
PDFDOCS = tapsets.pdf
endif

if XMLTO_STRINGPARAM
XMLTOMANPARAMS=--stringparam man.authors.section.enabled=0 --stringparam man.copyright.section.enabled=0
XMLTOHTMLPARAMS=--stringparam use.id.as.filename=1
endif

if BUILD_REFDOCS
noinst_PROGRAMS = docproc
SRCTREE=$(abs_top_srcdir)/
DOCPROC=$(abs_builddir)/docproc

all: $(PDFDOCS) stamp-htmldocs stamp-mandocs
tapsets.xml: docproc $(shell find $(SRCTREE)/tapset -name '*.stp')
	SRCTREE=$(SRCTREE) $(DOCPROC) doc $(abs_srcdir)/tapsets.tmpl > tapsets.xml.new
	if test -s tapsets.xml && cmp tapsets.xml.new tapsets.xml >/dev/null ; then \
		echo tapsets.xml unchanged; \
		rm tapsets.xml.new; \
	else \
		mv tapsets.xml.new tapsets.xml; \
	fi

stamp-htmldocs: tapsets.xml
	xmlto $(XMLTOHTMLPARAMS) html -o tapsets tapsets.xml >/dev/null 2>&1
	touch stamp-htmldocs

# bump up the allocated space so "xmlto pdf" works
tapsets.pdf: tapsets.xml
	env pool_size=3000000 hash_extra=3000000 xmlto $(XMLTOPDFPARAMS) pdf tapsets.xml >/dev/null 2>&1

stamp-mandocs: tapsets.xml
	xmlto $(XMLTOMANPARAMS) man -o man3 tapsets.xml >/dev/null 2>&1
	touch stamp-mandocs

#FIXME need to figure out where to install things appropriately
#installmandocs: mandocs
install-data-hook:
if BUILD_PDFREFDOCS
	$(MKDIR_P)  $(DOC_INSTALL_DIR)
	$(INSTALL_DATA) tapsets.pdf $(DOC_INSTALL_DIR)
endif
	$(MKDIR_P)  $(MAN_INSTALL_DIR)
	$(INSTALL_DATA) man3/* $(MAN_INSTALL_DIR)
	$(MKDIR_P)  $(HTML_INSTALL_DIR)
	$(INSTALL_DATA) tapsets/* $(HTML_INSTALL_DIR)
endif

CLEANFILES=tapsets.xml stamp-* $(PDFDOCS)
clean-local:
	rm -rf man3 tapsets
