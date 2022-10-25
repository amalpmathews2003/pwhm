SUT_DIR=$(realpath ../..)
include ../test_defines.mk

BINARIES = test.channel5G

test.channel5G: test_channel5G.o $(SUT_OBJECTS)
	$(CC) -o $(@) $^ $(LDFLAGS)

include ../test_common.mk