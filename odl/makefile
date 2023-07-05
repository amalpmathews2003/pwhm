all: wld.odl
wld.odl:  %: %.m4
	m4 -P -DMOD_AMXB_USP_ENABLED=$(CONFIG_PACKAGE_MOD_AMXB_USP) -DSTAGINGDIR=$(STAGINGDIR) $(<) > $(@)

clean:
	rm -f wld.odl

.PHONY: all clean