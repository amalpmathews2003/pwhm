# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]


## Release sah-next_v7.24.7 - 2025-08-14(13:39:46 +0000)

### Other

- fix extender early ep connect and regDom mismatch

## Release sah-next_v7.24.6 - 2025-08-13(12:29:26 +0000)

### Other

- fix Endpoint statistics

## Release sah-next_v7.24.5 - 2025-08-12(16:49:48 +0000)

### Other

- - Set 6GHz regulatory power type to Indoor (0)

## Release sah-next_v7.24.4 - 2025-08-12(13:56:35 +0000)

### Other

- - Repeater is not deauthenticated properly

## Release sah-next_v7.24.3 - 2025-08-12(10:53:12 +0000)

### Other

- Guest WiFi disabled but still shows traffic in WebUI

## Release sah-next_v7.24.2 - 2025-08-06(13:59:07 +0000)

### Other

- [ACL][Mainline] Admin cannot access WiFi.AccessPoint.*.MACFiltering

## Release sah-next_v7.24.1 - 2025-08-06(08:52:45 +0000)

### Other

- – Hostapd repeater repeatedly restarts after reboot when only vap6g0guest is enabled on the 6GHz band.

## Release sah-next_v7.24.0 - 2025-08-04(10:38:34 +0000)

### Other

- TR-181: WiFi - power management

## Release sah-next_v7.23.13 - 2025-08-04(08:01:43 +0000)

### Other

- Extender is not pairing with router in WPS

## Release sah-next_v7.23.12 - 2025-07-31(10:20:15 +0000)

### Other

- [import] Changing security mode from WPA2-WP3-Personal to WPA3-Personal triggers Hostapd restart

## Release sah-next_v7.23.11 - 2025-07-31(09:55:33 +0000)

## Release sah-next_v7.23.10 - 2025-07-31(09:20:00 +0000)

### Other

- 
- improve handling of empty ProfileReference

## Release sah-next_v7.23.9 - 2025-07-30(17:25:46 +0000)

### Other

- - set sta interface and 4addr mode per default

## Release sah-next_v7.23.8 - 2025-07-30(15:58:47 +0000)

### Other

- pwhm crash when starting hostapd

## Release sah-next_v7.23.7 - 2025-07-30(10:15:25 +0000)

### Other

- Memory leak in PWHM

## Release sah-next_v7.23.6 - 2025-07-30(07:21:29 +0000)

### Other

- – ZWDFS should not run during ZTP

## Release sah-next_v7.23.5 - 2025-07-29(16:03:15 +0000)

### Other

- fix ap enable conf overwritten by ssid enable sync

## Release sah-next_v7.23.4 - 2025-07-29(08:15:00 +0000)

### Other

- - print hostapd link status for debug

## Release sah-next_v7.23.3 - 2025-07-23(15:40:08 +0000)

### Other

- Steered devices to wifi 5ghz still appear associated with the 2.4 GHz

## Release sah-next_v7.23.2 - 2025-07-22(15:53:53 +0000)

### Other

- - Avoid connecting EP to a bss on the wrong band

## Release sah-next_v7.23.1 - 2025-07-22(15:07:16 +0000)

### Other

- avoid ep reconnection using misconfigured referenced profile

## Release sah-next_v7.23.0 - 2025-07-18(11:23:10 +0000)

### Other

- get vendor wiphy info using wiphy id

## Release sah-next_v7.22.9 - 2025-07-18(09:20:12 +0000)

### Other

- - Update all MLD links status

## Release sah-next_v7.22.8 - 2025-07-18(06:47:40 +0000)

### Other

- [import] remove wifi7 cipher/akm algo when MLO is disabled

## Release sah-next_v7.22.7 - 2025-07-17(15:21:24 +0000)

### Other

- - Avoid infinite recursion in wifiGen_refreshEpConnStatus

## Release sah-next_v7.22.6 - 2025-07-17(14:36:57 +0000)

## Release sah-next_v7.22.5 - 2025-07-16(07:04:49 +0000)

## Release sah-next_v7.22.4 - 2025-07-15(10:47:48 +0000)

### Other

- Per-link station signal strength value is not available

## Release sah-next_v7.22.3 - 2025-07-11(17:23:45 +0000)

### Other

- - Ensure 4addr is enabled on managed interface

## Release sah-next_v7.22.2 - 2025-07-11(17:06:32 +0000)

### Other

- [STB] bad behavior for TV live stream

## Release sah-next_v7.22.1 - 2025-07-04(08:24:51 +0000)

### Other

- prevent crash hapd UPDATE when enabling MLD link

## Release sah-next_v7.22.0 - 2025-07-03(08:13:34 +0000)

### Other

- set MLDRole,MLDStatus,MLDLinkID for all MLD links

## Release sah-next_v7.21.5 - 2025-07-03(08:03:20 +0000)

### Other

- avoid force hapd UPDATE while starting

## Release sah-next_v7.21.4 - 2025-07-02(16:47:42 +0000)

### Other

- ep stats transaction failing in loop

## Release sah-next_v7.21.3 - 2025-07-02(16:36:07 +0000)

### Other

- fix error logs flooding when refreshing disabled vaps status

## Release sah-next_v7.21.2 - 2025-07-02(16:25:19 +0000)

### Fixes

- multiple wpa_s wpactl events missed while EP connecting

## Release sah-next_v7.21.1 - 2025-07-01(17:19:06 +0000)

### Other

- - VAP remains down

## Release sah-next_v7.21.0 - 2025-06-30(14:32:55 +0000)

### Other

- support custom start args for wpa_supplicant

## Release sah-next_v7.20.5 - 2025-06-30(14:05:29 +0000)

### Other

- fix radio active criterias

## Release sah-next_v7.20.4 - 2025-06-30(13:47:54 +0000)

### Other

- PPW-525 [import] avoid too frequent refresh of assoc dev info

## Release sah-next_v7.20.3 - 2025-06-30(13:20:08 +0000)

### Other

- fail to switch between single and multi hostapd

## Release sah-next_v7.20.2 - 2025-06-30(09:32:54 +0000)

### Other

- [import]  Unable to Enable SSIDs via Wi-Fi/SSIDs WebUI page.

## Release sah-next_v7.20.1 - 2025-06-30(09:15:36 +0000)

### Other

- fail to set fronthaul chanspec with target backhaul chanspec

## Release sah-next_v7.20.0 - 2025-06-27(13:51:08 +0000)

### Other

- - removing wpa_s network specific channels conf params overridden by global freq_list

## Release sah-next_v7.19.0 - 2025-06-27(08:32:03 +0000)

### Other

- : Add beacon protection configuration support

## Release sah-next_v7.18.24 - 2025-06-27(08:00:57 +0000)

### Other

- Fix UT local run

## Release sah-next_v7.18.23 - 2025-06-26(11:40:45 +0000)

### Other

- - Missing target chanspec  update after EP connection

## Release sah-next_v7.18.22 - 2025-06-26(10:18:37 +0000)

### Other

- - clean autocreated hostapd interfaces

## Release sah-next_v7.18.21 - 2025-06-26(08:15:39 +0000)

### Other

- - MLO Disabled AP matching invalid

## Release sah-next_v7.18.20 - 2025-06-20(14:15:20 +0000)

### Other

- hostapd not started although vap is enabled

## Release sah-next_v7.18.19 - 2025-06-19(10:38:22 +0000)

### Other

- EndPoint 2g is connecting and disconnected in loop

## Release sah-next_v7.18.18 - 2025-06-18(09:33:49 +0000)

### Other

- fix undefined AkmSuite param in endpoint obj

## Release sah-next_v7.18.17 - 2025-06-17(17:28:27 +0000)

### Other

- - Radio invalid status

## Release sah-next_v7.18.16 - 2025-06-17(13:05:52 +0000)

### Other

- fix wifi reset letting AP/EP/SSID enabling unsynced

## Release sah-next_v7.18.15 - 2025-06-16(08:43:42 +0000)

### Other

- ep 6g connects with bw 160Mhz isof 320MHz

## Release sah-next_v7.18.14 - 2025-06-13(16:45:21 +0000)

### Other

- multi_ap_profile add when not supported

## Release sah-next_v7.18.13 - 2025-06-11(15:05:26 +0000)

### Other

- : Expose MRSNO capability, AKM and cipher info for STA

## Release sah-next_v7.18.12 - 2025-06-11(08:52:28 +0000)

### Other

- - The client PIN doesn't work if the PIN starts with 0

## Release sah-next_v7.18.11 - 2025-06-10(13:32:54 +0000)

### Other

- add api to get radio's MLO hw capability

## Release sah-next_v7.18.10 - 2025-06-10(09:36:51 +0000)

### Other

- Add a check if pairingTimer is running
- refresh bw on connected bSTAMLD link

## Release sah-next_v7.18.9 - 2025-06-10(07:10:45 +0000)

### Other

- - check the BSSID validity

## Release sah-next_v7.18.8 - 2025-06-06(14:23:51 +0000)

### Other

- fix reading link bandwidth 320MHz

## Release sah-next_v7.18.7 - 2025-06-06(14:09:36 +0000)

### Other

- fix reconnect triggered while connection is ongoing

## Release sah-next_v7.18.6 - 2025-06-06(13:58:50 +0000)

### Other

- [syslog] Problematic Log Entry for Parsing
- [import] [tr181-device] [import] NumberOfEntries are missing in Device.WiFi.
- fix ep connection bssid always saved in currentProfile

## Release sah-next_v7.18.5 - 2025-06-05(15:23:41 +0000)

### Other

- repeater's endpoint not reconnecting when toggling the gwy AP radio

## Release sah-next_v7.18.4 - 2025-05-27(12:13:02 +0000)

### Other

- - The disassociation management frame is always reported as being transmitted over the primary link

## Release sah-next_v7.18.3 - 2025-05-26(08:44:08 +0000)

### Other

- [import] Set multi_ap_profile to the wpa_supplicant config

## Release sah-next_v7.18.2 - 2025-05-26(08:05:55 +0000)

### Other

- [import] PWHM: wld_dmn_startDeamon: Prevent stacking of stop signal handlers during daemon restarts

## Release sah-next_v7.18.1 - 2025-05-26(07:48:27 +0000)

### Other

- - AP Roaming does not work

## Release sah-next_v7.18.0 - 2025-05-23(16:27:40 +0000)

### Other

- Add support for ZW_DFS block mechanism by components

## Release sah-next_v7.17.9 - 2025-05-23(08:54:16 +0000)

### Other

- - Avoid desync between AP enable and SSID enable

## Release sah-next_v7.17.8 - 2025-05-22(16:51:07 +0000)

### Other

- fix testRadio11be unit test
- - do not sync all AP for each connect/disconnect

## Release sah-next_v7.17.7 - 2025-05-16(14:35:51 +0000)

### Other

- fix hostapd startup failure loop when disabling vap

## Release sah-next_v7.17.6 - 2025-05-15(13:24:59 +0000)

### Other

- Add ProbeRequest event with RSSI only

## Release sah-next_v7.17.5 - 2025-05-15(10:24:28 +0000)

### Other

- wrong MldMAC set with disabled linkMAC

## Release sah-next_v7.17.4 - 2025-05-14(16:22:14 +0000)

### Other

- : Fix documentation generation
- Add ProbeRequest event with RSSI only

## Release sah-next_v7.17.3 - 2025-05-14(08:41:44 +0000)

### Other

- preferred AP doesnt work

## Release sah-next_v7.17.2 - 2025-05-12(14:06:21 +0000)

### Other

- : WiFi process is hanging when ZWDFS is launched

## Release sah-next_v7.17.1 - 2025-05-08(10:00:29 +0000)

### Other

- Mode is back to compatibility after reboot of hgw

## Release sah-next_v7.17.0 - 2025-05-07(14:48:40 +0000)

### Other

- add dump info of internal functions

## Release sah-next_v7.16.6 - 2025-05-06(12:45:19 +0000)

### Other

- [import] Invalid BTM Target BSSID in Client Steering BTM Report message.

## Release sah-next_v7.16.5 - 2025-05-06(12:34:09 +0000)

### Other

- [import] Wildcard BSSID support in BTM request

## Release sah-next_v7.16.4 - 2025-05-06(11:00:10 +0000)

### Other

- [import] Freedom: WHM: hostapd missing interworking parameter

## Release sah-next_v7.16.3 - 2025-05-06(08:19:44 +0000)

### Other

- [import] Occasional Issue with Sending MgmtActionFrameReceived Notification.

## Release sah-next_v7.16.2 - 2025-04-28(08:55:34 +0000)

### Other

- : Noise is not reported

## Release sah-next_v7.16.1 - 2025-04-25(15:30:29 +0000)

### Other

- - Error prints on assoc / disassoc

## Release sah-next_v7.16.0 - 2025-04-24(15:34:34 +0000)

### Other

- - EP netdev index not correct.

## Release sah-next_v7.15.8 - 2025-04-24(11:23:18 +0000)

### Other

- WiFi Sensing phase 2a step2.1 - support vendor data types
- PPM-3041 WiFi Sensing phase2a step2.1 - HLAPI and DM final state

## Release sah-next_v7.15.7 - 2025-04-18(13:03:36 +0000)

### Other

- [Terminating dot][tr181] fix Wifi.EndPoint

## Release sah-next_v7.15.6 - 2025-04-18(08:13:24 +0000)

### Other

- wpactrlMngr fails to connect to all sockets

## Release sah-next_v7.15.5 - 2025-04-17(06:51:22 +0000)

### Other

- - update secure daemon group member state

## Release sah-next_v7.15.4 - 2025-04-14(16:28:50 +0000)

### Other

- - too many logs

## Release sah-next_v7.15.3 - 2025-04-14(08:59:47 +0000)

### Other

- - activate DISASSOC_IMMINENT flag.
- pwhm start command throwing 'command not found' error

## Release sah-next_v7.15.2 - 2025-04-11(08:56:40 +0000)

### Other

- unable to set Auto bandwidth after static bw value

## Release sah-next_v7.15.1 - 2025-04-10(17:53:54 +0000)

