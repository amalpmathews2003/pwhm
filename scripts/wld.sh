#!/bin/sh

getZoneFromMap() {
	case $1 in
		"wld") zone="wld rad ssid ap wps";;
		"ap") zone="ap wps ap11v apMf apNeigh ap11k apRssi apSec";;
		"ep") zone="ep epMon";;
		"rad") zone="rad radPrb radCaps radStm";;
		"util") zone="util utilMon utilEvt";;
		"io") zone="ioIn ioOut ioProbe";;
		*) zone="wld wld_rad wld_ssid wld_ap wld_ep";;
	esac
}

getAllZones() {
	trace_zones="ad ap ap11k ap11v apMf apNeigh apRssi ep rad radCaps radPrb radStm ssid util utilMon wld wps"
}

case $1 in
	getZoneFromMap)
		shift 1
		getZoneFromMap $1
		echo $zone
		;;
	getAllZones)
		getAllZones
		echo $trace_zones
esac
