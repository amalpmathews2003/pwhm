PKG_CONFIG_LIBDIR := /usr/lib/pkgconfig:/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:$(PKG_CONFIG_LIBDIR)
MACHINE = $(shell $(CC) -dumpmachine)
SUBDIRS=$(subst /Makefile,,$(wildcard */*/Makefile) $(wildcard */Makefile))
UT_BUILDDIR = $(realpath ../output/$(MACHINE)/test)
OBJDIR = $(realpath ../output/$(MACHINE))
COVERREPORT = $(OBJDIR)/coverage/report

run: 
	for dir in $(SUBDIRS); do make -C $$dir $@  || exit -1; done

clean:
	rm -rf $(UT_BUILDDIR)/test_*.o
	rm -rf $(UT_BUILDDIR)/$(COVERREPORT)
	find .. -name "run_test" -delete

coverage:
	mkdir -p $(COVERREPORT)
	cd $(OBJDIR) && \
	for i in $$(find . -type f -iname "*.o"); do \
		gcov -c -b -f --long-file-names --preserve-paths $$i > /dev/null; \
    done
	cd $(OBJDIR) && for i in $$(find . -name "*.h.gcov"); do rm $$i > /dev/null; done
	cd $(OBJDIR) && gcovr -k -p -r ../../ -g -s --html --html-details -o $(COVERREPORT)/index.html .
	cd $(OBJDIR) && gcovr -k -p -r ../../ -g -s . | tee $(COVERREPORT)/gcovr_summary.txt


.PHONY: run clean coverage