### Other

- : Update testcases for SurroundingChannels

## Release sah-next_v7.15.0 - 2025-04-10(09:03:21 +0000)

### Other

- - add MLD Status

## Release sah-next_v7.14.17 - 2025-04-09(15:25:45 +0000)

### Other

- improve hostapd restart conditions when configuring MLO

## Release sah-next_v7.14.16 - 2025-04-09(15:10:20 +0000)

### Other

- - hostap updated security configuration for Wi-Fi 7

## Release sah-next_v7.14.15 - 2025-04-09(14:58:41 +0000)

### Other

- fix bandwidth changed after chanspec being auto set

## Release sah-next_v7.14.14 - 2025-04-09(14:01:03 +0000)

### Other

- fix failing to update rad status after hostapd restart

## Release sah-next_v7.14.13 - 2025-04-09(13:45:20 +0000)

### Other

- fsm wait for mirrored ap/ep ssid enable

## Release sah-next_v7.14.12 - 2025-04-08(16:20:25 +0000)

### Other

- : Stations are not properly disconnected when hostapd is stopped

## Release sah-next_v7.14.11 - 2025-04-08(09:31:40 +0000)

## Release sah-next_v7.14.10 - 2025-04-07(14:17:43 +0000)

### Other

- - pwhm does not have time to terminate

## Release sah-next_v7.14.9 - 2025-04-07(09:17:51 +0000)

### Other

- "WiFi.AccessPoint." cannot be retrieved with MQTT

## Release sah-next_v7.14.8 - 2025-04-07(07:57:36 +0000)

### Other

- : wld is consuming 30 % of CPU

## Release sah-next_v7.14.7 - 2025-04-06(09:18:02 +0000)

### Other

- PPW 491 : Fix SSID update on 6GHz band

## Release sah-next_v7.14.6 - 2025-04-04(08:57:21 +0000)

### Other

- - delay asking for hostapd status

## Release sah-next_v7.14.5 - 2025-04-01(07:59:13 +0000)

### Other

- manage AP/EP enabling with SSID

## Release sah-next_v7.14.4 - 2025-03-26(07:55:39 +0000)

### Other

- fix missing init of glob hapd at startup

## Release sah-next_v7.14.3 - 2025-03-24(14:48:35 +0000)

### Other

- fix crash custom hostapd when restarted with standard conf

## Release sah-next_v7.14.2 - 2025-03-24(08:44:41 +0000)

### Other

- wds device not seen connected

## Release sah-next_v7.14.1 - 2025-03-20(15:27:28 +0000)

### Other

- - Mock time in wld_nl80211 unit test

## Release sah-next_v7.14.0 - 2025-03-19(17:31:55 +0000)

### Other

- Add a new parameter as ChipsetVendor to WiFi.Radio

## Release sah-next_v7.13.0 - 2025-03-19(09:23:50 +0000)

### Other

- : Implment mfn_wvap_clean_sta

## Release sah-next_v7.12.6 - 2025-03-17(09:06:25 +0000)

### Other

- prevent crash hostapd when re-enabling radio having APMLD links

## Release sah-next_v7.12.5 - 2025-03-13(14:44:10 +0000)

### Other

- remove default AP RetryLimit set in the code

## Release sah-next_v7.12.4 - 2025-03-11(08:24:20 +0000)

## Release sah-next_v7.12.3 - 2025-03-10(15:46:48 +0000)

### Other

- fix fetching AssocDev ctx for station moving out MLO

## Release sah-next_v7.12.2 - 2025-03-10(11:12:49 +0000)

### Other

- - crash PWHM

## Release sah-next_v7.12.1 - 2025-03-07(08:19:17 +0000)

### Other

- match only connected stations in APMLD station stats

## Release sah-next_v7.12.0 - 2025-03-06(16:07:55 +0000)

### Other

- : use hostapd probe_rx_events=1 to catch PROBE_REQ

## Release sah-next_v7.11.2 - 2025-03-04(16:23:20 +0000)

### Other

- - Endpoint connection is failing

## Release sah-next_v7.11.1 - 2025-02-28(12:45:37 +0000)

### Other

- : AssociatedDevice is not always created after a non-MLO association

## Release sah-next_v7.11.0 - 2025-02-27(14:36:45 +0000)

### Other

- Add WPA3 Compatibility Mode

## Release sah-next_v7.10.16 - 2025-02-27(13:41:06 +0000)

### Other

- - WiFi5 AutoChannel selection still enabled when we change the channel from AP WebUI

## Release sah-next_v7.10.15 - 2025-02-27(09:13:23 +0000)

### Other

- - Avoid large stack memory alloc.

## Release sah-next_v7.10.14 - 2025-02-26(15:06:10 +0000)

### Other

- - fix MultiAP not set for fronthaul.

## Release sah-next_v7.10.13 - 2025-02-26(13:36:59 +0000)

### Other

- : Fix dependency on kmod-mac80211

## Release sah-next_v7.10.12 - 2025-02-25(17:19:08 +0000)

### Other

- : EndPoint not always reconnecting

## Release sah-next_v7.10.11 - 2025-02-25(16:25:50 +0000)

### Other

- - Fix potential memory lost.

## Release sah-next_v7.10.10 - 2025-02-25(16:19:34 +0000)

### Other

- - increse sync nl80211 request timout

## Release sah-next_v7.10.9 - 2025-02-25(10:12:20 +0000)

### Other

- : Randomly: Hostapd crash seen after upgrade

## Release sah-next_v7.10.8 - 2025-02-24(15:43:07 +0000)

### Other

- WPS ConfigMethodsEnabled parameter is wrongly updated

## Release sah-next_v7.10.7 - 2025-02-24(14:49:39 +0000)

### Other

- : SSID.Enable is not sw update proof

## Release sah-next_v7.10.6 - 2025-02-23(10:35:56 +0000)

### Other

- - WPS Pin update in infite loop.

## Release sah-next_v7.10.5 - 2025-02-17(16:10:15 +0000)

### Other

- : WPS not restarted

## Release sah-next_v7.10.4 - 2025-02-14(13:44:20 +0000)

### Other

- - Radio up while not able to perform traffic.

## Release sah-next_v7.10.3 - 2025-02-13(11:25:42 +0000)

### Other

- Fix debug messages:

## Release sah-next_v7.10.2 - 2025-02-13(09:29:28 +0000)

### Other

- 40MHz HT Capabilities infos are not transmitted on beacon frame

## Release sah-next_v7.10.1 - 2025-02-11(11:51:37 +0000)

### Other

- 
- - Add new RPC to retrieve combined scan results, including access points and spectrum information

## Release sah-next_v7.10.0 - 2025-02-11(11:19:08 +0000)

### Other

- add wifi firmware version to debug info

## Release sah-next_v7.9.4 - 2025-02-11(11:02:44 +0000)

### Other

- - Fix simultaneous switch of channel and bandwidth

## Release sah-next_v7.9.3 - 2025-02-11(09:12:00 +0000)

### Other

- disable fth when bkh fails to connect while chan is aligned

## Release sah-next_v7.9.2 - 2025-02-09(18:48:11 +0000)

### Other

- Statically defined value (DE) is first set for Regulatory Domain used when PWHM is restarted, without the board reboot.

## Release sah-next_v7.9.1 - 2025-02-09(18:34:49 +0000)

### Other

- fix global hostapd crash when enabling APMLD link on second radio

## Release sah-next_v7.9.0 - 2025-02-07(15:56:25 +0000)

### Other

- : Add action frame listening for EndPoints

## Release sah-next_v7.8.7 - 2025-02-07(09:24:52 +0000)

### Other

- - stop a crashed daemon

## Release sah-next_v7.8.6 - 2025-02-04(09:38:53 +0000)

### Other

- - Unexpected interfaces created.

## Release sah-next_v7.8.5 - 2025-02-03(17:02:44 +0000)

### Other

- - Auto bandwidth selection does not work on channel 6/213

## Release sah-next_v7.8.4 - 2025-02-03(13:25:21 +0000)

### Other

- - update endpoint wpa event handler call

## Release sah-next_v7.8.3 - 2025-02-03(11:04:40 +0000)

### Other

- : 6GHz backhaul connection with fronthaul enabled is not possible

## Release sah-next_v7.8.2 - 2025-01-31(10:09:38 +0000)

### Other

- - add Endpoint debug

## Release sah-next_v7.8.1 - 2025-01-31(09:58:49 +0000)

### Other

- [OSPv2] Security mode configuration change is not working

## Release sah-next_v7.8.0 - 2025-01-30(14:29:53 +0000)

### Other

- add WdsInterfaceName param for 4mac associated devices

## Release sah-next_v7.7.0 - 2025-01-28(08:51:23 +0000)

### Other

- - Add endpoint MLO support

## Release sah-next_v7.6.1 - 2025-01-27(15:21:13 +0000)

### Other

- : AssociatedDevice entry is not always created

## Release sah-next_v7.6.0 - 2025-01-27(10:01:56 +0000)

### Other

- - add FSM default wait time debug function

## Release sah-next_v7.5.2 - 2025-01-27(09:49:38 +0000)

### Other

- Client fails to connect to an AP through WPS using PIN.

## Release sah-next_v7.5.1 - 2025-01-23(11:07:58 +0000)

### Other

- fix wld crash when early starting AP wps

## Release sah-next_v7.5.0 - 2025-01-22(17:05:36 +0000)

### Other

- fix clearing ep profileReference

## Release sah-next_v7.4.0 - 2025-01-17(17:27:37 +0000)

### Other

- - Provide custom arguments to hostapd

## Release sah-next_v7.3.2 - 2025-01-17(13:28:41 +0000)

### Other

- - Fix single instance hostapd restart bug

## Release sah-next_v7.3.1 - 2025-01-13(15:27:57 +0000)

### Other

- - Fix args parameter being altered in wld deamon struct

## Release sah-next_v7.3.0 - 2025-01-09(17:37:57 +0000)

### Other

- add wpactrl Iface/Conn apis to get server sockets directory path

## Release sah-next_v7.2.1 - 2025-01-09(15:16:36 +0000)

### Other

- - fix crash with invalid obj ref.

## Release sah-next_v7.2.0 - 2025-01-09(13:12:19 +0000)

### Other

- - Add generic set VAP netdevID fucntion

## Release sah-next_v7.1.12 - 2025-01-06(14:19:19 +0000)

## Release sah-next_v7.1.11 - 2024-12-20(12:22:29 +0000)

### Other

- [SSH] Make getDebug working with non-root ssh user.

## Release sah-next_v7.1.10 - 2024-12-19(15:44:32 +0000)

### Other

- - Fix hostapd toggle when higher action is scheduled

## Release sah-next_v7.1.9 - 2024-12-19(15:02:29 +0000)

### Other

- : Invalid RSSI in scanResults

## Release sah-next_v7.1.8 - 2024-12-19(13:24:32 +0000)

## Release sah-next_v7.1.7 - 2024-12-19(10:38:40 +0000)

### Other

- - fix unit test
- - match frame target agains all affiliated APs
- - introduce getMbssidTransmitter

## Release sah-next_v7.1.6 - 2024-12-14(09:11:47 +0000)

### Other

- : Second scan attempt is cancelling the first one

## Release sah-next_v7.1.5 - 2024-12-13(12:21:02 +0000)

### Other

- : SSID Change on Main VAP Causes Deauthentication on Both Main and Guest VAPs

## Release sah-next_v7.1.4 - 2024-12-13(11:36:53 +0000)

### Other

- - VAP Enable Toggling

## Release sah-next_v7.1.3 - 2024-12-11(17:47:53 +0000)

### Other

- : new SSID not applied

## Release sah-next_v7.1.2 - 2024-12-11(17:39:41 +0000)

### Other

- - disable bss 11be if not part of a MLO

## Release sah-next_v7.1.1 - 2024-12-11(10:32:14 +0000)

### Other

- fix hostapd crash when setting specific ssid/secConf on MLD link

## Release sah-next_v7.1.0 - 2024-12-09(16:44:14 +0000)

### Other

- : Add GetInfo vendor module function

## Release sah-next_v7.0.9 - 2024-12-04(14:48:06 +0000)

### Other

- - [Wi-Fi] [pwhm] ResetCounters do not reset FastReconnects and FastReconnectTypes. ones

## Release sah-next_v7.0.8 - 2024-12-04(12:25:10 +0000)

### Other

- fix hostapd restart when secondary bss is set as MLD main iface

## Release sah-next_v7.0.7 - 2024-12-04(12:04:50 +0000)

### Other

- fix setChanspec failed when tried chspec has previously timeouted

## Release sah-next_v7.0.6 - 2024-12-04(07:50:40 +0000)

### Other

- SSID Change on Main VAP Causes Deauthentication on Both Main and Guest VAPs

## Release sah-next_v7.0.5 - 2024-12-02(14:32:19 +0000)

### Other

- fix invalid mac autogen when BaseMacOffset and NbRequiredBss are configured

## Release sah-next_v7.0.4 - 2024-11-29(08:32:40 +0000)

### Other

- : Index is not updated when interface are disabled at boot

## Release sah-next_v7.0.3 - 2024-11-28(18:12:42 +0000)

### Other

- cherry pick from mainline to stable
- : 2.4GHz priv cannot be reenabled

## Release sah-next_v7.0.2 - 2024-11-28(15:09:34 +0000)

### Other

- - EP fails to connect after WPS

## Release sah-next_v7.0.1 - 2024-11-28(09:59:52 +0000)

### Other

- [WiFi]"OperatingChannelBandwidth" No more reboot persistent

## Release sah-next_v7.0.0 - 2024-11-27(17:15:43 +0000)

### Other

- PPM-2984 add handler for custom reload of sec daemon

## Release sah-next_v6.44.17 - 2024-11-27(11:01:05 +0000)

### Other

- deploy WPA2-WPA3-Personal on 2.4GHz and 5GHz

## Release sah-next_v6.44.16 - 2024-11-25(09:59:49 +0000)

### Other

