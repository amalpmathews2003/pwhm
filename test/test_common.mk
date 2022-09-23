# Define the `runtest` and `unit.xml` targets
# in a reusable way.
#
# This makefile is intended to be included in a test makefile.
#
# USAGE
#
#   the test makefile should define
#
#   * BINARIES: the programs to be run as tests
#   * OBJECTS: the objects required to build these programs
#   * UNITXMLS: the unit.xml outputs expected to result from these programs

VALGRIND?=valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=10
UT_SETTINGS=CMOCKA_MESSAGE_OUTPUT=XML
LD_LIB=LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(STAGINGDIR)/usr/lib

runtest: $(BINARIES) prerun
	($(foreach binary,$(BINARIES),$(LD_LIB) $(VALGRIND) ./$(binary) &&) true) || ($(MAKE) postrun && false)
	$(MAKE) postrun

unit.xml: $(UNITXMLS) $(BINARIES) runtest
	# poorman's way of joining two xml files with <testsuites><testsuite> elements
	echo '<?xml version="1.0" encoding="UTF-8" ?>' > $@
	echo "<testsuites>" >> $@
	$(foreach binary,$(BINARIES),$(UT_SETTINGS) $(LD_LIB) $(VALGRIND) ./$(binary) | grep -v "testsuites" | grep -v "?xml" >> $@;)
	echo '</testsuites>' >> $@
	grep -B 1 -A 3 '<failure>' < $@ && exit 1 || exit 0

prerun: 

postrun:

-include $(OBJECTS:.o=.d)

CFLAGS += -g -O0 -Werror -Wextra -Wall

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	$(CC) $(CFLAGS) -MM -MP -MT '$@ $(@:.o=.d)' -MF $(@:.o=.d) $<

clean:
	rm -rf *.o
	rm -rf test.*
	rm -rf gentest_*

.PHONY: runtest prerun postrun clean
