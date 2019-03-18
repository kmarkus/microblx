# install various examples

install-exec-hook:
	install -d $(DESTDIR)$(datadir)/$(PACKAGE)/examples
	cp -r ${top_srcdir}/examples/blockmodels $(DESTDIR)$(datadir)/$(PACKAGE)/examples/
	cp -r ${top_srcdir}/examples/C-examples $(DESTDIR)$(datadir)/$(PACKAGE)/examples/
	cp -r ${top_srcdir}/examples/scripts $(DESTDIR)$(datadir)/$(PACKAGE)/examples/
	cp -r ${top_srcdir}/examples/systemmodels $(DESTDIR)$(datadir)/$(PACKAGE)/examples/

uninstall-hook:
	rm -rf $(DESTDIR)$(datadir)/$(PACKAGE)/examples