- fix updating next radios Mac and MbssBaseMac when adding vaps

## Release sah-next_v6.44.15 - 2024-11-22(20:37:16 +0000)

### Other

- fix mac duplication when having 6ghz endpoint and multi vaps

## Release sah-next_v6.44.14 - 2024-11-21(14:52:02 +0000)

### Other

- : Wrong SSID broadcasted

## Release sah-next_v6.44.13 - 2024-11-21(09:03:48 +0000)

### Other

- : SSID Index not reported anymore

## Release sah-next_v6.44.12 - 2024-11-20(16:31:24 +0000)

### Other

- fix secondary vap enabling with radio reconfig

## Release sah-next_v6.44.11 - 2024-11-20(12:35:44 +0000)

### Other

- - double commit at boot

## Release sah-next_v6.44.10 - 2024-11-20(08:13:25 +0000)

### Other

- fix amx events no more sent over amx

## Release sah-next_v6.44.9 - 2024-11-20(07:48:04 +0000)

### Other

- apply WPS_Cancel command to only VAPs with WPS activated

## Release sah-next_v6.44.8 - 2024-11-19(17:17:03 +0000)

### Other

- PPW-143 fix rad hapd iface remaining down after enabling vap

## Release sah-next_v6.44.7 - 2024-11-18(09:27:20 +0000)

### Other

- fix crash when calling detached AP rpc getFarAssociatedDevicesCount

## Release sah-next_v6.44.6 - 2024-11-15(16:34:58 +0000)

### Other

- - [PWHM] Init script restart action unsafe for processmonitor

## Release sah-next_v6.44.5 - 2024-11-15(14:02:14 +0000)

### Other

- : Increase security mode after a WPS session

## Release sah-next_v6.44.4 - 2024-11-15(13:53:20 +0000)

### Other

- create/delete ep iface when required

## Release sah-next_v6.44.3 - 2024-11-14(09:10:50 +0000)

## Release sah-next_v6.44.2 - 2024-11-13(15:34:41 +0000)

## Release sah-next_v6.44.1 - 2024-11-13(14:14:32 +0000)

### Other

- - pWHM with external configation slow to start

## Release sah-next_v6.44.0 - 2024-11-13(11:41:25 +0000)

### Other

- - Update MLO mode and afSta creation

## Release sah-next_v6.43.1 - 2024-11-12(16:21:43 +0000)

### Other

- : After toggling EndPoint Enable, wpa_supp is not started

## Release sah-next_v6.43.0 - 2024-11-12(12:04:53 +0000)

### Other

- use wpa_supp RECONFIGURE cmd isof SIGHUP to reload conf

## Release sah-next_v6.42.6 - 2024-11-12(10:40:35 +0000)

### Other

- add apis for external calling of wpactrl mgr/iface handlers

## Release sah-next_v6.42.5 - 2024-11-12(09:00:41 +0000)

## Release sah-next_v6.42.4 - 2024-11-08(17:24:05 +0000)

### Other

- : wpa_supp report scan_result from all frequencies

## Release sah-next_v6.42.3 - 2024-11-08(17:12:49 +0000)

### Other

- PPW-9 improve fix of successive ssid ap objs deletion

## Release sah-next_v6.42.2 - 2024-11-08(17:01:51 +0000)

### Other

- fix crash when running successive rad scan with single wiphy

## Release sah-next_v6.42.1 - 2024-11-08(16:16:25 +0000)

### Other

- : pwhm crash at boot in ethernet backhaul

## Release sah-next_v6.42.0 - 2024-11-08(13:07:33 +0000)

### Other

- Reduce regex usage in signal handling
- - add Wiphy name into radio ctx.

## Release sah-next_v6.41.12 - 2024-11-07(15:26:04 +0000)

### Other

- Reduce regex usage in signal handling

## Release sah-next_v6.41.11 - 2024-11-07(13:04:42 +0000)

### Other

- - accept disabling AcsBootChannel

## Release sah-next_v6.41.10 - 2024-11-07(10:46:41 +0000)

### Other

- - [Freedom] addVAPIntf failed with status 13 - duplicate

## Release sah-next_v6.41.9 - 2024-11-04(14:35:35 +0000)

### Other

- : Log spam

## Release sah-next_v6.41.8 - 2024-11-04(13:10:55 +0000)

### Other

- fix disassoc notif missing reason

## Release sah-next_v6.41.7 - 2024-11-04(06:09:22 +0000)

### Other

- getting-wld-segfault-in-stability-test

## Release sah-next_v6.41.6 - 2024-10-29(16:00:43 +0000)

### Other

- - AfSta packetsReceived shown as errors.

## Release sah-next_v6.41.5 - 2024-10-29(08:26:17 +0000)

### Other

- Received error during boot

## Release sah-next_v6.41.4 - 2024-10-24(15:04:57 +0000)

### Other

- : NaStaMonitor BSSID issue

## Release sah-next_v6.41.3 - 2024-10-24(14:28:23 +0000)

### Other

- PPW-9 fix successive ssid ap objs deletion

## Release sah-next_v6.41.2 - 2024-10-21(14:15:36 +0000)

### Other

- set default ModesAvailable conf for 6GHz to "WPA3-Personal,OWE"

## Release sah-next_v6.41.1 - 2024-10-21(13:19:04 +0000)

### Other

- mxl-station-not-able-to-connect-in-wpa2-enterprise-mode

## Release sah-next_v6.41.0 - 2024-10-18(08:10:33 +0000)

### Other

- auto fill radio wiphyId from networklayout

## Release sah-next_v6.40.2 - 2024-10-14(13:17:55 +0000)

## Release sah-next_v6.40.1 - 2024-10-14(11:52:13 +0000)

### Other

- : User config chanspec is applied while in AutoChanneEnable=1

## Release sah-next_v6.40.0 - 2024-10-14(10:12:06 +0000)

### Other

- : EHT Capabilities of connected stations

## Release sah-next_v6.39.11 - 2024-10-11(16:31:38 +0000)

### Other

- fix unit test to match new realTime display

## Release sah-next_v6.39.10 - 2024-10-11(12:30:53 +0000)

### Other

- fix getting param int value from hostapd conf file

## Release sah-next_v6.39.9 - 2024-10-11(09:09:05 +0000)

### Other

- fix unit test to match new realTime display

## Release sah-next_v6.39.8 - 2024-10-10(13:08:15 +0000)

### Other

- : Group key value under networks not preserved during firmware upgrade

## Release sah-next_v6.39.7 - 2024-10-10(07:11:38 +0000)

### Other

- Hostapd crash upon launching a WPS session

## Release sah-next_v6.39.6 - 2024-10-09(14:10:02 +0000)

### Other

- : Changed values of the bands on Radio Management are not retained after Firmware Upgrade

## Release sah-next_v6.39.5 - 2024-10-07(14:43:10 +0000)

### Other

- Add FTA handler to control scan iteration by external manager
- map multi radio instances of same Frequency Band using nl80211 HW wiphyId

## Release sah-next_v6.39.4 - 2024-10-04(11:58:40 +0000)

### Other

- Add enable flush scan argument

## Release sah-next_v6.39.3 - 2024-10-03(16:02:29 +0000)

### Other

- Radio operatingChannelBandwidth is not restored

## Release sah-next_v6.39.2 - 2024-10-03(07:49:17 +0000)

## Release sah-next_v6.39.1 - 2024-10-01(07:39:09 +0000)

## Release sah-next_v6.39.0 - 2024-09-27(16:12:22 +0000)

### Other

- - Adding a handler to filter the incoming scan requests.

## Release sah-next_v6.38.0 - 2024-09-27(13:56:55 +0000)

### Other

- : ScanResults update enabling

## Release sah-next_v6.37.5 - 2024-09-26(13:22:26 +0000)

### Other

- - Do not rewrite profile reference

## Release sah-next_v6.37.4 - 2024-09-25(09:38:49 +0000)

### Other

- - Allow profile Alias to be used as profile reference

## Release sah-next_v6.37.3 - 2024-09-24(16:33:14 +0000)

### Other

- : Fix ExternalAcsMgmt value not loaded on reboot

## Release sah-next_v6.37.2 - 2024-09-18(11:28:41 +0000)

### Other

- fix default supported channel

## Release sah-next_v6.37.1 - 2024-09-18(11:15:20 +0000)

### Other

- fix initial currChanpec of restarted hostapd

## Release sah-next_v6.37.0 - 2024-09-17(14:47:54 +0000)

## Release sah-next_v6.36.8 - 2024-09-17(14:30:43 +0000)

### Other

- - Scan results OUI list not cleaned

## Release sah-next_v6.36.7 - 2024-09-17(13:35:38 +0000)

### Other

- - Failed to generate documentation for component "services_pwhm"

## Release sah-next_v6.36.6 - 2024-09-17(12:26:53 +0000)

### Other

- sometimes missing scanResults dm entries

## Release sah-next_v6.36.5 - 2024-09-13(18:08:03 +0000)

### Other

- Support of socket in pwhm

## Release sah-next_v6.36.4 - 2024-09-13(11:26:53 +0000)

## Release sah-next_v6.36.3 - 2024-09-13(09:52:24 +0000)

### Other

- mxl-allowing-vap-addition-more-than-permissible-limit

## Release sah-next_v6.36.1 - 2024-09-10(13:50:21 +0000)

### Other

- fix overlapping fsm commits

## Release sah-next_v6.36.0 - 2024-09-07(15:07:33 +0000)

### Other

- - Minor fixes in VHTCapabilities

## Release sah-next_v6.35.3 - 2024-09-06(14:05:25 +0000)

### Other

- Changed values of the bands on Radio Management are not retained after Firmware Upgrade

## Release sah-next_v6.35.2 - 2024-09-06(10:40:30 +0000)

### Other

- mxl-getting-invalid-number-of-mapped-bss

## Release sah-next_v6.35.1 - 2024-09-05(14:05:51 +0000)

### Other

- : Add bssid parameter for NaSta

## Release sah-next_v6.35.0 - 2024-09-05(12:40:30 +0000)

### Other

- : Add bssid parameter for NaSta

## Release sah-next_v6.34.1 - 2024-09-05(06:41:22 +0000)

### Other

- : [Terminating dot][tr181] fix Wifi

## Release sah-next_v6.34.0 - 2024-09-03(15:10:51 +0000)

## Release sah-next_v6.33.1 - 2024-08-30(10:42:44 +0000)

## Release sah-next_v6.33.0 - 2024-08-29(07:30:05 +0000)

### Other

- - Add frequency to affiliated sta

## Release sah-next_v6.32.0 - 2024-08-28(17:35:08 +0000)

### Other

- - allow more HE capabities and default option

## Release sah-next_v6.31.5 - 2024-08-28(15:22:08 +0000)

### Other

- [prplos 23.05][MxL] Import default pwhm endpoint odl files from prplos v22.03

## Release sah-next_v6.31.4 - 2024-08-28(15:11:37 +0000)

### Other

- [prplos 23.05][MxL] Import default pwhm endpoint odl files from prplos v22.03

## Release sah-next_v6.31.3 - 2024-08-28(11:41:19 +0000)

### Other

- Fix RadioAirStats wrong values issue

## Release sah-next_v6.31.2 - 2024-08-27(12:20:55 +0000)

### Other

- : Primary wifi password is not upgrade persistent

## Release sah-next_v6.31.1 - 2024-08-23(15:16:07 +0000)

### Other

- : 2.4GHz radio is reconfiguring itself in loop

## Release sah-next_v6.31.0 - 2024-08-23(12:16:44 +0000)

### Other

- - provide function to start delay commit timer

## Release sah-next_v6.30.6 - 2024-08-19(08:13:40 +0000)

### Other

- - Accept empty string for passphrases

## Release sah-next_v6.30.5 - 2024-08-07(09:17:52 +0000)

## Release sah-next_v6.30.4 - 2024-08-05(14:13:39 +0000)

### Other

- ChannelLoad is not updated with pcb

## Release sah-next_v6.30.3 - 2024-08-05(12:11:35 +0000)

### Other

- Set Reporting Detail subelement to default value

## Release sah-next_v6.30.2 - 2024-08-05(11:54:33 +0000)

### Other

- - Fix Enable sync between Endpoint/Accesspoint and SSID.

## Release sah-next_v6.30.1 - 2024-07-31(12:28:05 +0000)

### Other

- - fix SSID sync looping

## Release sah-next_v6.30.0 - 2024-07-26(14:59:08 +0000)

### Other

- Implementation of fullscan function.

## Release sah-next_v6.29.10 - 2024-07-24(10:50:38 +0000)

## Release sah-next_v6.29.9 - 2024-07-24(06:49:16 +0000)

### Other

- : Access point edited names not restored

## Release sah-next_v6.29.8 - 2024-07-23(08:24:47 +0000)

### Other

- sync issue when ProfileReference set before profile

## Release sah-next_v6.29.7 - 2024-07-17(15:18:25 +0000)

### Other

- Debug error spam

## Release sah-next_v6.29.6 - 2024-07-17(12:45:16 +0000)

## Release sah-next_v6.29.5 - 2024-07-17(07:24:09 +0000)

### Other

- - wpa control socket never closed

## Release sah-next_v6.29.4 - 2024-07-16(08:56:09 +0000)

### Other

- : Add pcb socket in pwhm

## Release sah-next_v6.29.3 - 2024-07-16(07:13:34 +0000)

### Other

- - Operating standard not updated after device reassoc

## Release sah-next_v6.29.2 - 2024-07-15(09:51:56 +0000)

### Other

- Neighbours are not instantiated

## Release sah-next_v6.29.1 - 2024-07-12(08:57:33 +0000)

### Other

- manage detecting single wiphy with no wl ifaces

## Release sah-next_v6.29.0 - 2024-07-11(12:34:55 +0000)

### Other

- detect nl80211 interface mld links

## Release sah-next_v6.28.23 - 2024-07-11(09:05:12 +0000)

### Other

- improve fetch target radio for nl80211 notif

## Release sah-next_v6.28.22 - 2024-07-10(14:08:55 +0000)

