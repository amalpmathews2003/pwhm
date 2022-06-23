#!/bin/sh

get_base_wan_address()
{
    # Define WAN_ADDR env var needed by pwhm to initialize wifi devices
    if [ -e "/etc/environment" ]; then
        set -a
        . /etc/environment 2> /dev/null
        if [ -n "$WAN_ADDR" ]; then
            exit
        fi
    fi
    export WAN_ADDR=$(cat /sys/class/net/br-lan/address 2> /dev/null)
}

case $1 in
    start|boot)
        export LD_LIBRARY_PATH=/usr/lib/amx/wld
        get_base_wan_address
        mkdir -p /var/lib/wld
        amxrt -D /etc/amx/wld/wld.odl
        ;;
    stop)
        if [ -f /var/run/wld.pid ]; then
            kill `cat /var/run/wld.pid`
        fi
        ;;
    restart)
        $0 stop
        $0 start
        ;;
    debuginfo)
        ubus-cli "WiFi.?"
        ;;
    log)
        echo "log wld"
        ;;
    *)
        echo "Usage : $0 [start|boot|stop|debuginfo|log]"
        ;;
esac
