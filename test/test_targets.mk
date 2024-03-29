
all: $(TARGET)

run: $(TARGET)
	@mkdir -p $(LIB_OBJ_DIR)/coverage
	# create symlink to od import folder, needed for local tests.
	if [ ! -d ../../output/machine ] ; then ln -sf $(MACHINE) ../../output/machine;fi
	set -o pipefail; $(LD_LIB) valgrind --leak-check=full --exit-on-first-error=yes --error-exitcode=1 $< 2>&1 | tee -a $(LIB_OBJ_DIR)/coverage/ut_results_$(AUTO_TEST_FILE).txt;

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) -fprofile-arcs -ftest-coverage

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: ./%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o: $(TEST_COMMON_SRC_DIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o: $(MOCK_SRC_DIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/%.o:

$(OBJDIR)/:
	@mkdir -p $@

clean:
	rm -rf $(TARGET) $(OBJDIR)

.PHONY: clean $(OBJDIR)/
