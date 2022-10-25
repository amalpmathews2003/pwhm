SUT_DIR=$(realpath ../..)
include ../test_defines.mk

BINARIES = test.fta

test.fta: fta.o $(SUT_OBJECTS)
	$(CC) -o $(@) $^ $(LDFLAGS)

include ../test_common.mk