### Other

- - MACFilterAddressList modification does not work

## Release sah-next_v6.28.21 - 2024-07-10(13:54:36 +0000)

### Other

- - interface creation handler not called

## Release sah-next_v6.28.20 - 2024-07-10(09:41:50 +0000)

### Other

- - add FSM unit test and some simple statistics

## Release sah-next_v6.28.19 - 2024-07-09(15:40:10 +0000)

### Other

- - WiFi bits not executed

## Release sah-next_v6.28.18 - 2024-07-08(14:26:33 +0000)

### Other

- - Extra radio in data model

## Release sah-next_v6.28.17 - 2024-07-08(13:30:34 +0000)

### Other

- - WiFi FSM toggling of Vendor FSM

## Release sah-next_v6.28.16 - 2024-07-08(10:45:17 +0000)

### Other

- restore gitlab-ci

## Release sah-next_v6.28.15 - 2024-07-08(08:58:24 +0000)

## Release sah-next_v6.28.14 - 2024-07-08(08:26:21 +0000)

### Other

- - Parameters not properly set through persistance layer.

## Release sah-next_v6.28.13 - 2024-07-04(15:52:17 +0000)

### Other

- - External observers not seeing destroy changes

## Release sah-next_v6.28.12 - 2024-07-04(08:29:16 +0000)

### Other

- - unable to scan radio

## Release sah-next_v6.28.11 - 2024-07-03(13:18:15 +0000)

### Other

- - Clean up the private feature get Rssi per antenna.

## Release sah-next_v6.28.10 - 2024-07-03(11:43:10 +0000)

### Other

- - Ep connection issues with WPA3 and WPA2-WPA3 security modes

## Release sah-next_v6.28.9 - 2024-07-03(07:44:13 +0000)

### Other

- start hostapd/wpa_supp at the end of FSM

## Release sah-next_v6.28.8 - 2024-07-01(16:37:41 +0000)

### Other

- wait for wiphy device loading on boot

## Release sah-next_v6.28.7 - 2024-07-01(09:01:16 +0000)

### Other

- manage multi band scan in single wiphy mode

## Release sah-next_v6.28.6 - 2024-06-30(07:52:44 +0000)

### Other

- - delete old wds before using new one.

## Release sah-next_v6.28.5 - 2024-06-27(17:12:30 +0000)

### Other

- - Cannot restart wifi plugin

## Release sah-next_v6.28.4 - 2024-06-27(16:09:37 +0000)

### Other

- : Crash on hostapd process after reboot

## Release sah-next_v6.28.3 - 2024-06-27(09:25:40 +0000)

### Other

- detect bg cac abort event from nl80211

## Release sah-next_v6.28.2 - 2024-06-27(08:38:36 +0000)

### Other

- - update endpoint profile status
- temporary enable 11be with no MLO

## Release sah-next_v6.28.1 - 2024-06-26(14:24:15 +0000)

### Other

- : SSIDs are not broadcasted

## Release sah-next_v6.28.0 - 2024-06-26(07:57:11 +0000)

### Other

- - [pwhm] expose a scanInProgress param in DM

## Release sah-next_v6.27.9 - 2024-06-25(15:14:31 +0000)

### Other

- - wld start error

## Release sah-next_v6.27.8 - 2024-06-25(08:51:00 +0000)

## Release sah-next_v6.27.7 - 2024-06-25(07:48:19 +0000)

### Other

- detect bg cac starting event from nl80211

## Release sah-next_v6.27.6 - 2024-06-24(12:42:24 +0000)

### Other

- - Handle register mgmt frame command reply

## Release sah-next_v6.27.5 - 2024-06-24(09:23:42 +0000)

### Other

- manage radio air stats for each band with single wiphy

## Release sah-next_v6.27.4 - 2024-06-24(08:12:39 +0000)

### Other

- - fix fta return value

## Release sah-next_v6.27.3 - 2024-06-24(07:32:38 +0000)

### Other

- fix radio baseMacAddress duplication

## Release sah-next_v6.27.2 - 2024-06-21(15:17:03 +0000)

### Other

- fasten initial wpactrl connection

## Release sah-next_v6.27.1 - 2024-06-21(15:07:47 +0000)

### Other

- fix auto generated mac overlap

## Release sah-next_v6.27.0 - 2024-06-21(12:31:04 +0000)

### Other

- - Add debug of channel

## Release sah-next_v6.26.14 - 2024-06-21(07:52:29 +0000)

### Other

- : fix crash when displaying errno string

## Release sah-next_v6.26.13 - 2024-06-20(16:48:01 +0000)

### Other

- enable amx doc check and disable pcb doc check
- Map WiFi. to Device.WiFi.

## Release sah-next_v6.26.12 - 2024-06-20(09:06:11 +0000)

### Other

- : ProbeResponse not seen

## Release sah-next_v6.26.11 - 2024-06-19(15:59:05 +0000)

### Other

- - no index for AP

## Release sah-next_v6.26.10 - 2024-06-19(12:37:05 +0000)

## Release sah-next_v6.26.9 - 2024-06-19(11:17:42 +0000)

### Other

- -RecentTxBytes/RecentRxBytes_values_way_to_large_to_be_possible

## Release sah-next_v6.26.8 - 2024-06-19(11:09:00 +0000)

### Other

- - Adjust the fronthaul channel configuration upon backhaul connection

## Release sah-next_v6.26.7 - 2024-06-19(10:43:03 +0000)

## Release sah-next_v6.26.6 - 2024-06-18(07:54:47 +0000)

### Other

- : 320MHz bandwidth changes is not working

## Release sah-next_v6.26.5 - 2024-06-18(07:45:52 +0000)

### Other

- ignore invalid supp rad standards of multiband wiphy

## Release sah-next_v6.26.4 - 2024-06-18(07:24:15 +0000)

### Other

- - Endpoint Profile Alias is empty

## Release sah-next_v6.26.3 - 2024-06-17(16:04:29 +0000)

### Other

- fix default 6ghz vap wpactrl connection

## Release sah-next_v6.26.2 - 2024-06-17(08:32:20 +0000)

### Other

- add multiple BSSID advertisement support in IEEE 802.11ax

## Release sah-next_v6.26.1 - 2024-06-14(13:44:20 +0000)

### Other

- - fix missing check for pending actions

## Release sah-next_v6.26.0 - 2024-06-14(08:25:28 +0000)

### Other

- add gen impl of FTA addVapIf

## Release sah-next_v6.25.2 - 2024-06-13(13:56:10 +0000)

## Release sah-next_v6.25.1 - 2024-06-13(13:41:20 +0000)

### Other

- - fix reset pending FSM actions

## Release sah-next_v6.25.0 - 2024-06-13(13:29:29 +0000)

### Other

- - Support station inactivity timeout

## Release sah-next_v6.24.7 - 2024-06-12(14:34:53 +0000)

### Other

- -[Wi-Fi] WEP modes are not supported but in ModesSupported list

## Release sah-next_v6.24.6 - 2024-06-11(12:06:06 +0000)

### Other

- wrong operating class 0 in pwhm

## Release sah-next_v6.24.5 - 2024-06-10(13:43:42 +0000)

### Other

- - AffiliatedSta not present in higher level data model

## Release sah-next_v6.24.4 - 2024-06-10(09:54:59 +0000)

### Other

- - neighbour discovery value not set properly

## Release sah-next_v6.24.3 - 2024-06-10(09:28:12 +0000)

## Release sah-next_v6.24.2 - 2024-06-10(06:31:19 +0000)

### Other

- - Update default odl file genetation to enable radio STA mode

## Release sah-next_v6.24.1 - 2024-06-05(10:04:51 +0000)

### Other

- Steering is not working

## Release sah-next_v6.24.0 - 2024-06-05(09:24:16 +0000)

### Other

- - Update ODL with missing Wifi7/MLO stats

## Release sah-next_v6.23.0 - 2024-06-03(12:09:53 +0000)

### Other

- - add MLO Mode for associated device

## Release sah-next_v6.22.2 - 2024-05-31(12:08:50 +0000)

### Other

- check optional ssid arg in rrm request command

## Release sah-next_v6.22.1 - 2024-05-31(11:57:01 +0000)

### Other

- : MACFilterAddressList is not synced properly

## Release sah-next_v6.22.0 - 2024-05-31(08:46:15 +0000)

### Other

- - add optional parameter to sendRemoteMeasumentRequest

## Release sah-next_v6.21.2 - 2024-05-29(18:27:26 +0000)

### Other

- Admin cannot access to WiFi.AccessPoint. missing config in json file

## Release sah-next_v6.21.1 - 2024-05-29(12:29:13 +0000)

### Other

- : Bandwidth move to 20MHz when AutoChannelEnable=0

## Release sah-next_v6.21.0 - 2024-05-28(15:29:17 +0000)

### Other

- - Manage received and transmitted Deauth/Disassoc management frames

## Release sah-next_v6.20.5 - 2024-05-27(14:22:28 +0000)

### Other

- - VAP stats do not include WDS

## Release sah-next_v6.20.4 - 2024-05-27(13:47:34 +0000)

### Other

- - 6Ghz radio dm sync not working

## Release sah-next_v6.20.3 - 2024-05-27(10:04:29 +0000)

### Other

- : Sometimes 6GHz assoc is failing

## Release sah-next_v6.20.2 - 2024-05-24(13:35:14 +0000)

### Other

- - MaxAssociatedDevices on radio is not working

## Release sah-next_v6.20.1 - 2024-05-24(12:20:00 +0000)

## Release sah-next_v6.20.0 - 2024-05-24(11:49:50 +0000)

### Other

- - Handle the transmitted management frames status event

## Release sah-next_v6.19.1 - 2024-05-24(07:44:47 +0000)

### Other

- add nl80211 vap/ep iface using rad wiphyid iso ifIdx

## Release sah-next_v6.19.0 - 2024-05-23(18:13:41 +0000)

### Other

- - add wds queue and events

## Release sah-next_v6.18.1 - 2024-05-23(15:36:40 +0000)

### Other

- - Set RekeyingInterval default value to 3600 under AccessPoint.Security

## Release sah-next_v6.18.0 - 2024-05-23(09:09:57 +0000)

### Other

- - add EHT/80211BE support detection

## Release sah-next_v6.17.0 - 2024-05-22(10:26:30 +0000)

### Other

- add missing default radio ifaces when loading wiphy

## Release sah-next_v6.16.3 - 2024-05-21(12:22:28 +0000)

### Other

- allow using zwdfs fsm with vendor bgdfs implentation

## Release sah-next_v6.16.2 - 2024-05-20(13:22:59 +0000)

## Release sah-next_v6.16.1 - 2024-05-20(13:13:35 +0000)

### Other

- Admin cannot access to Device.WiFi.Radio.*.SupportedOperatingChannelBandwidth

## Release sah-next_v6.16.0 - 2024-05-17(09:02:58 +0000)

### Other

- - add extra MLD fields and some mld support

## Release sah-next_v6.15.1 - 2024-05-17(06:36:32 +0000)

### Other

- missing connection info and stats of a connected endpoint

## Release sah-next_v6.15.0 - 2024-05-16(09:39:48 +0000)

### Other

- - Add seamless DFS support based on generic implementation of ZW DFS fta start/stop

## Release sah-next_v6.14.0 - 2024-05-15(10:19:59 +0000)

### Other

- : Add OffChannelSupport read-only parameter

## Release sah-next_v6.13.1 - 2024-05-14(17:13:36 +0000)

## Release sah-next_v6.13.0 - 2024-05-14(16:30:23 +0000)

### Other

- - handle WDS station

## Release sah-next_v6.12.0 - 2024-05-13(15:37:41 +0000)

### Other

- - add generated file to gitIgnore
- - Add radio PacketAggregationEnable parameter

## Release sah-next_v6.11.6 - 2024-04-29(12:58:45 +0000)

### Other

- - add missing standard 'ax'

## Release sah-next_v6.11.5 - 2024-04-26(15:47:37 +0000)

### Other

- Adding upc flag to endpoint profileReference

## Release sah-next_v6.11.4 - 2024-04-26(12:23:09 +0000)

### Other

- - WPS Configuration status

## Release sah-next_v6.11.3 - 2024-04-26(12:01:45 +0000)

### Other

- Admin cannot access to WiFi.AccessPoint. missing config in json file

## Release sah-next_v6.11.2 - 2024-04-26(10:53:03 +0000)

### Other

- disable AutoChannelEnable by default

## Release sah-next_v6.11.1 - 2024-04-25(13:57:43 +0000)

### Other

- cannot set security mode WEP-64 on vap guest 2.4GHz

## Release sah-next_v6.11.0 - 2024-04-24(07:42:37 +0000)

### Other

- : Need data model parameter to set Description under different Network connections

## Release sah-next_v6.10.1 - 2024-04-23(16:32:45 +0000)

### Other

- wld_gen debug command is not working

## Release sah-next_v6.10.0 - 2024-04-23(09:20:50 +0000)

### Other

- - Add vap CpeOperationMode parameter

## Release sah-next_v6.9.7 - 2024-04-23(08:57:50 +0000)

### Other

- cannot switch to 40MHz - fallback in 20MHz

## Release sah-next_v6.9.6 - 2024-04-22(13:51:00 +0000)

### Other

- - supported standards do not match actual standards

## Release sah-next_v6.9.5 - 2024-04-22(09:27:21 +0000)

## Release sah-next_v6.9.4 - 2024-04-22(09:13:15 +0000)

### Other

- fix wpa_supp startup issue

## Release sah-next_v6.9.3 - 2024-04-22(08:25:31 +0000)

### Other

- refresh EP connStatus when wpa_supplicant mgr is ready

## Release sah-next_v6.9.2 - 2024-04-19(11:49:59 +0000)

### Other

- - Fix compilation issue with lib wld

## Release sah-next_v6.9.1 - 2024-04-18(15:34:06 +0000)

### Other

- fix new vap ifname checking conditions

## Release sah-next_v6.9.0 - 2024-04-16(15:18:16 +0000)

### Other

