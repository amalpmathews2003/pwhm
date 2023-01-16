ifndef STAGINGDIR
	$(error "STAGINGDIR not defined")
endif

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SUT_DIR = $(dir $(MKFILE_PATH))/..
$(info $$SUT_DIR is [${SUT_DIR}])

SUT_OBJECTS = \
	$(patsubst %.c, %.o, $(wildcard $(SUT_DIR)/test/testHelper/*.c))

INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include)
PKGCONFDIR = $(STAGINGDIR)/usr/lib/pkgconfig

CFLAGS += -I$(SUT_DIR)/include \
		  -I$(SUT_DIR)/include_priv \
		  -I$(SUT_DIR)/include/wld \
		  -I$(STAGINGDIR)/usr/include \
		  -fprofile-arcs -ftest-coverage \
		  -g3 \
		  -Wextra -Wall -Werror \
		  $(addprefix -I ,$(INCDIRS)) \
		  $(shell PKG_CONFIG_PATH=$(PKGCONFDIR) pkg-config --define-prefix --cflags sahtrace pcb cmocka swla swlc openssl test-toolbox)
LDFLAGS += -fprofile-arcs -ftest-coverage  \
		   -L$(STAGINGDIR)/lib \
		   -L$(STAGINGDIR)/usr/lib \
		   -Wl,-rpath,$(STAGINGDIR)/lib \
		   -Wl,-rpath,$(STAGINGDIR)/usr/lib \
		   $(shell PKG_CONFIG_PATH=$(PKGCONFDIR) pkg-config --define-prefix --libs sahtrace pcb cmocka swla swlc openssl test-toolbox) \
		   -lamxb -lamxc -lamxd -lamxo -lamxp -lamxj\
		   -L$(SUT_DIR)/src \
		   -L$(SUT_DIR)/src/Plugin \
		   -l:libwld.so \
		   -l:wld.so \
		   -lm \
