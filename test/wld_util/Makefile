ifndef STAGINGDIR
	$(error "STAGINGDIR not defined")
endif

SUT_DIR=$(realpath ../..)

SUT_OBJECTS = $(SUT_DIR)/src/wld_util.o \
		$(SUT_DIR)/src/Utils/wld_linuxIfUtils.o

CFLAGS += -I$(SUT_DIR)/include \
		  -I$(SUT_DIR)/include/wld \
		  -fprofile-arcs -ftest-coverage \
		  $(shell PKG_CONFIG_PATH=$(STAGINGDIR)/build/pkg-config pkg-config --cflags sahtrace cmocka swla swlc openssl)
LDFLAGS += -fprofile-arcs -ftest-coverage  \
           -L$(STAGINGDIR)/lib \
		   $(shell PKG_CONFIG_PATH=$(STAGINGDIR)/build/pkg-config pkg-config --libs sahtrace cmocka swla swlc openssl)

LDFLAGS += -lamxc -lamxd -lamxp -lamxb

BINARIES = test.wld_test

test.wld_test: test_wld.o $(SUT_OBJECTS)
	$(CC) -o $(@) $^ $(LDFLAGS)

include ../test_common.mk


REPORT_XMLS = $(addsuffix .xml,$(RUNTEST_SUBDIRS))


report: unit.xml

.PHONY: report
