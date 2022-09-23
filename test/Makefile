RUNTEST_SUB_MAKEFILES = $(wildcard */*/Makefile) $(wildcard */Makefile)
RUNTEST_SUBDIRS = $(subst /Makefile,,$(RUNTEST_SUB_MAKEFILES))
REPORT_XMLS = $(subst %,%.xml,$(RUNTEST_SUBDIRS))

# Workaround.
# For some reason on local testruns sometimes there are build errors
# because the PKG_CONFIG_LIBDIR set does not work, so unset it.
unexport PKG_CONFIG_LIBDIR

test: $(RUNTEST_SUBDIRS)
runtest: test	
	echo BLA
	cp $(SUT_DIR)/src/Plugin/wld.so $(SUT_DIR)/src/wld.so

report: $(patsubst %,%.xml,$(REPORT_XMLS))

$(RUNTEST_SUBDIRS):
	$(MAKE) -C $(subst .xml,,$@) runtest

%.xml:
	$(MAKE) -C $(subst .xml,,$@) unit.xml
	cp $(subst .xml,,$@)/unit.xml $@

export GCOV_SETTINGS=--print-summary --root $(realpath ..)
coverage_report:
	mkdir -p coverage
	gcovr $(GCOV_SETTINGS) # just for the output
	gcovr $(GCOV_SETTINGS) --html --html-details -o coverage/swl.html

clean:
	rm -rf coverage
	find . -name "*.o" -delete
	find . -name "*.xml" -delete
	find . -name "*.gcno" -delete
	find . -name "*.gcda" -delete
	find . -name "*.d" -delete
	find . -name "test.*_test" -delete
	
	find . -name "gentest_*" -delete

.PHONY: $(RUNTEST_SUBDIRS) $(REPORT_XMLS) coverage_report report runtest test clean
