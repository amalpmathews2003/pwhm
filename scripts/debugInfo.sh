printCmd(){
    echo "---------------------------------"
    echo -e "${1}"
    echo "---------------------------------"
    ${1}
}

echo "##### pWHM WiFi Debug #####"

echo "\n\n### Radios Firmware"
ba-cli 'WiFi.Radio.*.FirmwareVersion?' | sed 1d

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


interfaces=$(iw dev | grep -E "Interface|type" | grep -B 1 "AP" | grep Interface | awk '{print $2}'| sort)
for INTF in $interfaces; do
  echo ""
  echo "#### hostapd state ${INTF}"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} driver_flags"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} get_config"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} status"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} all_sta"
  printCmd "$SUDO hostapd_cli -p /var/run/hostapd -i ${INTF} wps_get_status"
done



for FILE in /tmp/*_wpa_supplicant.conf; do
    echo ""
    echo ""
    echo "#### wpa supplicant config ${FILE}"
    echo ""
    cat $FILE
done


epEnterfaces=$(iw dev | grep -E "Interface|type" | grep -B 1 "managed" | grep Interface | awk '{print $2}'| sort)
for INTF in $epEnterfaces; do
  echo ""
  echo "#### wpa_supplicant state ${INTF}"
  printCmd "$SUDO wpa_cli -p /var/run/wpa_supplicant -i ${INTF} status"
  printCmd "$SUDO wpa_cli -p /var/run/wpa_supplicant -i ${INTF} dump"
  printCmd "$SUDO wpa_cli -p /var/run/wpa_supplicant -i ${INTF} list_networks"
  printCmd "$SUDO wpa_cli -p /var/run/wpa_supplicant -i ${INTF} scan_results"
done
