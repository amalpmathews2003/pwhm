all: wld.odl
wld.odl:  %: %.m4
	m4 -P -DENABLE_USP_SERVER_SOCKET=$(CONFIG_SAH_WLD_ENABLE_USP_SERVER_SOCKET) -DSTAGINGDIR=$(STAGINGDIR) $(<) > $(@)

clean:
	rm -f wld.odl

.PHONY: all clean