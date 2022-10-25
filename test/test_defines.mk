ifndef STAGINGDIR
	$(error "STAGINGDIR not defined")
endif
ifndef SUT_DIR
	$(error "SUT_DIR not defined")
endif



SUT_OBJECTS = \
	$(patsubst %.c, %.o, $(wildcard $(SUT_DIR)/test/testHelper/*.c))

INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include/libnl3)


CFLAGS += -I$(SUT_DIR)/include \
		  -I$(SUT_DIR)/include_priv \
		  -I$(SUT_DIR)/include/wld \
		  -I$(SUT_DIR)/include_priv/nl80211 \
		  -I$(STAGINGDIR)/usr/include \
		  -fprofile-arcs -ftest-coverage \
		  -g3 \
		  -Wextra -Wall -Werror \
		  $(shell PKG_CONFIG_PATH=$(STAGINGDIR)/usr/lib/pkgconfig pkg-config --cflags sahtrace pcb cmocka swla swlc openssl test-toolbox)
LDFLAGS += -fprofile-arcs -ftest-coverage  \
		   -L$(STAGINGDIR)/lib \
		   -L$(STAGINGDIR)/usr/lib \
		   -Wl,-rpath,$(STAGINGDIR)/lib \
		   -Wl,-rpath,$(STAGINGDIR)/usr/lib \
		   $(shell PKG_CONFIG_PATH=$(STAGINGDIR)/usr/lib/pkgconfig pkg-config --libs sahtrace pcb cmocka swla swlc openssl test-toolbox) \
		   -lamxb -lamxc -lamxd -lamxo \
		   -L$(SUT_DIR)/src \
		   -L$(SUT_DIR)/src/Plugin \
		   -l:libwld.so \
		   -l:wld.so \
		   -lm \
