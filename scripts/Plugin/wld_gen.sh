#!/bin/sh
[ -f /etc/environment ] && source /etc/environment
ulimit -c ${ULIMIT_CONFIGURATION:-0}
name="wld"
MAX_SIGTERM_RETRIES=3

prevent_netifd_to_configure_wireless()
{
    entries=$(uci -q show wireless 2> /dev/null | grep -e "wifi-iface" -e "wifi-device" | cut -d '=' -f1 | cut -d '.' -f2) && [ -n "$entries" ] || return
    for i in $entries; do
        uci -q del wireless.$i
    done
    uci -q commit wireless
    logger -t $name -p daemon.warn "Wireless is managed by prplMesh Wireless Hardware Manager"
}

get_base_wan_address()
{
    # Define WAN_ADDR env var needed by pwhm to initialize wifi devices
    if [ -e "/etc/environment" ]; then
        set -a
        . /etc/environment 2> /dev/null
        if [ -z "$WAN_ADDR" -a -n "$BASEMACADDRESS" ]; then
            export WAN_ADDR="$BASEMACADDRESS"
        fi
        if [ -z "$WLAN_ADDR" -a -n "$BASEMACADDRESS_PLUS_1" ]; then
            export WLAN_ADDR="$BASEMACADDRESS_PLUS_1"
        fi
        [ -z "$WLAN_ADDR" ] || return
    fi
    export WLAN_ADDR=$(cat /sys/class/net/br-lan/address 2> /dev/null)
}

kill_process()
{
    # kill process with PID == $1
    # try a SIGTERM for MAX_SIGTERM_RETRIES tries, then SIGKILL
    # SIGTERM : 15 ; SIGKILL : 9

    exit_condition=false
    echo "killing PID" $1
    tries=0
    max_tries=$MAX_SIGTERM_RETRIES
    kill -0 $1 2>/dev/null
    while [[ $? -eq 0 && $((tries++)) -lt $max_tries ]] ; do
        # try sigterm : 15 first
        kill -SIGTERM $1 && sleep 1 && kill -0 $1 2>/dev/null
    done
    # if still running, try sigkill
    kill -0 $1 2>/dev/null && kill -SIGKILL $1 && echo "killed with sigkill"

    # kill -0 returns true if PID is running and we can send signals to it
    # && command linking executes the next if previous command returns true
}

case $1 in
    start|boot)
        get_base_wan_address
        prevent_netifd_to_configure_wireless
        mkdir -p /var/lib/${name}
        touch /var/lib/${name}/${name}_config.odl
        ${name} -D
        ;;
    stop|shutdown)
        if [ -f /var/run/${name}.pid ]; then
            echo "stopping ${name}"
            kill_process `cat /var/run/${name}.pid`
        fi
        ;;
    restart)
        $0 stop
        $0 start
        ;;
    debuginfo)
        ubus-cli "WiFi.?"
        /usr/lib/debuginfo/debug_wifi.sh
        ;;
    log)
        echo "log ${name}"
        ;;
    *)
        echo "Usage : $0 [start|boot|stop|shutdown|restart|debuginfo|log]"
        ;;
esac