- add generic implementation of BG DFS start/stop APIs

## Release sah-next_v6.8.2 - 2024-04-16(13:32:40 +0000)

### Other

- : Unable to Connect to 6GHz Band with Default KeyPassPhrase

## Release sah-next_v6.8.1 - 2024-04-16(11:41:01 +0000)

### Other

- set 4mac flag on new custom named EP iface

## Release sah-next_v6.8.0 - 2024-04-15(10:42:07 +0000)

### Other

- - Add data transmit rates configuration

## Release sah-next_v6.7.0 - 2024-04-15(07:04:32 +0000)

### Other

- - Prevent enabling multiple ACS entities simultaneously

## Release sah-next_v6.6.1 - 2024-04-14(12:16:57 +0000)

### Other

- [IB5][WIFI] Unknown system error: WiFi.Radio.wifi0 and Unknown system error: WiFi.Radio.wifi1 when trying to set setWLANConfig

## Release sah-next_v6.6.0 - 2024-04-12(17:53:55 +0000)

### Other

- set VAPs/EPs CustomNetDevName when created with RPCs

## Release sah-next_v6.5.0 - 2024-04-12(16:57:23 +0000)

### Other

- support setting custom iface name for EPs

## Release sah-next_v6.4.0 - 2024-04-12(08:34:56 +0000)

### Other

- support setting custom iface name for VAPs/EPs

## Release sah-next_v6.3.4 - 2024-04-11(07:39:19 +0000)

### Other

- : No Probe Request seen for repeaters

## Release sah-next_v6.3.3 - 2024-04-09(16:00:48 +0000)

### Changes

- Make amxb timeouts configurable

## Release sah-next_v6.3.2 - 2024-04-09(08:57:51 +0000)

### Other

- use autoBwMode MaxCleared for rad 5GHz

## Release sah-next_v6.3.1 - 2024-04-08(15:05:17 +0000)

### Other

- don't return error when no Nasta monitor entry is present

## Release sah-next_v6.3.0 - 2024-04-05(15:20:03 +0000)

### Other

- - Add SSID LastChange parameter

## Release sah-next_v6.2.0 - 2024-04-05(13:12:11 +0000)

### Other

- - Add supported WPS version

## Release sah-next_v6.1.2 - 2024-04-05(12:59:20 +0000)

### Other

- : WiFi 2.4/5 is down after reboot

## Release sah-next_v6.1.1 - 2024-04-05(10:56:30 +0000)

### Other

- remove log spam during updating monitor stats

## Release sah-next_v6.1.0 - 2024-04-04(17:08:16 +0000)

### Other

- use nl80211.h deployed by kernel isof toolchain

## Release sah-next_v6.0.0 - 2024-04-04(16:16:20 +0000)

### Other

- add single hostapd apiDocs and set it Off by default

## Release sah-next_v5.47.1 - 2024-04-02(13:18:08 +0000)

### Other

- add support for single hostapd

## Release sah-next_v5.47.0 - 2024-03-29(14:07:15 +0000)

## Release sah-next_v5.46.2 - 2024-03-29(13:59:05 +0000)

### Other

- - Add Associated device statistics for retransmitted packets

## Release sah-next_v5.46.1 - 2024-03-29(11:45:43 +0000)

### Fixes

- [tr181-pcm] Saved and defaults odl should not be included in the backup files

## Release sah-next_v5.46.0 - 2024-03-29(08:01:08 +0000)

### Other

- [WiFi Sensing] Add new util function

## Release sah-next_v5.45.2 - 2024-03-28(16:00:44 +0000)

### Other

- getSpectrumInfo with update does not work

## Release sah-next_v5.45.1 - 2024-03-28(11:34:49 +0000)

### Other

- : No ProbeRequest sent

## Release sah-next_v5.45.0 - 2024-03-27(14:46:58 +0000)

## Release sah-next_v5.44.2 - 2024-03-27(10:08:51 +0000)

### Other

- : client cannot authenticate to vap 2.4GHz / 5GHz in WPA2-WPA3

## Release sah-next_v5.44.1 - 2024-03-26(16:02:19 +0000)

### Other

- fix autoMac calc to comply with 11ax MultiBSSID spec

## Release sah-next_v5.44.0 - 2024-03-25(11:45:21 +0000)

### Other

- - Add wps event for state change

## Release sah-next_v5.43.0 - 2024-03-21(14:00:25 +0000)

## Release sah-next_v5.42.10 - 2024-03-21(11:33:33 +0000)

## Release sah-next_v5.42.9 - 2024-03-20(16:03:15 +0000)

### Other

- : client cannot authenticate to vap 2.4GHz / 5GHz in WPA2-WPA3

## Release sah-next_v5.42.8 - 2024-03-18(15:31:26 +0000)

### Other

- - Enable BgDfs by default for 5GHz radio

## Release sah-next_v5.42.7 - 2024-03-15(08:08:56 +0000)

### Other

- NaStaMonitor does not report any RSSI values

## Release sah-next_v5.42.6 - 2024-03-15(07:39:36 +0000)

### Other

- wrong operating class 0 in pwhm

## Release sah-next_v5.42.5 - 2024-03-14(10:04:38 +0000)

### Other

- - Revert "add eth_link flag."

## Release sah-next_v5.42.4 - 2024-03-13(15:36:39 +0000)

### Other

- : Log spam

## Release sah-next_v5.42.3 - 2024-03-13(15:11:32 +0000)

### Other

- : WMMEnable parameter is not upgrade persistent

## Release sah-next_v5.42.2 - 2024-03-13(09:46:22 +0000)

### Other

- fix typo wpa key_mgmt WPA-EAP-256

## Release sah-next_v5.42.1 - 2024-03-13(08:25:07 +0000)

### Other

- add SHA256 in RSN AKM when PMF is enabled

## Release sah-next_v5.42.0 - 2024-03-08(16:54:38 +0000)

### Other

- : Add setRelayCredentials function

## Release sah-next_v5.41.0 - 2024-03-08(15:15:06 +0000)

### Other

- - add wifi 7 affiliatedState

## Release sah-next_v5.40.4 - 2024-03-07(16:29:51 +0000)

## Release sah-next_v5.40.3 - 2024-03-06(10:25:09 +0000)

### Other

- : scanCombined crash

## Release sah-next_v5.40.2 - 2024-03-05(15:28:45 +0000)

### Other

- - certification: 4.6.2_ETH_FH5GL: getLastAssocReq returns not-the-last-assoc-req

## Release sah-next_v5.40.1 - 2024-03-05(09:51:10 +0000)

### Other

- Fixed memory leak in pwhm leading to crash

## Release sah-next_v5.40.0 - 2024-03-04(15:49:41 +0000)

### Other

- - add modStart and modStop calls

## Release sah-next_v5.39.2 - 2024-03-01(15:50:51 +0000)

### Other

- auto size nl sock recv buffer

## Release sah-next_v5.39.1 - 2024-03-01(10:28:16 +0000)

## Release sah-next_v5.39.0 - 2024-02-29(16:23:56 +0000)

### Other

- support dyn config_id hostapd cfg param

## Release sah-next_v5.38.3 - 2024-02-29(07:56:26 +0000)

### Other

- - add eth_link flag.

## Release sah-next_v5.38.2 - 2024-02-28(15:02:15 +0000)

### Other

- Automatic MAC offset based on radio index

## Release sah-next_v5.38.1 - 2024-02-28(09:02:24 +0000)

### Other

- ChannelsInUse not used

## Release sah-next_v5.38.0 - 2024-02-27(18:24:06 +0000)

### Other

- add dynamically rnr hostapd config param

## Release sah-next_v5.37.0 - 2024-02-27(17:59:58 +0000)

### Other

- - add EMLMR and EMLSR support

## Release sah-next_v5.36.0 - 2024-02-27(17:32:46 +0000)

### Other

- auto detect hostapd config params before writing them

## Release sah-next_v5.35.2 - 2024-02-27(11:57:36 +0000)

### Other

- : SSIDs and Accesspoints Enable parameters toggle in loop after calling WiFi reset

## Release sah-next_v5.35.1 - 2024-02-26(16:27:48 +0000)

### Other

- - OperatingChannelBandwidth is not upgrade persistent

## Release sah-next_v5.35.0 - 2024-02-26(15:28:56 +0000)

### Other

- : WiFi Radio and AccessPoint aliases are different than expected

## Release sah-next_v5.34.4 - 2024-02-22(15:24:30 +0000)

### Other

- add empty defaults when not doing persist

## Release sah-next_v5.34.3 - 2024-02-22(08:50:27 +0000)

### Other

- - enable auto commit manager when not doing persistance

## Release sah-next_v5.34.2 - 2024-02-21(09:22:56 +0000)

### Other

- - add the ability to enable the dummy vap for specific radio...

## Release sah-next_v5.34.1 - 2024-02-20(09:31:23 +0000)

### Other

- : Cannot bring-up 5GHz after reset_hard

## Release sah-next_v5.34.0 - 2024-02-16(18:08:08 +0000)

### Other

- - set more HT/VHT capabilities

## Release sah-next_v5.33.7 - 2024-02-16(15:17:27 +0000)

### Other

- set radio status with backhaul when fronthaul is inactive

## Release sah-next_v5.33.6 - 2024-02-16(14:53:58 +0000)

### Other

- - force to set SAE H2E/HnP support with WPA3

## Release sah-next_v5.33.5 - 2024-02-15(16:41:36 +0000)

### Other

- increase default value of min BSS per radio when MBSS is supported

## Release sah-next_v5.33.4 - 2024-02-15(16:20:51 +0000)

### Other

- Issue: ssw/services_pwhm#1 flow:
- Issue: ssw/services_pwhm#2 flow:

## Release sah-next_v5.33.3 - 2024-02-15(13:19:22 +0000)

### Other

- - add new debug API to display all wiphy interfaces

## Release sah-next_v5.33.2 - 2024-02-15(12:03:47 +0000)

### Other

- fix dfsDone notif and setChanspec rpc callInfo

## Release sah-next_v5.33.1 - 2024-02-12(17:27:22 +0000)

### Other

- [Wi-Fi] Probe Request received by pwhm only the first time - after reset

## Release sah-next_v5.33.0 - 2024-02-12(17:05:12 +0000)

### Other

- - [ZW-DFS] add new APIs to allow the management of ZW DFS feature on pWHM vendor modules side

## Release sah-next_v5.32.2 - 2024-02-12(16:13:31 +0000)

### Other

- Crash on pwhm

## Release sah-next_v5.32.1 - 2024-02-12(08:02:47 +0000)

### Other

- - fix station disabling HT/VHT/HE/BE if no WMM/QoS

## Release sah-next_v5.32.0 - 2024-02-09(16:11:33 +0000)

### Other

- : Add EndPoint event for config changes

## Release sah-next_v5.31.9 - 2024-02-09(15:16:10 +0000)

### Other

- [Wi-Fi] Observations are not reported by getStationInfo()

## Release sah-next_v5.31.8 - 2024-02-09(10:07:06 +0000)

### Other

- - Radio TransmitPower is not reboot persistent

## Release sah-next_v5.31.7 - 2024-02-08(16:06:28 +0000)

### Other

- - Scan API not always working

## Release sah-next_v5.31.6 - 2024-02-08(08:46:05 +0000)

### Other

- set RTS_threshold to hostapd config only when it's intented

## Release sah-next_v5.31.5 - 2024-02-07(13:19:20 +0000)

### Other

- : Vap6g0priv is down when Security.ModeEnabled is set to "OWE" or "None"

## Release sah-next_v5.31.4 - 2024-02-06(13:37:03 +0000)

### Other

- - Add sae_pwe parameter to wpa_supplicant.

## Release sah-next_v5.31.3 - 2024-02-04(11:20:07 +0000)

### Other

- - Fix not initialized array

## Release sah-next_v5.31.2 - 2024-02-02(17:15:40 +0000)

### Other

- : WiFi config not well restored when MACFiltering.Entry is not empty

## Release sah-next_v5.31.1 - 2024-02-02(13:56:29 +0000)

### Other

- - Add Radio LastChange parameter

## Release sah-next_v5.31.0 - 2024-02-01(17:36:17 +0000)

### Other

- - Add Preamble Type configuration

## Release sah-next_v5.30.6 - 2024-02-01(11:30:07 +0000)

### Other

- - fix 320Mhz channel configuration issues

## Release sah-next_v5.30.5 - 2024-02-01(08:39:44 +0000)

### Other

- : RssiEventing report only one station per event

## Release sah-next_v5.30.4 - 2024-02-01(08:28:19 +0000)

## Release sah-next_v5.30.3 - 2024-01-31(11:31:51 +0000)

## Release sah-next_v5.30.2 - 2024-01-31(11:09:39 +0000)

### Other

- Endpoint interface to bridge

## Release sah-next_v5.30.1 - 2024-01-30(10:56:29 +0000)

### Other

- - minor MLO fix + debug info + secDmn API

## Release sah-next_v5.30.0 - 2024-01-30(10:39:58 +0000)

### Other

- - add set/get radio DFS channel clear time based on wiphy info

## Release sah-next_v5.29.5 - 2024-01-29(08:41:09 +0000)

### Other

- fix wrong rad status when doing bgdfs or ep connected

## Release sah-next_v5.29.4 - 2024-01-26(11:29:53 +0000)

### Other

- - Primary VAP is UP even when it is disabled

## Release sah-next_v5.29.3 - 2024-01-26(09:39:58 +0000)

### Other

- fix hostapd running after disabling last vap

## Release sah-next_v5.29.2 - 2024-01-25(09:13:17 +0000)

### Other

- restore ep iface creation conditioned with rad StaMode

## Release sah-next_v5.29.1 - 2024-01-24(13:02:08 +0000)

### Other

- - Wi-Fi is off after modifying default IPAddress

## Release sah-next_v5.29.0 - 2024-01-24(09:49:27 +0000)

### Other

- - add debugging info

## Release sah-next_v5.28.2 - 2024-01-23(15:07:38 +0000)

### Other

- - Channel is not upgrade persistent

