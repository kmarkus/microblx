# install various examples

install-exec-hook:
	install -d $(DESTDIR)$(datadir)/microblx/examples
	cp -r ${top_srcdir}/examples/blockmodels $(DESTDIR)$(datadir)/microblx/examples/
	cp -r ${top_srcdir}/examples/C-examples $(DESTDIR)$(datadir)/microblx/examples/
	cp -r ${top_srcdir}/examples/scripts $(DESTDIR)$(datadir)/microblx/examples/
	cp -r ${top_srcdir}/examples/systemmodels $(DESTDIR)$(datadir)/microblx/examples/

uninstall-hook:
	rm -rf $(DESTDIR)$(datadir)/microblx/examples
