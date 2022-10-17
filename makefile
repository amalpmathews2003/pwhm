include makefile.inc

NOW = $(shell date +"%Y-%m-%d(%H:%M:%S %z)")

# Extra destination directories
PKGDIR = ./output/$(MACHINE)/pkg/

define create_changelog
	@$(ECHO) "Update changelog"
	mv CHANGELOG.md CHANGELOG.md.bak
	head -n 9 CHANGELOG.md.bak > CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	$(ECHO) "## Release $(VERSION) - $(NOW)" >> CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	$(GIT) log --pretty=format:"- %s" $$($(GIT) describe --tags | grep -v "merge" | cut -d'-' -f1)..HEAD  >> CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	tail -n +10 CHANGELOG.md.bak >> CHANGELOG.md
	rm CHANGELOG.md.bak
endef

# targets
all:
	$(MAKE) -C src all
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(MAKE) -C src/Plugin all
endif

clean:
	$(MAKE) -C src clean
	$(MAKE) -C doc clean
	$(MAKE) -C src/Plugin clean

install: all
	$(INSTALL) -d -m 0755 $(DEST)/$(INCLUDEDIR)/wld
	$(INSTALL) -D -p -m 0644 include/wld/*.h $(DEST)$(INCLUDEDIR)/wld/
	$(INSTALL) -d -m 0755 $(DEST)/$(INCLUDEDIR)/wld/Utils
	$(INSTALL) -D -p -m 0644 include/wld/Utils/*.h $(DEST)$(INCLUDEDIR)/wld/Utils/
	$(INSTALL) -D -p -m 0755 src/libwld.so $(DEST)$(LIBDIR)/libwld.so
	$(INSTALL) -D -p -m 0755 scripts/wld.sh $(DEST)$(LIBDIR)/wld/wld.sh
	$(INSTALL) -D -p -m 0644 odl/wld.odl $(DEST)/etc/amx/wld/wld.odl
	$(INSTALL) -D -p -m 0644 odl/wld_definitions.odl $(DEST)/etc/amx/wld/wld_definitions.odl
	$(INSTALL) -D -p -m 0644 pkgconfig/pkg-config.pc $(DEST)$(PKG_CONFIG_LIBDIR)/wld.pc
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 src/Plugin/wld.so $(DEST)$(LIBDIR)/amx/wld/wld.so
endif
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 scripts/Plugin/wld_gen.sh $(DEST)$(INITDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT)
endif
	$(INSTALL) -d -m 0755 $(DEST)$(BINDIR)
	$(INSTALL) -D -p -m 0755 scripts/debug_wifi.sh $(DEST)/usr/lib/debuginfo/debug_wifi.sh
	ln -sfr $(DEST)/usr/bin/amxrt $(DEST)$(BINDIR)/wld

package: all
	$(INSTALL) -d -m 0755 $(PKGDIR)/$(INCLUDEDIR)/wld
	$(INSTALL) -D -p -m 0644 include/wld/*.h $(PKGDIR)$(INCLUDEDIR)/wld/
	$(INSTALL) -d -m 0755 $(PKGDIR)/$(INCLUDEDIR)/wld/Utils
	$(INSTALL) -D -p -m 0644 include/wld/Utils/*.h $(PKGDIR)$(INCLUDEDIR)/wld/Utils/
	$(INSTALL) -D -p -m 0755 src/libwld.so $(PKGDIR)$(LIBDIR)/libwld.so
	$(INSTALL) -D -p -m 0755 scripts/wld.sh $(PKGDIR)$(LIBDIR)/wld/wld.sh
	$(INSTALL) -D -p -m 0644 odl/wld.odl $(PKGDIR)/etc/amx/wld/wld.odl
	$(INSTALL) -D -p -m 0644 odl/wld_definitions.odl $(PKGDIR)/etc/amx/wld/wld_definitions.odl
	$(INSTALL) -D -p -m 0644 pkgconfig/pkg-config.pc $(PKGDIR)$(PKG_CONFIG_LIBDIR)/wld.pc
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 src/Plugin/wld.so $(PKGDIR)$(LIBDIR)/amx/wld/wld.so
endif
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 scripts/Plugin/wld_gen.sh $(PKGDIR)$(INITDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT)
endif
	$(INSTALL) -d -m 0755 $(PKGDIR)$(BINDIR)
	$(INSTALL) -D -p -m 0755 scripts/debug_wifi.sh $(PKGDIR)/usr/lib/debuginfo/debug_wifi.sh
	cd $(PKGDIR) && $(TAR) -czvf ../$(COMPONENT)-$(VERSION).tar.gz .
	cp $(PKGDIR)../$(COMPONENT)-$(VERSION).tar.gz .
	make -C packages

changelog:
	$(call create_changelog)

doc:
	$(MAKE) -C doc doc

	$(eval ODLFILES += odl/wld.odl)
	$(eval ODLFILES += odl/wld_definitions.odl)

	mkdir -p output/xml
	mkdir -p output/html
	mkdir -p output/confluence
	amxo-cg -Gxml,output/xml/$(COMPONENT).xml $(or $(ODLFILES), "")
	amxo-xml-to -x html -o output-dir=output/html -o title="$(COMPONENT)" -o version=$(VERSION) -o sub-title="Datamodel reference" output/xml/*.xml
	amxo-xml-to -x confluence -o output-dir=output/confluence -o title="$(COMPONENT)" -o version=$(VERSION) -o sub-title="Datamodel reference" output/xml/*.xml

.PHONY: all clean changelog install package doc