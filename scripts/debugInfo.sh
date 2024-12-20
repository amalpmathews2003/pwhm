printCmd(){
    echo "---------------------------------"
    echo -e "${1}"
    echo "---------------------------------"
    ${1}
}

echo "##### pWHM WiFi Debug #####"

echo ""
echo ""
echo "#### iw dev"
iw dev

echo ""
echo ""
echo "#### iw list"
iw list


for FILE in /tmp/*_hapd.conf; do
    echo ""
    echo ""
    echo "#### hostapd config ${FILE}"
    echo ""
    cat $FILE
done


interfaces=$(iw dev | grep Interface | awk '{print $2}' | sort)
for INTF in $interfaces; do
  echo ""
  echo "#### hostapd state ${INTF}"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} driver_flags"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} get_config"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} status"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} all_sta"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} wps_get_status"
done
