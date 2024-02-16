include ../../makefile.inc
# build destination directories
OBJDIR = ../../output/$(MACHINE)

# TARGETS
TARGET_SO = $(OBJDIR)/Plugin/wld.so

# directories
# source directories
SRCDIR = .
INCDIR = ../../include
INCDIR_PRIV = ../../include_priv
INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include/libnl3)
STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib)

# files
#HEADERS = $(wildcard $(INCDIR_PUB)/$(TARGET_NAME)/*.h)
SOURCES = $(wildcard *.c */*.c */*/*.c)
OBJECTS = $(addprefix $(OBJDIR)/Plugin/,$(SOURCES:.c=.o))
SUBDIRS_BASE = $(shell find $(SRCDIR) -type d)
SUBDIRS = $(addprefix $(OBJDIR)/Plugin,$(patsubst $(SRCDIR)%,%/,$(SUBDIRS_BASE)))

CFLAGS := -std=gnu99 -fPIC -g -Wall -Werror -Wextra  \
		$(addprefix -I ,$(INCDIRS)) \
		-I$(INCDIR_PRIV)/Plugin/ \
		-I$(INCDIR)/ \
		-DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=500
CFLAGS += -Wformat -Wformat-security -Wimplicit-function-declaration -Wl,--no-undefined -Wno-unused-variable
LDFLAGS := -L$(OBJDIR) $(STAGING_LIBDIR) -Wl,-rpath,/lib -Wl,-rpath,/usr/lib -shared  -lsahtrace -lswlc -lswla -lwld -lamxo -lamxb -lamxc -lamxm -lamxp -lamxd

all: $(TARGET_SO)

$(TARGET_SO): $(OBJECTS)
	$(CC) -Wl,-soname,wld.so -o $(@) $(OBJECTS) $(LDFLAGS)

-include $(OBJECTS:.o=.d)

$(OBJDIR)/Plugin/%.o: $(SRCDIR)/%.c | $(OBJDIR)/Plugin/ $(SUBDIRS)
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

%/:
	$(MKDIR) -p $@

clean:
	rm -f $(OBJDIR)/Plugin

.PHONY: clean all
