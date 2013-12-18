include make.conf

INCLUDE_DIR=$(CURDIR)/src/
DIRS=src $(wildcard std_blocks/* std_types/* examples)

SHELL = /bin/sh
INSTALL = /usr/bin/install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644
# include Makefile.conf

BUILDDIRS = $(DIRS:%=build-%)
INSTALLDIRS = $(DIRS:%=install-%)
CLEANDIRS = $(DIRS:%=clean-%)
TESTDIRS = $(DIRS:%=test-%)

all: $(BUILDDIRS)
$(DIRS): $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) -C $(@:build-%=%)

# # the utils need the libraries in dev built first
# build-utils: build-dev

install: $(INSTALLDIRS) all
$(INSTALLDIRS):
	$(MAKE) -C $(@:install-%=%) install

test: $(TESTDIRS) all
$(TESTDIRS):
	$(MAKE) -C $(@:test-%=%) test

clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean
	@rm -f core vgcore*
	@rm -rf doc/lua doc/manual.html

doc: lua/ubx.lua doc/manual.md
	@mkdir -p doc/lua
	@echo "generating luadoc in doc/lua"
	@luadoc -d doc/lua lua/ubx.lua
	@echo "generating doc/manual.html"
	@markdown doc/manual.md > doc/manual.html

.PHONY: subdirs $(DIRS)
.PHONY: subdirs $(BUILDDIRS)
.PHONY: subdirs $(INSTALLDIRS)
.PHONY: subdirs $(TESTDIRS)
.PHONY: subdirs $(CLEANDIRS)
.PHONY: all install clean test
.PHONY: doc
