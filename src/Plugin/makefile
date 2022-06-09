include ../../makefile.inc

CFLAGS += -std=gnu99 -fPIC -g -Wall -Werror -Wextra -Wno-unused-but-set-variable -I../../include/ -I$(STAGINGDIR)/include -I../../include_priv/Plugin
CFLAGS += -Wimplicit-function-declaration -Wl,--no-undefined
CFLAGS += -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=500

LDFLAGS += -L.. -L$(STAGINGDIR)/lib -L$(STAGINGDIR)/usr/lib
LDFLAGS += -Wl,-rpath,$(STAGINGDIR)/lib -Wl,-rpath,$(STAGINGDIR)/usr/lib -Wl,-rpath,$(STAGINGDIR)/usr/lib/wld
LDFLAGS += -lsahtrace -lswlc -lswla -lwld -lamxo -lamxb -lamxc -lamxm -lamxp -lamxd

OBJS-wld.so = wld_plugin.o \
		  wifiGen.o \
		  wifiGen_rad.o \
		  wifiGen_radIfMgt.o \
		  wifiGen_vap.o \
		  wifiGen_hapd.o \
		  wifiGen_wpaSupp.o \
		  wifiGen_eventHandlers.o \
		  wifiGen_fsm.o \


OBJS = $(OBJS-wld.so)
LIBS = wld.so

all: $(LIBS)

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -o $@ -c $<

-include $(OBJS:.o=.d)

$(LIBS):
	$(CC) $(CFLAGS) -Wl,-soname,$@ -shared -o $@ $(OBJS-$@) $(LDFLAGS) $(LDFLAGS-$@)

wld.so: $(OBJS-wld.so)

clean:
	rm -f $(LIBS) $(OBJS) $(OBJS:.o=.d)

.PHONY: clean all
