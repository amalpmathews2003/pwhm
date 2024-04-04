include ../makefile.inc

all: wld/wld_nl80211_path.h

KERN_NL80211_HDR_PATH := $(if $(STAGINGDIR),$(if $(wildcard $(STAGINGDIR)/usr/include/mac80211/uapi/linux/nl80211.h),mac80211/uapi/linux/nl80211.h,),)
NL80211_HDR_PATH := $(if $(KERN_NL80211_HDR_PATH),$(KERN_NL80211_HDR_PATH),linux/nl80211.h)

wld/wld_nl80211_path.h:
	$(ECHO) "#include <$(NL80211_HDR_PATH)>" > $@

.PHONY: all
