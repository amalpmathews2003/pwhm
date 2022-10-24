ifndef STAGINGDIR
	$(error "STAGINGDIR not defined")
endif


INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include/libnl3)


SUT_DIR=$(realpath ../..)

SUT_OBJECTS = \
	$(patsubst %.c, %.o, $(wildcard ../mocks/*.c))

CFLAGS += -I$(SUT_DIR)/include \
		  -I$(SUT_DIR)/include/wld \
		  -I$(SUT_DIR)/include_priv \
		  -fprofile-arcs -ftest-coverage \
		  $(addprefix -I ,$(INCDIRS)) \
		  $(shell PKG_CONFIG_PATH=$(STAGINGDIR)/usr/lib/pkgconfig pkg-config --cflags sahtrace cmocka swla swlc openssl)
LDFLAGS += -fprofile-arcs -ftest-coverage  \
           -L$(STAGINGDIR)/lib \
           -L$(STAGINGDIR)/usr/lib \
           -L$(STAGINGDIR)/usr/lib/amx/wld \
           -L../../src \
           -L../../src/Plugin \
		   $(shell PKG_CONFIG_PATH=$(STAGINGDIR)/usr/lib/pkgconfig pkg-config --libs sahtrace cmocka swla swlc openssl)\
		   -lamxc -lamxp -lamxd -lamxo -lamxb -lamxm -lnl-3 -lnl-genl-3 -luriparser\
		   -l:libwld.so \
		   -l:wld.so \
		   -lm 

BINARIES = test.fta

test.fta: fta.o $(SUT_OBJECTS)
	$(CC) -o $(@) $^ $(LDFLAGS)

include ../test_common.mk

prerun:
	cp $(SUT_DIR)/src/Plugin/wld.so $(SUT_DIR)/src/wld.so

REPORT_XMLS = $(addsuffix .xml,$(RUNTEST_SUBDIRS))

report: unit.xml

.PHONY: report
