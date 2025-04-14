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
	$(MAKE) -C include all
	$(MAKE) -C src all
	$(MAKE) -C odl all
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(MAKE) -C src/Plugin all
endif

clean:
	$(MAKE) -C src clean
	$(MAKE) -C odl clean
	$(MAKE) -C doc clean
	$(MAKE) -C src/Plugin clean

install: all
	$(INSTALL) -d -m 0755 $(DEST)/$(INCLUDEDIR)/wld
	$(INSTALL) -D -p -m 0644 include/wld/*.h $(DEST)$(INCLUDEDIR)/wld/
	$(INSTALL) -d -m 0755 $(DEST)/$(INCLUDEDIR)/wld/Utils
	$(INSTALL) -D -p -m 0644 include/wld/Utils/*.h $(DEST)$(INCLUDEDIR)/wld/Utils/
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/libwld.so.$(VERSION) $(DEST)$(LIBDIR)/libwld.so.$(VERSION)
	$(INSTALL) -D -p -m 0755 scripts/wld.sh $(DEST)$(LIBDIR)/wld/wld.sh
	$(INSTALL) -D -p -m 0644 odl/wld.odl $(DEST)/etc/amx/wld/wld.odl
	$(INSTALL) -D -p -m 0644 odl/wld_definitions.odl $(DEST)/etc/amx/wld/wld_definitions.odl
	$(INSTALL) -D -p -m 0644 odl/wld_radio.odl $(DEST)/etc/amx/wld/wld_radio.odl
	$(INSTALL) -D -p -m 0644 odl/wld_ssid.odl $(DEST)/etc/amx/wld/wld_ssid.odl
	$(INSTALL) -D -p -m 0644 odl/wld_accesspoint.odl $(DEST)/etc/amx/wld/wld_accesspoint.odl
	$(INSTALL) -D -p -m 0644 odl/wld_endpoint.odl $(DEST)/etc/amx/wld/wld_endpoint.odl
	$(INSTALL) -D -p -m 0644 odl/01_device-wifi_pwhm_mapping.odl $(DEST)/etc/amx/tr181-device/extensions/01_device-wifi_pwhm_mapping.odl
ifneq ($(CONFIG_SAH_SERVICES_PWHM_DISABLE_PERSIST),y)
	$(INSTALL) -d -m 0755 $(DEST)//etc/amx/wld/wld_defaults
	$(INSTALL) -D -p -m 0644 odl/wld_defaults/* $(DEST)/etc/amx/wld/wld_defaults/
endif
ifeq ($(CONFIG_SAH_SERVICES_PWHM_DISABLE_PERSIST),y)
	$(INSTALL) -d -m 0755 $(DEST)//etc/amx/wld/wld_defaults
	$(INSTALL) -D -p -m 0644 odl/wld_defaults_empty/* $(DEST)/etc/amx/wld/wld_defaults/
endif
ifeq ($(CONFIG_SAH_WLD_ENABLE_USP_SERVER_SOCKET),y)
	$(INSTALL) -D -p -m 0644 odl/wld_usp.odl $(DEST)/etc/amx/wld/extensions/wld_usp.odl
endif
	$(INSTALL) -D -p -m 0660 acl/admin/$(COMPONENT).json $(DEST)$(ACLDIR)/admin/$(COMPONENT).json
	$(INSTALL) -D -p -m 0660 acl/admin/$(COMPONENT)-internal.json $(DEST)$(ACLDIR)/admin/$(COMPONENT)-internal.json
	$(INSTALL) -D -p -m 0644 pkgconfig/pkg-config.pc $(PKG_CONFIG_LIBDIR)/wld.pc
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/Plugin/wld.so.$(VERSION) $(DEST)$(LIBDIR)/amx/wld/wld.so.$(VERSION)
endif
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 scripts/Plugin/wld_gen.sh $(DEST)$(INITDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT)
endif
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -d -m 0755 $(DEST)$(PROCMONDIR)
	ln -sfr $(DEST)$(INITDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT) $(DEST)$(PROCMONDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT)
endif
	$(INSTALL) -d -m 0755 $(DEST)$(BINDIR)
	ln -sfr $(DEST)/usr/bin/amxrt $(DEST)$(BINDIR)/wld
	$(INSTALL) -D -p -m 0755 scripts/debug_wifi.sh $(DEST)/usr/lib/debuginfo/debug_wifi.sh
	$(INSTALL) -D -p -m 0755 scripts/debugInfo.sh $(DEST)$(LIBDIR)/amx/wld/debugInfo.sh
	ln -sfr $(DEST)$(LIBDIR)/libwld.so.$(VERSION) $(DEST)$(LIBDIR)/libwld.so.$(VMAJOR)
	ln -sfr $(DEST)$(LIBDIR)/libwld.so.$(VERSION) $(DEST)$(LIBDIR)/libwld.so
	ln -sfr $(DEST)$(LIBDIR)/amx/wld/wld.so.$(VERSION) $(DEST)$(LIBDIR)/amx/wld/wld.so.$(VMAJOR)
	ln -sfr $(DEST)$(LIBDIR)/amx/wld/wld.so.$(VERSION) $(DEST)$(LIBDIR)/amx/wld/wld.so

package: all
	$(INSTALL) -d -m 0755 $(PKGDIR)/$(INCLUDEDIR)/wld
	$(INSTALL) -D -p -m 0644 include/wld/*.h $(PKGDIR)$(INCLUDEDIR)/wld/
	$(INSTALL) -d -m 0755 $(PKGDIR)/$(INCLUDEDIR)/wld/Utils
	$(INSTALL) -D -p -m 0644 include/wld/Utils/*.h $(PKGDIR)$(INCLUDEDIR)/wld/Utils/
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/libwld.so.$(VERSION) $(PKGDIR)$(LIBDIR)/libwld.so.$(VERSION)
	$(INSTALL) -D -p -m 0755 scripts/wld.sh $(PKGDIR)$(LIBDIR)/wld/wld.sh
	$(INSTALL) -D -p -m 0644 odl/wld.odl $(PKGDIR)/etc/amx/wld/wld.odl
	$(INSTALL) -D -p -m 0644 odl/wld_definitions.odl $(PKGDIR)/etc/amx/wld/wld_definitions.odl
	$(INSTALL) -D -p -m 0644 odl/wld_radio.odl $(PKGDIR)/etc/amx/wld/wld_radio.odl
	$(INSTALL) -D -p -m 0644 odl/wld_ssid.odl $(PKGDIR)/etc/amx/wld/wld_ssid.odl
	$(INSTALL) -D -p -m 0644 odl/wld_accesspoint.odl $(PKGDIR)/etc/amx/wld/wld_accesspoint.odl
	$(INSTALL) -D -p -m 0644 odl/wld_endpoint.odl $(PKGDIR)/etc/amx/wld/wld_endpoint.odl
	$(INSTALL) -D -p -m 0644 odl/01_device-wifi_pwhm_mapping.odl $(PKGDIR)/etc/amx/tr181-device/extensions/01_device-wifi_pwhm_mapping.odl
ifneq ($(CONFIG_SAH_SERVICES_PWHM_DISABLE_PERSIST),y)
	$(INSTALL) -d -m 0755 $(PKGDIR)//etc/amx/wld/wld_defaults
	$(INSTALL) -D -p -m 0644 odl/wld_defaults/* $(PKGDIR)/etc/amx/wld/wld_defaults/
endif
ifeq ($(CONFIG_SAH_SERVICES_PWHM_DISABLE_PERSIST),y)
	$(INSTALL) -d -m 0755 $(PKGDIR)//etc/amx/wld/wld_defaults
	$(INSTALL) -D -p -m 0644 odl/wld_defaults_empty/* $(PKGDIR)/etc/amx/wld/wld_defaults/
endif
ifeq ($(CONFIG_SAH_WLD_ENABLE_USP_SERVER_SOCKET),y)
	$(INSTALL) -D -p -m 0644 odl/wld_usp.odl $(PKGDIR)/etc/amx/wld/extensions/wld_usp.odl
endif
	$(INSTALL) -D -p -m 0660 acl/admin/$(COMPONENT).json $(PKGDIR)$(ACLDIR)/admin/$(COMPONENT).json
	$(INSTALL) -D -p -m 0660 acl/admin/$(COMPONENT)-internal.json $(PKGDIR)$(ACLDIR)/admin/$(COMPONENT)-internal.json
	$(INSTALL) -D -p -m 0644 pkgconfig/pkg-config.pc $(PKGDIR)$(PKG_CONFIG_LIBDIR)/wld.pc
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/Plugin/wld.so.$(VERSION) $(PKGDIR)$(LIBDIR)/amx/wld/wld.so.$(VERSION)
endif
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 scripts/Plugin/wld_gen.sh $(PKGDIR)$(INITDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT)
endif
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -d -m 0755 $(PKGDIR)$(PROCMONDIR)
	rm -f $(PKGDIR)$(PROCMONDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT)
	ln -sfr $(PKGDIR)$(INITDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT) $(PKGDIR)$(PROCMONDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT)
endif
	$(INSTALL) -d -m 0755 $(PKGDIR)$(BINDIR)
	rm -f $(PKGDIR)$(BINDIR)/wld
	ln -sfr $(PKGDIR)/usr/bin/amxrt $(PKGDIR)$(BINDIR)/wld
	$(INSTALL) -D -p -m 0755 scripts/debug_wifi.sh $(PKGDIR)/usr/lib/debuginfo/debug_wifi.sh
	$(INSTALL) -D -p -m 0755 scripts/debugInfo.sh $(PKGDIR)$(LIBDIR)/amx/wld/debugInfo.sh
	cd $(PKGDIR) && $(TAR) -czvf ../$(COMPONENT)-$(VERSION).tar.gz .
	cp $(PKGDIR)../$(COMPONENT)-$(VERSION).tar.gz .
	make -C packages

changelog:
	$(call create_changelog)

doc:
	$(MAKE) -C doc doc

	$(MAKE) -C odl all

	$(eval ODLFILES += odl/wld.odl)
	$(eval ODLFILES += odl/wld_definitions.odl)
	$(eval ODLFILES += odl/wld_radio.odl)
	$(eval ODLFILES += odl/wld_ssid.odl)
	$(eval ODLFILES += odl/wld_accesspoint.odl)
	$(eval ODLFILES += odl/wld_endpoint.odl)
	$(eval ODLFILES += odl/01_device-wifi_pwhm_mapping.odl)
ifeq ($(CONFIG_SAH_WLD_ENABLE_USP_SERVER_SOCKET),y)
	$(eval ODLFILES += odl/wld_usp.odl)
endif

	mkdir -p output/xml
	mkdir -p output/html
	mkdir -p output/confluence
	amxo-cg -Gxml,output/xml/$(COMPONENT).xml $(or $(ODLFILES), "")
	amxo-xml-to -x html -o output-dir=output/html -o title="$(COMPONENT)" -o version=$(VERSION) -o sub-title="Datamodel reference" output/xml/*.xml
	amxo-xml-to -x confluence -o output-dir=output/confluence -o title="$(COMPONENT)" -o version=$(VERSION) -o sub-title="Datamodel reference" output/xml/*.xml

test:
	$(MAKE) -C test run
	$(MAKE) -C test coverage

.PHONY: all clean changelog install package doc test