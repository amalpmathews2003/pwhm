# include components.config
-include $(STAGINGDIR)/components.config

CFLAGS += -std=gnu99 -fPIC -g -Wall -Werror -Wextra  \
		-I. \
		-I../include_priv/ChannelManagement/ \
		-I../include_priv/VendorModule/ \
		-I../include/wld/ -I$(STAGINGDIR)/include \
		-I../include_priv/nl80211/ \
		$(shell pkg-config --cflags sahtrace swla libnl-3.0)
CFLAGS += -Wimplicit-function-declaration -Wl,--no-undefined -Wno-unused-variable
LDFLAGS += -L$(STAGINGDIR)/lib $(shell pkg-config --libs sahtrace swla openssl libnl-genl-3.0) -lrt -lm -lamxo -lamxb -lamxc -lamxm -lamxp -lamxd -ldl

ifeq ($(COVERAGE),y)
CFLAGS +=-fprofile-arcs -ftest-coverage
LDFLAGS += -fprofile-arcs -ftest-coverage
endif

OBJS-libwld.so = wld.o wld_accesspoint.o wld_endpoint.o wld_radio.o \
		wld_ssid.o wld_util.o wld_wps.o wld_assocdev.o wld_statsmon.o \
		wld_functionTable.o \
		wld_radioOperatingStandards.o 
		
OBJS-libwld.so += 	ChannelManagement/wld_channel.o \
					ChannelManagement/wld_channel_info.o \
					RadMgt/wld_prb_req.o\
					RadMgt/wld_rad_stamon.o\
					RadMgt/wld_rad_caps.o\
					RadMgt/wld_rad_ifMgt.o\
					RadMgt/wld_rad_fsm.o\
					RadMgt/wld_rad_evtHandler.o\
					RadMgt/wld_rad_scan.o\
					RadMgt/wld_rad_delayMgr.o\
					ApMgt/wld_ap_neighbours.o\
					ApMgt/wld_ap_macfilter.o\
					ApMgt/wld_ap_bss_transfer.o\
					ApMgt/wld_ap_rrm.o \
					ApMgt/wld_ap_rssiMonitor.o \
					ApMgt/wld_ap_security.o \
					EpMgt/wld_tinyRoam.o \
					EpMgt/wld_tinyRoam_bus.o \
					Utils/wld_eventing.o \
					Utils/wld_monitor.o \
					Utils/wld_autoCommitMgr.o \
					Utils/wld_daemon.o \
					Utils/wld_linuxIfUtils.o \
					Utils/wld_linuxWExtUtils.o \
					Utils/wld_linuxIfStats.o \
					VendorModule/wld_vendorModule_mgr.o \
					VendorModule/wld_vendorModule.o \
					nl80211/wld_nl80211_core.o \
					nl80211/wld_nl80211_api.o \
					nl80211/wld_nl80211_events.o \
					nl80211/wld_nl80211_parser.o \
					nl80211/wld_rad_nl80211.o \
					nl80211/wld_ap_nl80211.o \
					nl80211/wld_wpaCtrlInterface.o \
					nl80211/wld_wpaCtrlMngr.o \
					nl80211/wld_wpaCtrl_events.o \
					nl80211/wld_secDmn.o \
					nl80211/wld_hostapd_cfgManager.o \
					nl80211/wld_hostapd_cfgFile.o \
					nl80211/wld_hostapd_ap_api.o \
					nl80211/wld_rad_hostapd_api.o

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
