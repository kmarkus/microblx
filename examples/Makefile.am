# install various examples

install-exec-hook:
	install -d $(DESTDIR)$(datadir)/microblx/examples
	cp -r systemmodels $(DESTDIR)$(datadir)/microblx/examples
	cp -r blockmodels $(DESTDIR)$(datadir)/microblx/examples
	cp -r C-examples $(DESTDIR)$(datadir)/microblx/examples
	cp -r scripts $(DESTDIR)$(datadir)/microblx/examples

uninstall-hook:
	rm -rf $(DESTDIR)$(datadir)/microblx/examples
