-include $(CONFIGDIR)/components.config

export INSTALL ?= install
export PKG_CONFIG_LIBDIR ?= /usr/lib/pkgconfig
export BINDIR ?= /usr/bin
export LIBDIR ?= /usr/lib
export SLIBDIR ?= /usr/lib
export LUALIBDIR ?= /usr/lib/lua
export INCLUDEDIR ?= /usr/include
export INITDIR ?= /etc/init.d
export ACLDIR ?= /etc/acl
export DOCDIR ?= $(D)/usr/share/doc/pwhm
export PROCMONDIR ?= /usr/lib/processmonitor/scripts
export RESETDIR ?= /etc/reset
export MACHINE ?= $(shell $(CC) -dumpmachine)

export COMPONENT = pwhm
export WIFIROOT = WiFiDRV

compile:
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

install:
	$(INSTALL) -d -m 0755 $(D)/$(INCLUDEDIR)/wld
	$(INSTALL) -D -p -m 0644 include/wld/*.h $(D)$(INCLUDEDIR)/wld/
	$(INSTALL) -d -m 0755 $(D)/$(INCLUDEDIR)/wld/Utils
	$(INSTALL) -D -p -m 0644 include/wld/Utils/*.h $(D)$(INCLUDEDIR)/wld/Utils/
	$(INSTALL) -D -p -m 0755 src/libwld.so $(D)$(LIBDIR)/amx/wld/libwld.so.$(PV)
	ln -sfr $(D)$(LIBDIR)/amx/wld/libwld.so.$(PV) $(D)$(LIBDIR)/amx/wld/libwld.so
	$(INSTALL) -D -p -m 0755 scripts/wld.sh $(D)$(LIBDIR)/wld/wld.sh
	$(INSTALL) -D -p -m 0644 odl/wld.odl $(D)/etc/amx/wld/wld.odl
	$(INSTALL) -D -p -m 0644 odl/wld_definitions.odl $(D)/etc/amx/wld/wld_definitions.odl
	$(INSTALL) -D -p -m 0644 pkgconfig/pkg-config.pc $(PKG_CONFIG_LIBDIR)/wld.pc
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 src/Plugin/wld.so $(D)$(LIBDIR)/amx/wld/wld.so
endif
ifneq ($(CONFIG_SAH_WLD_INIT_LEGACY),y)
	$(INSTALL) -D -p -m 0755 scripts/Plugin/wld_gen.sh $(D)$(INITDIR)/$(CONFIG_SAH_WLD_INIT_SCRIPT)
endif
	$(INSTALL) -d $(D)$(BINDIR)
	ln -sfr $(D)/usr/bin/amxrt $(D)$(BINDIR)/wld

.PHONY: compile clean install

