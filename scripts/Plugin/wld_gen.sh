#!/bin/sh
[ -f /etc/environment ] && source /etc/environment
ulimit -c ${ULIMIT_CONFIGURATION:-0}

prevent_netifd_to_configure_wireless()
{
    if [ -e "/etc/config/wireless" ]; then
        for i in $(uci -q show wireless | grep wifi-device | cut -d '=' -f1 | cut -d '.' -f2); do
            uci -q del wireless.$i
        done
        echo "# Wireless is managed by prplMesh Wireless Hardware Manager" >> /etc/config/wireless
        echo "# this is Backup of previous configuration that was available at /etc/config/wireless" >> /etc/config/wireless
        mv /etc/config/wireless /etc/config/netifd.wireless.backup
    fi
}

get_base_wan_address()
{
    # Define WAN_ADDR env var needed by pwhm to initialize wifi devices
    if [ -e "/etc/environment" ]; then
        set -a
        . /etc/environment 2> /dev/null
        if [ -n "$WAN_ADDR" ]; then
            return
        fi
    fi
    export WAN_ADDR=$(cat /sys/class/net/br-lan/address 2> /dev/null)
}

case $1 in
    start|boot)
        get_base_wan_address
        prevent_netifd_to_configure_wireless
        mkdir -p /var/lib/wld
        amxrt /etc/amx/wld/wld.odl &
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