## Release sah-next_v5.28.1 - 2024-01-23(14:22:17 +0000)

### Other

- set BSS max_num_sta in hapd conf when explicit

## Release sah-next_v5.28.0 - 2024-01-23(12:01:51 +0000)

### Other

- fetch multi-field wpactrl event names

## Release sah-next_v5.27.5 - 2024-01-23(11:08:00 +0000)

## Release sah-next_v5.27.4 - 2024-01-23(10:15:33 +0000)

### Other

- handle as much association as supported stations

## Release sah-next_v5.27.3 - 2024-01-22(09:50:21 +0000)

### Other

- RSSI events are missing elements.

## Release sah-next_v5.27.2 - 2024-01-19(12:30:05 +0000)

### Other

- : Change pwhm default process name in prplOS

## Release sah-next_v5.27.1 - 2024-01-19(12:17:44 +0000)

### Other

- : RssiEventing is sending empty map

## Release sah-next_v5.27.0 - 2024-01-17(16:26:48 +0000)

### Other

- - add noise by chain

## Release sah-next_v5.26.3 - 2024-01-17(13:29:48 +0000)

## Release sah-next_v5.26.2 - 2024-01-17(11:22:17 +0000)

### Other

- - Delete VAP doesn't remove the VAP from hostapd config file

## Release sah-next_v5.26.1 - 2024-01-16(11:29:33 +0000)

### Other

- Endpoint interface to bridge: fix after review

## Release sah-next_v5.26.0 - 2024-01-15(09:58:39 +0000)

### Other

- Endpoint interface to bridge

## Release sah-next_v5.25.3 - 2024-01-12(13:43:01 +0000)

### Other

- - TrafficMonitor - Fix Prio traffic busy level

## Release sah-next_v5.25.2 - 2024-01-11(19:16:08 +0000)

### Other

- fix rad obj mapping to device supporting multi-band

## Release sah-next_v5.25.1 - 2024-01-11(18:24:23 +0000)

### Other

- restore fix removed wrongly by SSW-7598

## Release sah-next_v5.25.0 - 2024-01-11(13:17:16 +0000)

### Other

- - Add RTSThreshold parameter

## Release sah-next_v5.24.0 - 2024-01-09(12:32:16 +0000)

### Other

- - add global sync

## Release sah-next_v5.23.2 - 2024-01-08(07:55:00 +0000)

### Other

- - Fix Multiple get stats call for each station

## Release sah-next_v5.23.1 - 2024-01-08(07:42:47 +0000)

### Other

- : Invalid BSSID scan

## Release sah-next_v5.23.0 - 2024-01-04(08:53:17 +0000)

### Other

- - Add MLO capability

## Release sah-next_v5.22.0 - 2024-01-04(08:01:56 +0000)

### Other

- - add vap action eventing

## Release sah-next_v5.21.0 - 2024-01-03(16:19:44 +0000)

### Other

- - Automatic Neighbor addition

## Release sah-next_v5.20.1 - 2024-01-03(16:00:20 +0000)

### Other

- fix 6ghz baseChan calculation

## Release sah-next_v5.20.0 - 2024-01-02(12:37:57 +0000)

### Other

- : Add MACAddressControlEnabled parameter

## Release sah-next_v5.19.4 - 2024-01-02(10:06:27 +0000)

### Other

- : No notification when Index changes

## Release sah-next_v5.19.3 - 2023-12-27(16:22:55 +0000)

### Other

- - fix klocwork issue

## Release sah-next_v5.19.2 - 2023-12-19(17:34:35 +0000)

### Other

- : KeyPassPhrase is lost after upgrade

## Release sah-next_v5.19.1 - 2023-12-14(14:48:48 +0000)

### Other

- -  defining missing TRAP impl

## Release sah-next_v5.19.0 - 2023-12-14(13:07:59 +0000)

### Other

- - add start ZeroWait DFS call to FTA.

## Release sah-next_v5.18.6 - 2023-12-13(16:55:26 +0000)

### Other

- - Check loaded hostapd conf not being empty

## Release sah-next_v5.18.5 - 2023-12-13(15:39:36 +0000)

### Other

- only keep vdr mod specific wpactrl rad evt hdlrs on reload

## Release sah-next_v5.18.4 - 2023-12-13(07:58:15 +0000)

### Other

- keep vdr mod specific wpactrl evt hdlrs on reload

## Release sah-next_v5.18.3 - 2023-12-13(07:35:15 +0000)

### Other

- fix hotspot2 obj validation

## Release sah-next_v5.18.2 - 2023-12-12(18:05:47 +0000)

### Other

- fix chan_in_bands when bw auto

## Release sah-next_v5.18.1 - 2023-12-12(15:50:05 +0000)

### Other

- keep radio dm status notPresent until mapped to hw device

## Release sah-next_v5.18.0 - 2023-12-12(15:27:04 +0000)

### Other

- propagate wpactrl dfs/csa/cac events

## Release sah-next_v5.17.1 - 2023-12-12(14:50:57 +0000)

### Other

- : Add InstanceName

## Release sah-next_v5.17.0 - 2023-12-12(14:23:07 +0000)

### Other

- add public apis to manage rad macConfig

## Release sah-next_v5.16.4 - 2023-12-11(14:08:57 +0000)

### Other

- - fix using wrong amx variant set value

## Release sah-next_v5.16.3 - 2023-12-08(12:44:08 +0000)

### Other

- WiFi.AccessPoint.{x}.AssociatedDevice.{x}.Retransmission not updated

## Release sah-next_v5.16.2 - 2023-12-08(11:37:25 +0000)

### Other

- [USP] Plugins that use the USP backend need to load it explicitly

## Release sah-next_v5.16.1 - 2023-12-07(13:23:13 +0000)

### Other

- install ep config

## Release sah-next_v5.16.0 - 2023-12-06(15:08:35 +0000)

### Other

- - add AcsBootChannel parameter to Radio.

## Release sah-next_v5.15.3 - 2023-12-06(08:06:55 +0000)

## Release sah-next_v5.15.2 - 2023-12-05(15:38:09 +0000)

### Other

- fill endpoint iface name/mac of radio with enabled STASupported_Mode

## Release sah-next_v5.15.1 - 2023-12-05(13:37:45 +0000)

### Other

- use fta call isof direct fsm sched when adding vap iface
- chanmgt init with available chanspec from driver

## Release sah-next_v5.15.0 - 2023-12-05(09:43:17 +0000)

### Other

- load var env for WLAN_ADDR

## Release sah-next_v5.14.1 - 2023-12-04(13:06:35 +0000)

### Other

- auto calc rad baseMacAddress from env vars

## Release sah-next_v5.14.0 - 2023-12-04(09:47:55 +0000)

### Other

- - handle and notify RRM beacon response and request status events

## Release sah-next_v5.13.0 - 2023-12-01(10:28:43 +0000)

### Other

- add new public util APIs for vendor modules

## Release sah-next_v5.12.1 - 2023-12-01(09:29:17 +0000)

### Other

- fix warnings in dm param validator on template object

## Release sah-next_v5.12.0 - 2023-12-01(08:50:27 +0000)

### Other

- move fsm state sched out of wpactrl gen event handling

## Release sah-next_v5.11.0 - 2023-11-30(09:28:44 +0000)

### Other

- add wpactrl eventHandlers to intercept redirect and modify msg

## Release sah-next_v5.10.0 - 2023-11-28(14:09:27 +0000)

### Other

- - handle and notify channel switch and CSA finished events

## Release sah-next_v5.9.6 - 2023-11-27(11:17:24 +0000)

### Other

- trigger RAD_CHANGE_INIT event after Rad DM be loaded

## Release sah-next_v5.9.5 - 2023-11-27(09:56:07 +0000)

### Other

- [PPM-2628][osp] update ht/vht caps in hostapd conf with new target operChBw

## Release sah-next_v5.9.4 - 2023-11-24(12:11:05 +0000)

### Other

- - BSSID not properly set when external config source

## Release sah-next_v5.9.3 - 2023-11-24(11:58:37 +0000)

### Other

- correct wrong linux utils function declaration

## Release sah-next_v5.9.2 - 2023-11-24(07:26:07 +0000)

## Release sah-next_v5.9.1 - 2023-11-23(16:18:59 +0000)

### Other

- VAPs are down after endpoint activation and reconnection

## Release sah-next_v5.9.0 - 2023-11-22(15:37:20 +0000)

### Other

- - Add eventing for ap and sta, and vendor data lists

## Release sah-next_v5.8.0 - 2023-11-20(10:13:50 +0000)

### Other

- - handle and notify association failures

## Release sah-next_v5.7.0 - 2023-11-17(16:08:56 +0000)

### Other

- - clean up channel functions

## Release sah-next_v5.6.2 - 2023-11-17(14:03:20 +0000)

### Other

- - fixing small errors

## Release sah-next_v5.6.1 - 2023-11-17(12:00:44 +0000)

### Other

- - Several band steering failures

## Release sah-next_v5.6.0 - 2023-11-17(10:44:42 +0000)

### Other

- - add MLO capabilities

## Release sah-next_v5.5.0 - 2023-11-17(10:02:00 +0000)

### Other

- - add WiFi 7 support

## Release sah-next_v5.4.0 - 2023-11-14(18:26:36 +0000)

### Other

- : Add TLVs support

## Release sah-next_v5.3.11 - 2023-11-14(16:29:31 +0000)

### Other

- - do not send scan notification in async mode.

## Release sah-next_v5.3.10 - 2023-11-14(16:03:53 +0000)

### Other

- restore legacy wld_radio_updateAntenna

## Release sah-next_v5.3.9 - 2023-11-14(14:56:55 +0000)

### Other

- load vendor modules from alternative default path

## Release sah-next_v5.3.8 - 2023-11-14(14:06:39 +0000)

### Other

- fix scanStats dm notif

## Release sah-next_v5.3.7 - 2023-11-14(12:35:57 +0000)

### Other

- direct set regdomain at boot to early update possible channels

## Release sah-next_v5.3.6 - 2023-11-14(11:28:56 +0000)

### Other

- set direct label pin when hostapd connection is ready

## Release sah-next_v5.3.5 - 2023-11-14(11:15:06 +0000)

### Other

- apply transaction only on DM configurable VAPs

## Release sah-next_v5.3.4 - 2023-11-14(10:44:10 +0000)

### Other

- set AssociatedDevice stats params without transaction

## Release sah-next_v5.3.3 - 2023-11-14(07:37:51 +0000)

### Other

- : Incorrect value for "WiFi.AccessPoint.[].MaxAssociatedDevices" after reboot

## Release sah-next_v5.3.2 - 2023-11-13(18:01:46 +0000)

### Other

- set SSID/Radio stats params without transaction

## Release sah-next_v5.3.1 - 2023-11-13(17:28:56 +0000)

### Other

- finalize VAP iface creation after referencing SSID and Radio

## Release sah-next_v5.3.0 - 2023-11-13(12:15:07 +0000)

### Other

- - add ScanReason to ScanChange notif.

## Release sah-next_v5.2.1 - 2023-11-09(12:02:23 +0000)

### Other

- - ChannelMgt sub-object not persistent

## Release sah-next_v5.2.0 - 2023-11-07(15:21:28 +0000)

## Release sah-next_v5.1.1 - 2023-11-07(14:00:48 +0000)

### Other

- : WiFi not showing when channel and bandwidth changes multiple times

## Release sah-next_v5.1.0 - 2023-11-07(11:58:02 +0000)

### Other

- use of libamxo APIs to get parser config and connections

## Release sah-next_v5.0.1 - 2023-10-27(10:10:32 +0000)

### Other

- - Radio alias invalid

## Release sah-next_v5.0.0 - 2023-10-26(07:24:31 +0000)

### Other

- - ensure changes get propagated in the data model

## Release sah-next_v4.21.0 - 2023-10-25(17:03:14 +0000)

### Other

- : Add relay credentials feature

## Release sah-next_v4.20.4 - 2023-10-25(14:14:21 +0000)

### Other

- : Remove ProbeRequest handling

## Release sah-next_v4.20.3 - 2023-10-25(14:03:03 +0000)

### Other

- : Remove ProbeRequest handling

## Release sah-next_v4.20.2 - 2023-10-25(13:43:10 +0000)

### Other

- fix scan dep
- set wps uuid in wpa_supp conf

## Release sah-next_v4.20.1 - 2023-10-23(15:21:18 +0000)

### Other

- [WiFi Sensing] handle API errors while adding new CSI client instance

## Release sah-next_v4.20.0 - 2023-10-20(08:46:50 +0000)

### Other

- Add support for adding a monitor interface to a radio

## Release sah-next_v4.19.4 - 2023-10-16(15:26:10 +0000)

### Other

- remove assumption of default rad channel

## Release sah-next_v4.19.3 - 2023-10-16(13:47:21 +0000)

### Other

- : No management frame sent when channel equals 0
- [prpl][osp] fix 20s delay when applying any conf through FSM

## Release sah-next_v4.19.2 - 2023-10-13(12:08:50 +0000)

### Other

- - DM reports sensing is enabled while it is not after station dis/re-association

## Release sah-next_v4.19.1 - 2023-10-13(11:20:03 +0000)

## Release sah-next_v4.19.0 - 2023-10-13(07:34:44 +0000)

### Other

- :WDS module for pwhm

## Release sah-next_v4.18.6 - 2023-10-12(16:56:47 +0000)

### Other

- WiFi Reset Support on Router and Extender

## Release sah-next_v4.18.5 - 2023-10-11(07:24:39 +0000)

### Other

- : Missing security mode "OWE" for Wifi guest

## Release sah-next_v4.18.4 - 2023-10-10(15:03:06 +0000)

### Other

- fix getting NaSta stats

## Release sah-next_v4.18.3 - 2023-10-10(08:20:59 +0000)

### Other

- : Remove VendorSpecific action frame notifications

## Release sah-next_v4.18.2 - 2023-10-09(15:10:21 +0000)

### Other

- :WPS Methods - Self PIN is not working on both bands

