#!/bin/sh

get_base_wan_address()
{
    # Define WAN_ADDR env var needed by pwhm to initialize wifi devices
    if [ -z "$WAN_ADDR" ]; then
        export WAN_ADDR=$(cat /sys/class/net/br-lan/address 2> /dev/null)
    fi
}

case $1 in
    start|boot)
        export LD_LIBRARY_PATH=/usr/lib/amx/wld
        set -a
        . /etc/environment
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
