include ../makefile.inc


# build destination directories
OBJDIR = ../output/$(MACHINE)

# TARGETS
TARGET_SO = $(OBJDIR)/$(COMPONENT).so

# directories
# source directories
SRCDIR = .
INCDIR_PRIV = ../include_priv
INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include/libnl3)
STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib)

CFLAGS += -std=gnu99 -fPIC -g -Wall -Werror -Wextra  \
		-I. \
		-I../include_priv/ChannelManagement/ \
		-I../include_priv/VendorModule/ \
		-I../include/wld/ $(addprefix -I ,$(INCDIRS)) \
		-I../include/ $(addprefix -I ,$(INCDIRS)) \
		-I../include_priv/nl80211/ \
		-DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=500
CFLAGS += -Wformat -Wformat-security -Wimplicit-function-declaration -Wl,--no-undefined -Wno-unused-variable
LDFLAGS += $(STAGING_LIBDIR) -lsahtrace -lswlc -lswla -lcrypto -lssl -lnl-3 -lnl-genl-3 -lrt -lm -lamxo -lamxb -lamxc -lamxm -lamxp -lamxd -ldl

ifeq ($(COVERAGE),y)
CFLAGS +=-fprofile-arcs -ftest-coverage
LDFLAGS += -fprofile-arcs -ftest-coverage
endif

OBJS-libwld.so = wld.o wld_accesspoint.o wld_endpoint.o wld_radio.o \
		wld_ssid.o wld_util.o wld_wps.o wld_assocdev.o wld_statsmon.o \
		wld_functionTable.o \
		wld_radioOperatingStandards.o \
		wld_sensing.o
		
OBJS-libwld.so += 	ChannelManagement/wld_channel.o \
					ChannelManagement/wld_chanmgt.o \
					ChannelManagement/wld_bgdfs.o \
					ChannelManagement/wld_channel_info.o \
					RadMgt/wld_rad_stamon.o\
					RadMgt/wld_rad_config.o\
					RadMgt/wld_rad_caps.o\
					RadMgt/wld_rad_ifMgt.o\
					RadMgt/wld_rad_fsm.o\
					RadMgt/wld_rad_evtHandler.o\
					RadMgt/wld_rad_scan.o\
					RadMgt/wld_rad_delayMgr.o\
					RadMgt/wld_rad_macCfg.o\
					ApMgt/wld_ap_neighbours.o\
					ApMgt/wld_ap_macfilter.o\
					ApMgt/wld_ap_bss_transfer.o\
					ApMgt/wld_apMgmt.o\
					ApMgt/wld_ap_rrm.o \
					ApMgt/wld_ap_rssiMonitor.o \
					ApMgt/wld_ap_security.o \
					EpMgt/wld_tinyRoam.o \
					EpMgt/wld_tinyRoam_bus.o \
					EpMgt/wld_epProfile.o \
					EpMgt/wld_epMgmt.o \
					Utils/wld_eventing.o \
					Utils/wld_monitor.o \
					Utils/wld_autoCommitMgr.o \
					Utils/wld_autoNeighAdd.o \
					Utils/wld_daemon.o \
					Utils/wld_linuxIfUtils.o \
					Utils/wld_linuxWExtUtils.o \
					Utils/wld_linuxIfStats.o \
					Utils/wld_extMod.o \
					VendorModule/wld_vendorModule_mgr.o \
					VendorModule/wld_vendorModule.o \
					nl80211/wld_nl80211_core.o \
					nl80211/wld_nl80211_attr.o \
					nl80211/wld_nl80211_api.o \
					nl80211/wld_nl80211_events.o \
					nl80211/wld_nl80211_utils.o \
					nl80211/wld_nl80211_parser.o \
					nl80211/wld_nl80211_debug.o \
					nl80211/wld_rad_nl80211.o \
					nl80211/wld_ap_nl80211.o \
					nl80211/wld_ep_nl80211.o \
					nl80211/wld_wpaCtrlInterface.o \
					nl80211/wld_wpaCtrlMngr.o \
					nl80211/wld_wpaCtrl_events.o \
					nl80211/wld_secDmn.o \
					nl80211/wld_hostapd_cfgManager.o \
					nl80211/wld_hostapd_cfgFile.o \
					nl80211/wld_hostapd_ap_api.o \
					nl80211/wld_rad_hostapd_api.o \
					nl80211/wld_wpaSupp_cfgFile.o \
					nl80211/wld_wpaSupp_cfgManager.o \
					nl80211/wld_wpaSupp_ep_api.o \
					nl80211/wld_wpaSupp_parser.o

ifeq ($(CONFIG_SAH_SERVICES_PWHM_DISABLE_PERSIST),y)
$(info Feature persist OFF)
	OBJS-libwld.so += Features/persist/wld_persist_off.o
else
$(info Feature persist ON)
	OBJS-libwld.so += Features/persist/wld_persist_on.o
endif

OBJS = $(OBJS-libwld.so)
LIBS = libwld.so

all: $(LIBS)

$(OBJS): %.o: %.c 
	$(CC) $(CFLAGS) -MMD -MF $(@:.o=.d) -o $@ -c $<

-include $(OBJS:.o=.d)

$(LIBS):
	$(CC) $(CFLAGS) -Wl,-soname,$@ -shared -o $@ $(OBJS-$@) $(LDFLAGS) $(LDFLAGS-$@)

libwld.so: $(OBJS-libwld.so)

clean:
	find . -name "*.so" -delete
	find . -name "*.o" -delete
	find . -name "*.d" -delete

.PHONY: all clean 