## Release sah-next_v4.18.1 - 2023-10-06(16:30:09 +0000)

## Release sah-next_v4.18.0 - 2023-10-06(15:19:20 +0000)

### Other

- - Integrate pWHM on SOP

## Release sah-next_v4.17.2 - 2023-10-06(08:27:30 +0000)

### Other

- - channel switch while SwitchDelayVideo is not over

## Release sah-next_v4.17.1 - 2023-10-04(15:28:47 +0000)

### Other

- enable odl and doc check
- add support for guest vap on all radio in odl defaults

## Release sah-next_v4.17.0 - 2023-10-04(09:21:52 +0000)

### Other

- - Add Radio firmware Version

## Release sah-next_v4.16.0 - 2023-10-03(08:04:12 +0000)

### Other

- - send notification for received mgmt action frames

## Release sah-next_v4.15.2 - 2023-10-02(11:35:24 +0000)

### Other

- : Add wld_nl80211_getEvtListenerHandlers for vendor module.

## Release sah-next_v4.15.1 - 2023-09-29(13:30:10 +0000)

### Other

- - prplmesh Mxl - Factorise duplicated code lines in sub cmd callback function

## Release sah-next_v4.15.0 - 2023-09-25(07:49:19 +0000)

### Other

- - Add AgileDFS API in startBgDfsClear

## Release sah-next_v4.14.3 - 2023-09-22(07:00:49 +0000)

### Other

- SSID is not correctly applied

## Release sah-next_v4.14.1 - 2023-09-21(14:12:58 +0000)

### Other

- update licences according to licence check

## Release sah-next_v4.14.0 - 2023-09-18(14:01:44 +0000)

## Release sah-next_v4.13.1 - 2023-09-18(13:01:24 +0000)

### Other

- - [Wifi sensing] use event handling to manage Radio.Sensing object

## Release sah-next_v4.13.0 - 2023-09-15(12:05:22 +0000)

### Other

- - [Wifi sensing] implement control HL API
- WPS Methods - Client PIN is not working on both

## Release sah-next_v4.12.0 - 2023-09-14(10:08:43 +0000)

### Other

- expose MaxSupportedSSIDs

## Release sah-next_v4.11.0 - 2023-09-13(13:29:00 +0000)

### Other

- define capabilities needed by process

## Release sah-next_v4.10.2 - 2023-09-13(10:50:11 +0000)

## Release sah-next_v4.10.1 - 2023-09-12(13:42:26 +0000)

### Other

- Reload bss only

## Release sah-next_v4.10.0 - 2023-09-11(10:07:02 +0000)

### Other

- fill scanned neighBss noise and spectrum chan nbColocAp

## Release sah-next_v4.9.0 - 2023-09-07(15:29:20 +0000)

### Other

- [tr181-pcm] All the plugins that are register to PCM should set the pcm_svc_config

## Release sah-next_v4.8.1 - 2023-09-07(07:48:11 +0000)

### Other

- - RadCaps not filled.

## Release sah-next_v4.8.0 - 2023-09-05(09:27:58 +0000)

### Other

- Update scan and channel switch APIs

## Release sah-next_v4.7.0 - 2023-09-05(09:14:41 +0000)

## Release sah-next_v4.6.1 - 2023-09-04(14:22:14 +0000)

## Release sah-next_v4.6.0 - 2023-08-29(12:52:08 +0000)

### Other

- [tr181-pcm] Set the usersetting parameters for each plugin

## Release sah-next_v4.5.3 - 2023-08-28(12:10:18 +0000)

### Other

- : Add Ht/Vht/He capabilities from assoc request

## Release sah-next_v4.5.2 - 2023-08-28(07:27:57 +0000)

### Other

- getSpectrumInfo() returns <NULL>
- No Probe Request received

## Release sah-next_v4.5.1 - 2023-08-21(13:53:41 +0000)

### Other

- - Predict netlink message before allocation.

## Release sah-next_v4.5.0 - 2023-08-11(12:33:17 +0000)

### Other

- : EasyMesh R2 4.2.1 - MBO Station Disallow support

## Release sah-next_v4.4.0 - 2023-08-08(09:23:06 +0000)

### Other

- - Add getSpectrumInfo stats

## Release sah-next_v4.3.1 - 2023-08-04(16:33:00 +0000)

### Other

- fix NaSta creation/deletion with dm rpcs

## Release sah-next_v4.3.0 - 2023-08-04(15:42:26 +0000)

### Other

- add nl80211 util apis and make nl compat public

## Release sah-next_v4.2.0 - 2023-08-02(18:37:49 +0000)

### Other

- add nl80211 api to do custom endpoint creation and configuration

## Release sah-next_v4.1.1 - 2023-08-02(17:55:52 +0000)

### Other

- - fill missing remote BSSID in Endpoint wps pairingDoneSuccess notif

## Release sah-next_v4.1.0 - 2023-08-02(16:16:55 +0000)

### Other

- - apply nl80211 scan passive and active dwell time

## Release sah-next_v4.0.3 - 2023-08-02(15:52:15 +0000)

### Other

- - recover updated netdev index of vaps when missing nl80211 events

## Release sah-next_v4.0.2 - 2023-07-28(12:33:45 +0000)

### Other

- improve netlink stats getting
- add missing noise value for scanned neighbor BSSs

## Release sah-next_v4.0.1 - 2023-07-25(14:58:21 +0000)

### Other

- : Missing FTA setEvtHandlers call

## Release sah-next_v4.0.0 - 2023-07-25(07:03:32 +0000)

### Other

- fix RNR IE in beacon when having default Discovery method

## Release sah-next_v3.19.1 - 2023-07-21(07:46:59 +0000)

### Other

- restore old wld_ap_create proto still used in mod-whm

## Release sah-next_v3.19.0 - 2023-07-21(07:14:18 +0000)

### Other

- : Add new configMap FTA

## Release sah-next_v3.18.3 - 2023-07-20(08:53:37 +0000)

### Other

- use swla dm api to commit param with one transaction

## Release sah-next_v3.18.2 - 2023-07-18(16:25:14 +0000)

### Other

- revert listening for usp sock in pwhm, causing crash

## Release sah-next_v3.18.1 - 2023-07-18(12:24:10 +0000)

### Other

- : Add new Vendor attributes function

## Release sah-next_v3.18.0 - 2023-07-17(14:48:08 +0000)

### Other

- - Channel Clearing information missing when BG_CAC

## Release sah-next_v3.17.3 - 2023-07-17(14:23:08 +0000)

### Other

- MBO IE element is missing in the beacons

## Release sah-next_v3.17.2 - 2023-07-17(12:56:02 +0000)

## Release sah-next_v3.17.1 - 2023-07-05(12:57:54 +0000)

### Fixes

- Fix syntax in 30_wld-defaults-vaps.odl.uc

## Release sah-next_v3.17.0 - 2023-06-29(07:26:04 +0000)

### Other

- [pwhm] Support for GCC 12.1 and removal of deprecated functions

## Release sah-next_v3.16.3 - 2023-06-26(07:25:38 +0000)

### Other

- - Fix ME in header
- - rewrite swl_crypto for openssl 3.0 support
- add ref type fun for val types
- - sub second timestamps sometimes float in dm
- Update TBTT length values
- : Add management frame sending
- Generate gcovr txt report and upload reports to documentation server

## Release sah-next_v3.16.2 - 2023-06-23(09:11:33 +0000)

### Other

- Unable to restart prplmesh_whm script

## Release sah-next_v3.16.1 - 2023-06-16(07:48:36 +0000)

### Other

- : Add prb request event

## Release sah-next_v3.16.0 - 2023-06-14(15:04:44 +0000)

### Other

- WFA Easy Mesh certification - 4.8.1 fail on client steering

## Release sah-next_v3.15.2 - 2023-06-14(14:46:48 +0000)

### Other

- - pwhm: Propagate bBSS config to fBSS

## Release sah-next_v3.15.1 - 2023-06-09(10:17:04 +0000)

### Other

- - MultiAP wps on EndPoint

## Release sah-next_v3.15.0 - 2023-06-01(14:09:37 +0000)

### Other

- Add missing odl templates files

## Release sah-next_v3.14.0 - 2023-05-31(17:04:09 +0000)

### Other

- - prplMesh Mxl - Add ap-force flag to trigger scan request

## Release sah-next_v3.13.2 - 2023-05-30(09:47:34 +0000)

### Other

- - Create plugin's ACLs permissions

## Release sah-next_v3.13.1 - 2023-05-26(09:05:37 +0000)

### Other

- use pwhm dm root name to fetch SSID Reference

## Release sah-next_v3.13.0 - 2023-05-25(15:42:20 +0000)

### Other

- - Vendor events architecture

## Release sah-next_v3.12.1 - 2023-05-25(13:50:08 +0000)

### Other

- Wrong Mode bg reported in AssociatedDevice when N sta connected

## Release sah-next_v3.12.0 - 2023-05-24(13:59:15 +0000)

### Other

- - expose attribute management tools

## Release sah-next_v3.10.3 - 2023-05-22(11:53:04 +0000)

### Other

- - Solve invalid hostapd conf on 6GHz

## Release sah-next_v3.10.2 - 2023-05-17(08:08:26 +0000)

### Other

- persistent storage on parameters mark

## Release sah-next_v3.10.1 - 2023-05-16(10:18:54 +0000)

## Release sah-next_v3.10.0 - 2023-05-15(09:41:43 +0000)

## Release sah-next_v3.9.0 - 2023-05-12(07:25:36 +0000)

## Release sah-next_v3.8.0 - 2023-05-11(09:11:57 +0000)

### Other

- -Tests Failing dur to wrong "WiFi.AccessPoint.*.SSIDReference" 's mapping

## Release sah-next_v3.7.3 - 2023-05-05(13:59:17 +0000)

### Other

- add detailed radioStatus to datamodel

## Release sah-next_v3.7.2 - 2023-04-27(07:52:27 +0000)

### Other

- - solve uninitialised values from valgrind

## Release sah-next_v3.7.1 - 2023-04-20(11:59:27 +0000)

### Other

- - Implementing delNeighbourAP and addNeighbourAP API using Linux Std APIs

## Release sah-next_v3.7.0 - 2023-04-20(07:12:59 +0000)

### Fixes

- [odl]Remove deprecated odl keywords

### Other

- - Save wDevId value
- - Update wld_util.h and related source files

## Release sah-next_v3.6.0 - 2023-04-14(18:25:53 +0000)

### Other

- move vap enabling code to fta isof fsm state

## Release sah-next_v3.5.0 - 2023-04-14(16:36:41 +0000)

### Other

- make getLastAssocFrame rpc callable from AssocDev obj

## Release sah-next_v3.4.3 - 2023-04-13(15:35:39 +0000)

### Other

- cleanup all APs assocDev lists when stopping hostapd

## Release sah-next_v3.4.2 - 2023-04-13(11:39:51 +0000)

### Other

- update nl80211 iface event listener when ifIndex changes

## Release sah-next_v3.4.1 - 2023-04-12(13:50:00 +0000)

### Other

- force stations to re-authenticate after vap ifaces enabling reconf

## Release sah-next_v3.4.0 - 2023-04-12(12:24:39 +0000)

### Other

- : centralize internal ssid ctx creation

## Release sah-next_v3.3.0 - 2023-04-12(10:54:40 +0000)

### Other

- add nl80211 api get all wiphy devices info

## Release sah-next_v3.2.0 - 2023-04-07(15:58:11 +0000)

### Other

- allow external call of wpa_ctrl mgr event handlers

## Release sah-next_v3.1.0 - 2023-04-07(15:48:13 +0000)

### Other

- move chanspec applying code from fsm to fta

## Release sah-next_v3.0.3 - 2023-04-07(15:34:27 +0000)

### Other

- fix wps config methods in hapd conf

## Release sah-next_v3.0.2 - 2023-04-07(08:20:33 +0000)

### Other

- support setting regDomain before starting hostapd
- fix regression in wifi neighbour scan

## Release sah-next_v3.0.1 - 2023-04-03(13:16:06 +0000)

### Other

- - MACFiltering doesnt work

## Release sah-next_v3.0.0 - 2023-04-03(12:53:14 +0000)

### Other

- - Update channel mgt and radio statistics

## Release sah-next_v2.26.0 - 2023-03-29(12:14:58 +0000)

### Other

- - [prplMesh_WHM] Exposing VHT,HT and HE radio capabilities in the datamodel

## Release sah-next_v2.25.0 - 2023-03-24(15:57:07 +0000)

### Other

- get radio air stats using nl80211 chan survey

## Release sah-next_v2.24.2 - 2023-03-16(14:28:29 +0000)

### Other

- - Use SWL defined macro WPS notification

## Release sah-next_v2.24.1 - 2023-03-16(12:47:52 +0000)

### Other

- fix missing btm response notification

## Release sah-next_v2.24.0 - 2023-03-16(11:02:14 +0000)

### Other

- cleanup & improv saving stats in dm

## Release sah-next_v2.23.3 - 2023-03-16(08:38:15 +0000)

### Other

- fix default enabling of 80211h on radio 5gHz

## Release sah-next_v2.23.2 - 2023-03-13(09:23:11 +0000)

### Other

- remove initial arbitrary timer for dm conf loading

## Release sah-next_v2.23.1 - 2023-03-13(09:03:16 +0000)

### Other

- turris-omnia: random: pwhm failed to start

## Release sah-next_v2.23.0 - 2023-03-10(12:33:50 +0000)

### Other

- make configurable pwhm coredump generation

## Release sah-next_v2.22.3 - 2023-03-03(15:15:38 +0000)

### Other

- deauth connected stations when disabling host rad/vap

## Release sah-next_v2.22.2 - 2023-03-03(14:23:11 +0000)

### Other

- fix disabled endpoint interface name and status on boot

## Release sah-next_v2.22.1 - 2023-03-02(07:49:30 +0000)

### Other

- Add Operating Classes for United States and Japan

## Release sah-next_v2.22.0 - 2023-02-27(11:11:02 +0000)

