AUTO_TEST_FILE = wld_nl80211

include ../test_defines.mk

CFLAGS += $(shell PKG_CONFIG_PATH=$(PKGCONFDIR) pkg-config --define-prefix --cflags libnl-genl-3.0) \
          -I../../include_priv/nl80211 \

LDFLAGS += $(shell PKG_CONFIG_PATH=$(PKGCONFDIR) pkg-config --define-prefix --libs libnl-genl-3.0)

include ../test_targets.mk
