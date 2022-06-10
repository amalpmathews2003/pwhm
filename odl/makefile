export COMPONENT = wld

all: $(COMPONENT)_definitions.odl

$(COMPONENT)_definitions.odl: $(COMPONENT)_definitions.odl.m4 $(STAGINGDIR)/components.config_m4.m4
	m4 -P -DCOMPONENT=$(COMPONENT) -DSTAGINGDIR=$(STAGINGDIR) $(COMPONENT)_definitions.odl.m4 > $(COMPONENT)_definitions.odl

clean:
	rm -f $(COMPONENT)_definitions.odl

.PHONY: clean all