### Other

- handle actions on vendor object hierarchy

## Release sah-next_v2.21.1 - 2023-02-22(08:45:46 +0000)

## Release sah-next_v2.21.0 - 2023-02-21(09:14:45 +0000)

### Other

- - add more options for IEEE80211v BTM request

## Release sah-next_v2.20.0 - 2023-02-20(08:26:10 +0000)

### Other

- - IEEE80211k capabilities per station

## Release sah-next_v2.19.2 - 2023-02-09(08:32:45 +0000)

### Other

- Remove the set of bridge interfaces

## Release sah-next_v2.19.1 - 2023-02-08(12:35:53 +0000)

### Other

- [pwhm] fix splitted defaults loading

## Release sah-next_v2.19.0 - 2023-02-07(08:27:34 +0000)

## Release sah-next_v2.18.0 - 2023-02-06(12:29:05 +0000)

### Other

- - Add disassociation event

## Release sah-next_v2.17.2 - 2023-02-03(10:26:20 +0000)

### Other

- [pwhm] fix iphone device authentication issue in wpa3 modes

## Release sah-next_v2.17.1 - 2023-02-02(12:09:17 +0000)

### Other

- fix cleanup of obsolete assocDevs with invalid objs

## Release sah-next_v2.17.0 - 2023-01-30(14:22:54 +0000)

### Other

- cleanup wld: replace wld securityMode with swl type
- adding the bssid as an argument to the scan function(pwhm+Nl802)

## Release sah-next_v2.16.2 - 2023-01-27(12:05:17 +0000)

### Other

- Testing issue

## Release sah-next_v2.16.1 - 2023-01-25(16:37:19 +0000)

### Changes

- [pwhm] use default dir iso one default file

## Release sah-next_v2.16.0 - 2023-01-24(13:44:22 +0000)

### Other

- - cleanup and add testing

## Release sah-next_v2.15.0 - 2023-01-18(14:54:22 +0000)

### Other

- add apis for nl80211 ap scan actions

## Release sah-next_v2.14.1 - 2023-01-18(09:51:14 +0000)

### Other

- - RssiMonitoring issues fixed

## Release sah-next_v2.14.0 - 2023-01-17(11:01:08 +0000)

### Other

- disable config opt check
- load datamodel extension definition and defaults

## Release sah-next_v2.13.5 - 2023-01-06(18:21:48 +0000)

### Other

- Fix pwhm integration into feeds

## Release sah-next_v2.13.4 - 2023-01-03(16:18:00 +0000)

### Other

- - Fix rpc argument types

## Release sah-next_v2.13.3 - 2022-12-21(10:27:32 +0000)

### Other

- [prplMesh WHM] endpoint ProfileReference && RadioReference setting issue
- [prplMesh WHM] wps connection credentials are empty and parsing issue

## Release sah-next_v2.13.2 - 2022-12-16(15:51:09 +0000)

### Other

- [prplMesh WHM] endpoint ConnectionStatus dm change notification is not sent

## Release sah-next_v2.13.1 - 2022-12-13(16:10:35 +0000)

### Other

- Fix klocwork issues on WiFi components

## Release sah-next_v2.13.0 - 2022-12-08(13:38:29 +0000)

### New

- [pwhm] use default dir iso one default file

## Release sah-next_v2.12.1 - 2022-12-07(17:06:21 +0000)

### Other

- - Adding operating_class and channel for the NonAssociatedDevice object

## Release sah-next_v2.12.0 - 2022-12-07(15:49:36 +0000)

### Other

- - Adding operating_class and channel for the NonAssociatedDevice object

## Release sah-next_v2.11.1 - 2022-12-05(08:28:49 +0000)

### Other

- [pwhm] fix crash when getting sta stats after 2 disconnections
- remove squash commits as no needed anymore

## Release sah-next_v2.11.0 - 2022-12-02(14:58:49 +0000)

### Other

- - [prplMesh M2] WiFi.NeighboringWiFiDiagnostic

## Release sah-next_v2.10.0 - 2022-12-02(09:01:51 +0000)

### Other

- [pwhm][libwld] restore nl80211 unit tests

## Release sah-next_v2.9.0 - 2022-11-29(12:03:46 +0000)

### Other

- support SSID LowerLayer TR181 compliant format

## Release sah-next_v2.8.0 - 2022-11-28(14:26:26 +0000)

### Other

- [pwhm] ssid interface name set in dm with no amx notification

## Release sah-next_v2.7.2 - 2022-11-25(11:13:06 +0000)

### Other

- [pwhm] crash on stop

## Release sah-next_v2.7.1 - 2022-11-24(08:41:17 +0000)

### Other

- - [pwhm] Enabled secondary vaps status remain Down

## Release sah-next_v2.7.0 - 2022-11-18(16:41:25 +0000)

### Other

- - [prplMesh WHM] Adding NonAssociatedDevice through createNonAssociatedDevice() does not work

## Release sah-next_v2.6.0 - 2022-11-18(11:22:07 +0000)

### Other

- - Add rad & vap state statistics

## Release sah-next_v2.4.2 - 2022-11-03(10:03:23 +0000)

### Other

- [prplMesh WHM] endpoint: setting the radio in managed (station) mode

## Release sah-next_v2.4.1 - 2022-11-02(13:42:02 +0000)

### Other

- prplMesh - Endpoint support - stats

## Release sah-next_v2.4.0 - 2022-10-25(10:09:28 +0000)

### Other

- - Add list of DFS marked channels to DFS event

## Release sah-next_v2.3.1 - 2022-10-24(07:45:19 +0000)

### Other

- : pwhm - Station history is not working anymore

## Release sah-next_v2.3.0 - 2022-10-20(06:58:55 +0000)

### Other

- Alias missing from WiFi Radio and SSID interface
- [prplMesh WHM] the endpoint connection is not working properly

## Release sah-next_v2.2.5 - 2022-10-17(14:45:36 +0000)

### Other

- - Implementing endpoint/endpoint profiles instances

## Release sah-next_v2.2.4 - 2022-10-17(12:51:47 +0000)

### Other

- squash commits before opensourcing them to make sahbot principal author for SoftAtHome deliveries
- [prpl] create an initial wifi debug script

## Release sah-next_v2.2.3 - 2022-10-07(10:29:54 +0000)

### Other

- : Invalid MBO transition reason code

## Release sah-next_v2.2.2 - 2022-10-07(07:47:30 +0000)

### Other

- build with -Wformat -Wformat-security

## Release sah-next_v2.2.1 - 2022-10-06(16:34:08 +0000)

### Other

- - Fix wld compliation issue

## Release sah-next_v2.2.0 - 2022-10-06(12:23:46 +0000)

### Other

- [prpl] code tweak for stats impl

## Release sah-next_v2.1.2 - 2022-10-06(09:24:41 +0000)

### Other

- : Wrong RSSI value

## Release sah-next_v2.1.1 - 2022-10-06(09:19:08 +0000)

### Other

- - Implementing endpoint Profile security

## Release sah-next_v2.1.0 - 2022-10-05(09:38:36 +0000)

### Other

- - Endpoint support - WPS connection

## Release sah-next_v2.0.2 - 2022-09-28(12:36:23 +0000)

### Other

- : Segfault wld_bcm

## Release sah-next_v2.0.1 - 2022-09-28(11:22:06 +0000)

### Other

- Support for Vendor Specific nl80211 extensions

## Release sah-next_v2.0.0 - 2022-09-27(08:16:58 +0000)

### Other

- [prpl] clean swl swla headers install

## Release sah-next_v1.13.0 - 2022-09-26(16:25:34 +0000)

### Other

- support loading vendor modules

## Release sah-next_v1.12.1 - 2022-09-26(13:25:42 +0000)

### Other

- - Add initial tests
- - Implement 11k using hostapd cmd/event

## Release sah-next_v1.12.0 - 2022-09-22(09:49:27 +0000)

### Other

- Add channel noise function

## Release sah-next_v1.11.4 - 2022-09-21(07:36:38 +0000)

### Other

- - Implementing wpa_supplicant fsm

## Release sah-next_v1.11.3 - 2022-09-20(07:39:38 +0000)

### Other

- 6GHz discovery optimization

## Release sah-next_v1.11.2 - 2022-09-09(09:39:46 +0000)

### Other

- [prpl] remove unused token in pwhm

## Release sah-next_v1.11.1 - 2022-09-08(13:06:22 +0000)

### Other

- - FastStaReconnect catching to many events

## Release sah-next_v1.11.0 - 2022-09-08(10:28:10 +0000)

## Release sah-next_v1.10.1 - 2022-09-08(09:47:20 +0000)

### Other

- : Add public action frame API.

## Release sah-next_v1.10.0 - 2022-09-07(14:14:35 +0000)

### Other

- improve wpactrl connections of MultiVAP

## Release sah-next_v1.9.0 - 2022-09-07(10:37:06 +0000)

### Other

- manage secDmn and hostapd crash and stop

## Release sah-next_v1.8.6 - 2022-09-06(17:13:55 +0000)

### Other

- add missing dependency tag of libswlc

## Release sah-next_v1.8.5 - 2022-09-06(16:39:30 +0000)

### Other

- handle wps registrar config and triggering

## Release sah-next_v1.8.4 - 2022-09-05(15:34:45 +0000)

### Other

- Enabling traces for WiFi component

## Release sah-next_v1.8.3 - 2022-09-02(16:56:11 +0000)

### Other

- Enabling traces for WiFi component

## Release sah-next_v1.8.2 - 2022-09-02(14:42:33 +0000)

### Other

- Enabling traces for WiFi component

## Release sah-next_v1.8.1 - 2022-09-02(08:30:37 +0000)

### Other

- - Implementing get endpoint bssid api

## Release sah-next_v1.8.0 - 2022-09-01(13:02:38 +0000)

## Release sah-next_v1.7.9 - 2022-09-01(09:50:10 +0000)

### Other

- Implementing endpoint_disconnect api

## Release sah-next_v1.7.8 - 2022-08-30(17:11:36 +0000)

### Other

- Add FortyMHz Intolerant bit in datamodel per STA

## Release sah-next_v1.7.7 - 2022-08-30(17:04:52 +0000)

### Other

- emit the AP and the SSID status changed event

## Release sah-next_v1.7.6 - 2022-08-29(12:52:50 +0000)

### Other

- emit the radio status changed event

## Release sah-next_v1.7.5 - 2022-08-29(11:31:46 +0000)

### Other

- : Station statistic histograms

## Release sah-next_v1.7.4 - 2022-08-29(10:01:15 +0000)

### Other

- - Add endpoint create/destroy hook apis

## Release sah-next_v1.7.3 - 2022-08-29(09:05:17 +0000)

### Other

- [pWHM] getLastAssocReq api adjustment

## Release sah-next_v1.7.2 - 2022-08-26(16:53:40 +0000)

### Other

- set dynamically vap security config
- [prpl] disable uci wireless through pwhm init script

## Release sah-next_v1.7.1 - 2022-08-26(09:49:49 +0000)

### Other

- : [pwhm] detect radios by context and not by pci order

## Release sah-next_v1.7.0 - 2022-08-18(09:42:12 +0000)

### Other

- [prplMesh WHM] the AP AssociatedDevice dm object change events are not sent over the bus

## Release sah-next_v1.6.1 - 2022-08-18(09:39:14 +0000)

### Other

- schedule fsm actions to apply dynamic conf to hapd
- [prplMesh WHM] add additional info to the pwhm getLastAssocReq api

## Release sah-next_v1.6.0 - 2022-08-08(16:16:23 +0000)

### Other

- creating/updating the wpa_supplicant configguration file

## Release sah-next_v1.5.11 - 2022-08-05(10:31:07 +0000)

### Other

- fill missing station stats info

## Release sah-next_v1.5.10 - 2022-08-01(07:51:03 +0000)

### Other

- - make amx getStationStats work

## Release sah-next_v1.5.9 - 2022-07-29(11:54:44 +0000)

### Other

- clean up components.h header

## Release sah-next_v1.5.8 - 2022-07-29(07:43:48 +0000)

### Other

- clean up components.h header

## Release sah-next_v1.5.7 - 2022-07-28(08:00:27 +0000)

### Other

- : RssiEventing not reboot persistent

## Release sah-next_v1.5.6 - 2022-07-19(11:46:54 +0000)

### Other

- ChannelChangeReason not properly working

## Release sah-next_v1.5.5 - 2022-07-13(08:43:45 +0000)

### Other

- - Integrate prplMesh on Dish HW

## Release sah-next_v1.5.4 - 2022-06-29(16:56:45 +0000)

### Other

- fix stability issues with pwhm amx on wnc board

## Release sah-next_v1.5.3 - 2022-06-29(12:47:29 +0000)

### Other

- : Wrong disassoc value in BTM

## Release sah-next_v1.5.2 - 2022-06-29(12:09:15 +0000)

### Other

- 2.4 GHz out of order after changing the channel

## Release sah-next_v1.5.1 - 2022-06-28(16:23:52 +0000)

### Other

- fix amxc_string_clean crash at boot on wnc

## Release sah-next_v1.5.0 - 2022-06-27(13:30:11 +0000)

### Other

- - add IEEE80211 frame parsing

## Release sah-next_v1.4.6 - 2022-06-27(09:29:28 +0000)

### Other

- BSSID and Alias DM params remain empty after boot

## Release sah-next_v1.4.5 - 2022-06-27(08:47:01 +0000)

### Other

- wld does not start automatically after reboot

## Release sah-next_v1.4.4 - 2022-06-24(08:49:48 +0000)

### Other

- don't fail when /etc/environment doesn't exist

## Release sah-next_v1.4.3 - 2022-06-23(08:40:52 +0000)

### Other

- - AutoCommitMgr not triggered on enable 0

## Release sah-next_v1.4.2 - 2022-06-22(13:06:03 +0000)

### Other

- - [amx] SSID can't be modified when having a numrical value

## Release sah-next_v1.4.1 - 2022-06-22(12:13:58 +0000)

### Other

- - [amx] Security cannot be modified

