# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]


## Release v5.32.2 - 2024-02-12(16:13:31 +0000)

### Other

- Crash on pwhm

## Release v5.32.1 - 2024-02-12(08:02:47 +0000)

### Other

- - fix station disabling HT/VHT/HE/BE if no WMM/QoS

## Release v5.32.0 - 2024-02-09(16:11:33 +0000)

### Other

- : Add EndPoint event for config changes

## Release v5.31.9 - 2024-02-09(15:16:10 +0000)

### Other

- [Wi-Fi] Observations are not reported by getStationInfo()

## Release v5.31.8 - 2024-02-09(10:07:06 +0000)

### Other

- - Radio TransmitPower is not reboot persistent

## Release v5.31.7 - 2024-02-08(16:06:28 +0000)

### Other

- - Scan API not always working

## Release v5.31.6 - 2024-02-08(08:46:05 +0000)

### Other

- set RTS_threshold to hostapd config only when it's intented

## Release v5.31.5 - 2024-02-07(13:19:20 +0000)

### Other

- : Vap6g0priv is down when Security.ModeEnabled is set to "OWE" or "None"

## Release v5.31.4 - 2024-02-06(13:37:03 +0000)

### Other

- - Add sae_pwe parameter to wpa_supplicant.

## Release v5.31.3 - 2024-02-04(11:20:07 +0000)

### Other

- - Fix not initialized array

## Release v5.31.2 - 2024-02-02(17:15:40 +0000)

### Other

- : WiFi config not well restored when MACFiltering.Entry is not empty

## Release v5.31.1 - 2024-02-02(13:56:29 +0000)

### Other

- - Add Radio LastChange parameter

## Release v5.31.0 - 2024-02-01(17:36:17 +0000)

### Other

- - Add Preamble Type configuration

## Release v5.30.6 - 2024-02-01(11:30:07 +0000)

### Other

- - fix 320Mhz channel configuration issues

## Release v5.30.5 - 2024-02-01(08:39:44 +0000)

### Other

- : RssiEventing report only one station per event

## Release v5.30.4 - 2024-02-01(08:28:19 +0000)

## Release v5.30.3 - 2024-01-31(11:31:51 +0000)

## Release v5.30.2 - 2024-01-31(11:09:39 +0000)

### Other

- Endpoint interface to bridge

## Release v5.30.1 - 2024-01-30(10:56:29 +0000)

### Other

- - minor MLO fix + debug info + secDmn API

## Release v5.30.0 - 2024-01-30(10:39:58 +0000)

### Other

- - add set/get radio DFS channel clear time based on wiphy info

## Release v5.29.5 - 2024-01-29(08:41:09 +0000)

### Other

- fix wrong rad status when doing bgdfs or ep connected

## Release v5.29.4 - 2024-01-26(11:29:53 +0000)

### Other

- - Primary VAP is UP even when it is disabled

## Release v5.29.3 - 2024-01-26(09:39:58 +0000)

### Other

- fix hostapd running after disabling last vap

## Release v5.29.2 - 2024-01-25(09:13:17 +0000)

### Other

- restore ep iface creation conditioned with rad StaMode

## Release v5.29.1 - 2024-01-24(13:02:08 +0000)

### Other

- - Wi-Fi is off after modifying default IPAddress

## Release v5.29.0 - 2024-01-24(09:49:27 +0000)

### Other

- - add debugging info

## Release v5.28.2 - 2024-01-23(15:07:38 +0000)

### Other

- - Channel is not upgrade persistent

## Release v5.28.1 - 2024-01-23(14:22:17 +0000)

### Other

- set BSS max_num_sta in hapd conf when explicit

## Release v5.28.0 - 2024-01-23(12:01:51 +0000)

### Other

- fetch multi-field wpactrl event names

## Release v5.27.5 - 2024-01-23(11:08:00 +0000)

## Release v5.27.4 - 2024-01-23(10:15:33 +0000)

### Other

- handle as much association as supported stations

## Release v5.27.3 - 2024-01-22(09:50:21 +0000)

### Other

- RSSI events are missing elements.

## Release v5.27.2 - 2024-01-19(12:30:05 +0000)

### Other

- : Change pwhm default process name in prplOS

## Release v5.27.1 - 2024-01-19(12:17:44 +0000)

### Other

- : RssiEventing is sending empty map

## Release v5.27.0 - 2024-01-17(16:26:48 +0000)

### Other

- - add noise by chain

## Release v5.26.3 - 2024-01-17(13:29:48 +0000)

## Release v5.26.2 - 2024-01-17(11:22:17 +0000)

### Other

- - Delete VAP doesn't remove the VAP from hostapd config file

## Release v5.26.1 - 2024-01-16(11:29:33 +0000)

### Other

- Endpoint interface to bridge: fix after review

## Release v5.26.0 - 2024-01-15(09:58:39 +0000)

### Other

- Endpoint interface to bridge

## Release v5.25.3 - 2024-01-12(13:43:01 +0000)

### Other

- - TrafficMonitor - Fix Prio traffic busy level

## Release v5.25.2 - 2024-01-11(19:16:08 +0000)

### Other

- fix rad obj mapping to device supporting multi-band

## Release v5.25.1 - 2024-01-11(18:24:23 +0000)

### Other

- restore fix removed wrongly by SSW-7598

## Release v5.25.0 - 2024-01-11(13:17:16 +0000)

### Other

- - Add RTSThreshold parameter

## Release v5.24.0 - 2024-01-09(12:32:16 +0000)

### Other

- - add global sync

## Release v5.23.2 - 2024-01-08(07:55:00 +0000)

### Other

- - Fix Multiple get stats call for each station

## Release v5.23.1 - 2024-01-08(07:42:47 +0000)

### Other

- : Invalid BSSID scan

## Release v5.23.0 - 2024-01-04(08:53:17 +0000)

### Other

- - Add MLO capability

## Release v5.22.0 - 2024-01-04(08:01:56 +0000)

### Other

- - add vap action eventing

## Release v5.21.0 - 2024-01-03(16:19:44 +0000)

### Other

- - Automatic Neighbor addition

## Release v5.20.1 - 2024-01-03(16:00:20 +0000)

### Other

- fix 6ghz baseChan calculation

## Release v5.20.0 - 2024-01-02(12:37:57 +0000)

### Other

- : Add MACAddressControlEnabled parameter

## Release v5.19.4 - 2024-01-02(10:06:27 +0000)

### Other

- : No notification when Index changes

## Release v5.19.3 - 2023-12-27(16:22:55 +0000)

### Other

- - fix klocwork issue

## Release v5.19.2 - 2023-12-19(17:34:35 +0000)

### Other

- : KeyPassPhrase is lost after upgrade

## Release v5.19.1 - 2023-12-14(14:48:48 +0000)

### Other

- -  defining missing TRAP impl

## Release v5.19.0 - 2023-12-14(13:07:59 +0000)

### Other

- - add start ZeroWait DFS call to FTA.

## Release v5.18.6 - 2023-12-13(16:55:26 +0000)

### Other

- - Check loaded hostapd conf not being empty

## Release v5.18.5 - 2023-12-13(15:39:36 +0000)

### Other

- only keep vdr mod specific wpactrl rad evt hdlrs on reload

## Release v5.18.4 - 2023-12-13(07:58:15 +0000)

### Other

- keep vdr mod specific wpactrl evt hdlrs on reload

## Release v5.18.3 - 2023-12-13(07:35:15 +0000)

### Other

- fix hotspot2 obj validation

## Release v5.18.2 - 2023-12-12(18:05:47 +0000)

### Other

- fix chan_in_bands when bw auto

## Release v5.18.1 - 2023-12-12(15:50:05 +0000)

### Other

- keep radio dm status notPresent until mapped to hw device

## Release v5.18.0 - 2023-12-12(15:27:04 +0000)

### Other

- propagate wpactrl dfs/csa/cac events

## Release v5.17.1 - 2023-12-12(14:50:57 +0000)

### Other

- : Add InstanceName

## Release v5.17.0 - 2023-12-12(14:23:07 +0000)

### Other

- add public apis to manage rad macConfig

## Release v5.16.4 - 2023-12-11(14:08:57 +0000)

### Other

- - fix using wrong amx variant set value

## Release v5.16.3 - 2023-12-08(12:44:08 +0000)

### Other

- WiFi.AccessPoint.{x}.AssociatedDevice.{x}.Retransmission not updated

## Release v5.16.2 - 2023-12-08(11:37:25 +0000)

### Other

- [USP] Plugins that use the USP backend need to load it explicitly

## Release v5.16.1 - 2023-12-07(13:23:13 +0000)

### Other

- install ep config

## Release v5.16.0 - 2023-12-06(15:08:35 +0000)

### Other

- - add AcsBootChannel parameter to Radio.

## Release v5.15.3 - 2023-12-06(08:06:55 +0000)

## Release v5.15.2 - 2023-12-05(15:38:09 +0000)

### Other

- fill endpoint iface name/mac of radio with enabled STASupported_Mode

## Release v5.15.1 - 2023-12-05(13:37:45 +0000)

### Other

- use fta call isof direct fsm sched when adding vap iface
- chanmgt init with available chanspec from driver

## Release v5.15.0 - 2023-12-05(09:43:17 +0000)

### Other

- load var env for WLAN_ADDR

## Release v5.14.1 - 2023-12-04(13:06:35 +0000)

### Other

- auto calc rad baseMacAddress from env vars

## Release v5.14.0 - 2023-12-04(09:47:55 +0000)

### Other

- - handle and notify RRM beacon response and request status events

## Release v5.13.0 - 2023-12-01(10:28:43 +0000)

### Other

- add new public util APIs for vendor modules

## Release v5.12.1 - 2023-12-01(09:29:17 +0000)

### Other

- fix warnings in dm param validator on template object

## Release v5.12.0 - 2023-12-01(08:50:27 +0000)

### Other

- move fsm state sched out of wpactrl gen event handling

## Release v5.11.0 - 2023-11-30(09:28:44 +0000)

### Other

- add wpactrl eventHandlers to intercept redirect and modify msg

## Release v5.10.0 - 2023-11-28(14:09:27 +0000)

### Other

- - handle and notify channel switch and CSA finished events

## Release v5.9.6 - 2023-11-27(11:17:24 +0000)

### Other

- trigger RAD_CHANGE_INIT event after Rad DM be loaded

## Release v5.9.5 - 2023-11-27(09:56:07 +0000)

### Other

- [PPM-2628][osp] update ht/vht caps in hostapd conf with new target operChBw

## Release v5.9.4 - 2023-11-24(12:11:05 +0000)

### Other

- - BSSID not properly set when external config source

## Release v5.9.3 - 2023-11-24(11:58:37 +0000)

### Other

- correct wrong linux utils function declaration

## Release v5.9.2 - 2023-11-24(07:26:07 +0000)

## Release v5.9.1 - 2023-11-23(16:18:59 +0000)

### Other

- VAPs are down after endpoint activation and reconnection

## Release v5.9.0 - 2023-11-22(15:37:20 +0000)

### Other

- - Add eventing for ap and sta, and vendor data lists

## Release v5.8.0 - 2023-11-20(10:13:50 +0000)

### Other

- - handle and notify association failures

## Release v5.7.0 - 2023-11-17(16:08:56 +0000)

### Other

- - clean up channel functions

## Release v5.6.2 - 2023-11-17(14:03:20 +0000)

### Other

- - fixing small errors

## Release v5.6.1 - 2023-11-17(12:00:44 +0000)

### Other

- - Several band steering failures

## Release v5.6.0 - 2023-11-17(10:44:42 +0000)

### Other

- - add MLO capabilities

## Release v5.5.0 - 2023-11-17(10:02:00 +0000)

### Other

- - add WiFi 7 support

## Release v5.4.0 - 2023-11-14(18:26:36 +0000)

### Other

- : Add TLVs support

## Release v5.3.11 - 2023-11-14(16:29:31 +0000)

### Other

- - do not send scan notification in async mode.

## Release v5.3.10 - 2023-11-14(16:03:53 +0000)

### Other

- restore legacy wld_radio_updateAntenna

## Release v5.3.9 - 2023-11-14(14:56:55 +0000)

### Other

- load vendor modules from alternative default path

## Release v5.3.8 - 2023-11-14(14:06:39 +0000)

### Other

- fix scanStats dm notif

## Release v5.3.7 - 2023-11-14(12:35:57 +0000)

### Other

- direct set regdomain at boot to early update possible channels

## Release v5.3.6 - 2023-11-14(11:28:56 +0000)

### Other

- set direct label pin when hostapd connection is ready

## Release v5.3.5 - 2023-11-14(11:15:06 +0000)

### Other

- apply transaction only on DM configurable VAPs

## Release v5.3.4 - 2023-11-14(10:44:10 +0000)

### Other

- set AssociatedDevice stats params without transaction

## Release v5.3.3 - 2023-11-14(07:37:51 +0000)

### Other

- : Incorrect value for "WiFi.AccessPoint.[].MaxAssociatedDevices" after reboot

## Release v5.3.2 - 2023-11-13(18:01:46 +0000)

### Other

- set SSID/Radio stats params without transaction

## Release v5.3.1 - 2023-11-13(17:28:56 +0000)

### Other

- finalize VAP iface creation after referencing SSID and Radio

## Release v5.3.0 - 2023-11-13(12:15:07 +0000)

### Other

- - add ScanReason to ScanChange notif.

## Release v5.2.1 - 2023-11-09(12:02:23 +0000)

### Other

- - ChannelMgt sub-object not persistent

## Release v5.2.0 - 2023-11-07(15:21:28 +0000)

## Release v5.1.1 - 2023-11-07(14:00:48 +0000)

### Other

- : WiFi not showing when channel and bandwidth changes multiple times

## Release v5.1.0 - 2023-11-07(11:58:02 +0000)

### Other

- use of libamxo APIs to get parser config and connections

## Release v5.0.1 - 2023-10-27(10:10:32 +0000)

### Other

- - Radio alias invalid

## Release v5.0.0 - 2023-10-26(07:24:31 +0000)

### Other

- - ensure changes get propagated in the data model

## Release v4.21.0 - 2023-10-25(17:03:14 +0000)

### Other

- : Add relay credentials feature

## Release v4.20.4 - 2023-10-25(14:14:21 +0000)

### Other

- : Remove ProbeRequest handling

## Release v4.20.3 - 2023-10-25(14:03:03 +0000)

### Other

- : Remove ProbeRequest handling

## Release v4.20.2 - 2023-10-25(13:43:10 +0000)

### Other

- fix scan dep
- set wps uuid in wpa_supp conf

## Release v4.20.1 - 2023-10-23(15:21:18 +0000)

### Other

- [WiFi Sensing] handle API errors while adding new CSI client instance

## Release v4.20.0 - 2023-10-20(08:46:50 +0000)

### Other

- Add support for adding a monitor interface to a radio

## Release v4.19.4 - 2023-10-16(15:26:10 +0000)

### Other

- remove assumption of default rad channel

## Release v4.19.3 - 2023-10-16(13:47:21 +0000)

### Other

- : No management frame sent when channel equals 0
- [prpl][osp] fix 20s delay when applying any conf through FSM

## Release v4.19.2 - 2023-10-13(12:08:50 +0000)

### Other

- - DM reports sensing is enabled while it is not after station dis/re-association

## Release v4.19.1 - 2023-10-13(11:20:03 +0000)

## Release v4.19.0 - 2023-10-13(07:34:44 +0000)

### Other

- :WDS module for pwhm

## Release v4.18.6 - 2023-10-12(16:56:47 +0000)

### Other

- WiFi Reset Support on Router and Extender

## Release v4.18.5 - 2023-10-11(07:24:39 +0000)

### Other

- : Missing security mode "OWE" for Wifi guest

## Release v4.18.4 - 2023-10-10(15:03:06 +0000)

### Other

- fix getting NaSta stats

## Release v4.18.3 - 2023-10-10(08:20:59 +0000)

### Other

- : Remove VendorSpecific action frame notifications

## Release v4.18.2 - 2023-10-09(15:10:21 +0000)

### Other

- :WPS Methods - Self PIN is not working on both bands

## Release v4.18.1 - 2023-10-06(16:30:09 +0000)

## Release v4.18.0 - 2023-10-06(15:19:20 +0000)

### Other

- - Integrate pWHM on SOP

## Release v4.17.2 - 2023-10-06(08:27:30 +0000)

### Other

- - channel switch while SwitchDelayVideo is not over

## Release v4.17.1 - 2023-10-04(15:28:47 +0000)

### Other

- enable odl and doc check
- add support for guest vap on all radio in odl defaults

## Release v4.17.0 - 2023-10-04(09:21:52 +0000)

### Other

- - Add Radio firmware Version

## Release v4.16.0 - 2023-10-03(08:04:12 +0000)

### Other

- - send notification for received mgmt action frames

## Release v4.15.2 - 2023-10-02(11:35:24 +0000)

### Other

- : Add wld_nl80211_getEvtListenerHandlers for vendor module.

## Release v4.15.1 - 2023-09-29(13:30:10 +0000)

### Other

- - prplmesh Mxl - Factorise duplicated code lines in sub cmd callback function

## Release v4.15.0 - 2023-09-25(07:49:19 +0000)

### Other

- - Add AgileDFS API in startBgDfsClear

## Release v4.14.3 - 2023-09-22(07:00:49 +0000)

### Other

- SSID is not correctly applied

## Release v4.14.1 - 2023-09-21(14:12:58 +0000)

### Other

- update licences according to licence check

## Release v4.14.0 - 2023-09-18(14:01:44 +0000)

## Release v4.13.1 - 2023-09-18(13:01:24 +0000)

### Other

- - [Wifi sensing] use event handling to manage Radio.Sensing object

## Release v4.13.0 - 2023-09-15(12:05:22 +0000)

### Other

- - [Wifi sensing] implement control HL API
- WPS Methods - Client PIN is not working on both

## Release v4.12.0 - 2023-09-14(10:08:43 +0000)

### Other

- expose MaxSupportedSSIDs

## Release v4.11.0 - 2023-09-13(13:29:00 +0000)

### Other

- define capabilities needed by process

## Release v4.10.2 - 2023-09-13(10:50:11 +0000)

## Release v4.10.1 - 2023-09-12(13:42:26 +0000)

### Other

- Reload bss only

## Release v4.10.0 - 2023-09-11(10:07:02 +0000)

### Other

- fill scanned neighBss noise and spectrum chan nbColocAp

## Release v4.9.0 - 2023-09-07(15:29:20 +0000)

### Other

- [tr181-pcm] All the plugins that are register to PCM should set the pcm_svc_config

## Release v4.8.1 - 2023-09-07(07:48:11 +0000)

### Other

- - RadCaps not filled.

## Release v4.8.0 - 2023-09-05(09:27:58 +0000)

### Other

- Update scan and channel switch APIs

## Release v4.7.0 - 2023-09-05(09:14:41 +0000)

## Release v4.6.1 - 2023-09-04(14:22:14 +0000)

## Release v4.6.0 - 2023-08-29(12:52:08 +0000)

### Other

- [tr181-pcm] Set the usersetting parameters for each plugin

## Release v4.5.3 - 2023-08-28(12:10:18 +0000)

### Other

- : Add Ht/Vht/He capabilities from assoc request

## Release v4.5.2 - 2023-08-28(07:27:57 +0000)

### Other

- getSpectrumInfo() returns <NULL>
- No Probe Request received

## Release v4.5.1 - 2023-08-21(13:53:41 +0000)

### Other

- - Predict netlink message before allocation.

## Release v4.5.0 - 2023-08-11(12:33:17 +0000)

### Other

- : EasyMesh R2 4.2.1 - MBO Station Disallow support

## Release v4.4.0 - 2023-08-08(09:23:06 +0000)

### Other

- - Add getSpectrumInfo stats

## Release v4.3.1 - 2023-08-04(16:33:00 +0000)

### Other

- fix NaSta creation/deletion with dm rpcs

## Release v4.3.0 - 2023-08-04(15:42:26 +0000)

### Other

- add nl80211 util apis and make nl compat public

## Release v4.2.0 - 2023-08-02(18:37:49 +0000)

### Other

- add nl80211 api to do custom endpoint creation and configuration

## Release v4.1.1 - 2023-08-02(17:55:52 +0000)

### Other

- - fill missing remote BSSID in Endpoint wps pairingDoneSuccess notif

## Release v4.1.0 - 2023-08-02(16:16:55 +0000)

### Other

- - apply nl80211 scan passive and active dwell time

## Release v4.0.3 - 2023-08-02(15:52:15 +0000)

### Other

- - recover updated netdev index of vaps when missing nl80211 events

## Release v4.0.2 - 2023-07-28(12:33:45 +0000)

### Other

- improve netlink stats getting
- add missing noise value for scanned neighbor BSSs

## Release v4.0.1 - 2023-07-25(14:58:21 +0000)

### Other

- : Missing FTA setEvtHandlers call

## Release v4.0.0 - 2023-07-25(07:03:32 +0000)

### Other

- fix RNR IE in beacon when having default Discovery method

## Release v3.19.1 - 2023-07-21(07:46:59 +0000)

### Other

- restore old wld_ap_create proto still used in mod-whm

## Release v3.19.0 - 2023-07-21(07:14:18 +0000)

### Other

- : Add new configMap FTA

## Release v3.18.3 - 2023-07-20(08:53:37 +0000)

### Other

- use swla dm api to commit param with one transaction

## Release v3.18.2 - 2023-07-18(16:25:14 +0000)

### Other

- revert listening for usp sock in pwhm, causing crash

## Release v3.18.1 - 2023-07-18(12:24:10 +0000)

### Other

- : Add new Vendor attributes function

## Release v3.18.0 - 2023-07-17(14:48:08 +0000)

### Other

- - Channel Clearing information missing when BG_CAC

## Release v3.17.3 - 2023-07-17(14:23:08 +0000)

### Other

- MBO IE element is missing in the beacons

## Release v3.17.2 - 2023-07-17(12:56:02 +0000)

## Release v3.17.1 - 2023-07-05(12:57:54 +0000)

### Fixes

- Fix syntax in 30_wld-defaults-vaps.odl.uc

## Release v3.17.0 - 2023-06-29(07:26:04 +0000)

### Other

- [pwhm] Support for GCC 12.1 and removal of deprecated functions

## Release v3.16.3 - 2023-06-26(07:25:38 +0000)

### Other

- - Fix ME in header
- - rewrite swl_crypto for openssl 3.0 support
- add ref type fun for val types
- - sub second timestamps sometimes float in dm
- Update TBTT length values
- : Add management frame sending
- Generate gcovr txt report and upload reports to documentation server

## Release v3.16.2 - 2023-06-23(09:11:33 +0000)

### Other

- Unable to restart prplmesh_whm script

## Release v3.16.1 - 2023-06-16(07:48:36 +0000)

### Other

- : Add prb request event

## Release v3.16.0 - 2023-06-14(15:04:44 +0000)

### Other

- WFA Easy Mesh certification - 4.8.1 fail on client steering

## Release v3.15.2 - 2023-06-14(14:46:48 +0000)

### Other

- - pwhm: Propagate bBSS config to fBSS

## Release v3.15.1 - 2023-06-09(10:17:04 +0000)

### Other

- - MultiAP wps on EndPoint

## Release v3.15.0 - 2023-06-01(14:09:37 +0000)

### Other

- Add missing odl templates files

## Release v3.14.0 - 2023-05-31(17:04:09 +0000)

### Other

- - prplMesh Mxl - Add ap-force flag to trigger scan request

## Release v3.13.2 - 2023-05-30(09:47:34 +0000)

### Other

- - Create plugin's ACLs permissions

## Release v3.13.1 - 2023-05-26(09:05:37 +0000)

### Other

- use pwhm dm root name to fetch SSID Reference

## Release v3.13.0 - 2023-05-25(15:42:20 +0000)

### Other

- - Vendor events architecture

## Release v3.12.1 - 2023-05-25(13:50:08 +0000)

### Other

- Wrong Mode bg reported in AssociatedDevice when N sta connected

## Release v3.12.0 - 2023-05-24(13:59:15 +0000)

### Other

- - expose attribute management tools

## Release v3.10.3 - 2023-05-22(11:53:04 +0000)

### Other

- - Solve invalid hostapd conf on 6GHz

## Release v3.10.2 - 2023-05-17(08:08:26 +0000)

### Other

- persistent storage on parameters mark

## Release v3.10.1 - 2023-05-16(10:18:54 +0000)

## Release v3.10.0 - 2023-05-15(09:41:43 +0000)

## Release v3.9.0 - 2023-05-12(07:25:36 +0000)

## Release v3.8.0 - 2023-05-11(09:11:57 +0000)

### Other

- -Tests Failing dur to wrong "WiFi.AccessPoint.*.SSIDReference" 's mapping

## Release v3.7.3 - 2023-05-05(13:59:17 +0000)

### Other

- add detailed radioStatus to datamodel

## Release v3.7.2 - 2023-04-27(07:52:27 +0000)

### Other

- - solve uninitialised values from valgrind

## Release v3.7.1 - 2023-04-20(11:59:27 +0000)

### Other

- - Implementing delNeighbourAP and addNeighbourAP API using Linux Std APIs

## Release v3.7.0 - 2023-04-20(07:12:59 +0000)

### Fixes

- [odl]Remove deprecated odl keywords

### Other

- - Save wDevId value
- - Update wld_util.h and related source files

## Release v3.6.0 - 2023-04-14(18:25:53 +0000)

### Other

- move vap enabling code to fta isof fsm state

## Release v3.5.0 - 2023-04-14(16:36:41 +0000)

### Other

- make getLastAssocFrame rpc callable from AssocDev obj

## Release v3.4.3 - 2023-04-13(15:35:39 +0000)

### Other

- cleanup all APs assocDev lists when stopping hostapd

## Release v3.4.2 - 2023-04-13(11:39:51 +0000)

### Other

- update nl80211 iface event listener when ifIndex changes

## Release v3.4.1 - 2023-04-12(13:50:00 +0000)

### Other

- force stations to re-authenticate after vap ifaces enabling reconf

## Release v3.4.0 - 2023-04-12(12:24:39 +0000)

### Other

- : centralize internal ssid ctx creation

## Release v3.3.0 - 2023-04-12(10:54:40 +0000)

### Other

- add nl80211 api get all wiphy devices info

## Release v3.2.0 - 2023-04-07(15:58:11 +0000)

### Other

- allow external call of wpa_ctrl mgr event handlers

## Release v3.1.0 - 2023-04-07(15:48:13 +0000)

### Other

- move chanspec applying code from fsm to fta

## Release v3.0.3 - 2023-04-07(15:34:27 +0000)

### Other

- fix wps config methods in hapd conf

## Release v3.0.2 - 2023-04-07(08:20:33 +0000)

### Other

- support setting regDomain before starting hostapd
- fix regression in wifi neighbour scan

## Release v3.0.1 - 2023-04-03(13:16:06 +0000)

### Other

- - MACFiltering doesnt work

## Release v3.0.0 - 2023-04-03(12:53:14 +0000)

### Other

- - Update channel mgt and radio statistics

## Release v2.26.0 - 2023-03-29(12:14:58 +0000)

### Other

- - [prplMesh_WHM] Exposing VHT,HT and HE radio capabilities in the datamodel

## Release v2.25.0 - 2023-03-24(15:57:07 +0000)

### Other

- get radio air stats using nl80211 chan survey

## Release v2.24.2 - 2023-03-16(14:28:29 +0000)

### Other

- - Use SWL defined macro WPS notification

## Release v2.24.1 - 2023-03-16(12:47:52 +0000)

### Other

- fix missing btm response notification

## Release v2.24.0 - 2023-03-16(11:02:14 +0000)

### Other

- cleanup & improv saving stats in dm

## Release v2.23.3 - 2023-03-16(08:38:15 +0000)

### Other

- fix default enabling of 80211h on radio 5gHz

## Release v2.23.2 - 2023-03-13(09:23:11 +0000)

### Other

- remove initial arbitrary timer for dm conf loading

## Release v2.23.1 - 2023-03-13(09:03:16 +0000)

### Other

- turris-omnia: random: pwhm failed to start

## Release v2.23.0 - 2023-03-10(12:33:50 +0000)

### Other

- make configurable pwhm coredump generation

## Release v2.22.3 - 2023-03-03(15:15:38 +0000)

### Other

- deauth connected stations when disabling host rad/vap

## Release v2.22.2 - 2023-03-03(14:23:11 +0000)

### Other

- fix disabled endpoint interface name and status on boot

## Release v2.22.1 - 2023-03-02(07:49:30 +0000)

### Other

- Add Operating Classes for United States and Japan

## Release v2.22.0 - 2023-02-27(11:11:02 +0000)

### Other

- handle actions on vendor object hierarchy

## Release v2.21.1 - 2023-02-22(08:45:46 +0000)

## Release v2.21.0 - 2023-02-21(09:14:45 +0000)

### Other

- - add more options for IEEE80211v BTM request

## Release v2.20.0 - 2023-02-20(08:26:10 +0000)

### Other

- - IEEE80211k capabilities per station

## Release v2.19.2 - 2023-02-09(08:32:45 +0000)

### Other

- Remove the set of bridge interfaces

## Release v2.19.1 - 2023-02-08(12:35:53 +0000)

### Other

- [pwhm] fix splitted defaults loading

## Release v2.19.0 - 2023-02-07(08:27:34 +0000)

## Release v2.18.0 - 2023-02-06(12:29:05 +0000)

### Other

- - Add disassociation event

## Release v2.17.2 - 2023-02-03(10:26:20 +0000)

### Other

- [pwhm] fix iphone device authentication issue in wpa3 modes

## Release v2.17.1 - 2023-02-02(12:09:17 +0000)

### Other

- fix cleanup of obsolete assocDevs with invalid objs

## Release v2.17.0 - 2023-01-30(14:22:54 +0000)

### Other

- cleanup wld: replace wld securityMode with swl type
- adding the bssid as an argument to the scan function(pwhm+Nl802)

## Release v2.16.2 - 2023-01-27(12:05:17 +0000)

### Other

- Testing issue

## Release v2.16.1 - 2023-01-25(16:37:19 +0000)

### Changes

- [pwhm] use default dir iso one default file

## Release v2.16.0 - 2023-01-24(13:44:22 +0000)

### Other

- - cleanup and add testing

## Release v2.15.0 - 2023-01-18(14:54:22 +0000)

### Other

- add apis for nl80211 ap scan actions

## Release v2.14.1 - 2023-01-18(09:51:14 +0000)

### Other

- - RssiMonitoring issues fixed

## Release v2.14.0 - 2023-01-17(11:01:08 +0000)

### Other

- disable config opt check
- load datamodel extension definition and defaults

## Release v2.13.5 - 2023-01-06(18:21:48 +0000)

### Other

- Fix pwhm integration into feeds

## Release v2.13.4 - 2023-01-03(16:18:00 +0000)

### Other

- - Fix rpc argument types

## Release v2.13.3 - 2022-12-21(10:27:32 +0000)

### Other

- [prplMesh WHM] endpoint ProfileReference && RadioReference setting issue
- [prplMesh WHM] wps connection credentials are empty and parsing issue

## Release v2.13.2 - 2022-12-16(15:51:09 +0000)

### Other

- [prplMesh WHM] endpoint ConnectionStatus dm change notification is not sent

## Release v2.13.1 - 2022-12-13(16:10:35 +0000)

### Other

- Fix klocwork issues on WiFi components

## Release v2.13.0 - 2022-12-08(13:38:29 +0000)

### New

- [pwhm] use default dir iso one default file

## Release v2.12.1 - 2022-12-07(17:06:21 +0000)

### Other

- - Adding operating_class and channel for the NonAssociatedDevice object

## Release v2.12.0 - 2022-12-07(15:49:36 +0000)

### Other

- - Adding operating_class and channel for the NonAssociatedDevice object

## Release v2.11.1 - 2022-12-05(08:28:49 +0000)

### Other

- [pwhm] fix crash when getting sta stats after 2 disconnections
- remove squash commits as no needed anymore

## Release v2.11.0 - 2022-12-02(14:58:49 +0000)

### Other

- - [prplMesh M2] WiFi.NeighboringWiFiDiagnostic

## Release v2.10.0 - 2022-12-02(09:01:51 +0000)

### Other

- [pwhm][libwld] restore nl80211 unit tests

## Release v2.9.0 - 2022-11-29(12:03:46 +0000)

### Other

- support SSID LowerLayer TR181 compliant format

## Release v2.8.0 - 2022-11-28(14:26:26 +0000)

### Other

- [pwhm] ssid interface name set in dm with no amx notification

## Release v2.7.2 - 2022-11-25(11:13:06 +0000)

### Other

- [pwhm] crash on stop

## Release v2.7.1 - 2022-11-24(08:41:17 +0000)

### Other

- - [pwhm] Enabled secondary vaps status remain Down

## Release v2.7.0 - 2022-11-18(16:41:25 +0000)

### Other

- - [prplMesh WHM] Adding NonAssociatedDevice through createNonAssociatedDevice() does not work

## Release v2.6.0 - 2022-11-18(11:22:07 +0000)

### Other

- - Add rad & vap state statistics

## Release v2.4.2 - 2022-11-03(10:03:23 +0000)

### Other

- [prplMesh WHM] endpoint: setting the radio in managed (station) mode

## Release v2.4.1 - 2022-11-02(13:42:02 +0000)

### Other

- prplMesh - Endpoint support - stats

## Release v2.4.0 - 2022-10-25(10:09:28 +0000)

### Other

- - Add list of DFS marked channels to DFS event

## Release v2.3.1 - 2022-10-24(07:45:19 +0000)

### Other

- : pwhm - Station history is not working anymore

## Release v2.3.0 - 2022-10-20(06:58:55 +0000)

### Other

- Alias missing from WiFi Radio and SSID interface
- [prplMesh WHM] the endpoint connection is not working properly

## Release v2.2.5 - 2022-10-17(14:45:36 +0000)

### Other

- - Implementing endpoint/endpoint profiles instances

## Release v2.2.4 - 2022-10-17(12:51:47 +0000)

### Other

- squash commits before opensourcing them to make sahbot principal author for SoftAtHome deliveries
- [prpl] create an initial wifi debug script

## Release v2.2.3 - 2022-10-07(10:29:54 +0000)

### Other

- : Invalid MBO transition reason code

## Release v2.2.2 - 2022-10-07(07:47:30 +0000)

### Other

- build with -Wformat -Wformat-security

## Release v2.2.1 - 2022-10-06(16:34:08 +0000)

### Other

- - Fix wld compliation issue

## Release v2.2.0 - 2022-10-06(12:23:46 +0000)

### Other

- [prpl] code tweak for stats impl

## Release v2.1.2 - 2022-10-06(09:24:41 +0000)

### Other

- : Wrong RSSI value

## Release v2.1.1 - 2022-10-06(09:19:08 +0000)

### Other

- - Implementing endpoint Profile security

## Release v2.1.0 - 2022-10-05(09:38:36 +0000)

### Other

- - Endpoint support - WPS connection

## Release v2.0.2 - 2022-09-28(12:36:23 +0000)

### Other

- : Segfault wld_bcm

## Release v2.0.1 - 2022-09-28(11:22:06 +0000)

### Other

- Support for Vendor Specific nl80211 extensions

## Release v2.0.0 - 2022-09-27(08:16:58 +0000)

### Other

- [prpl] clean swl swla headers install

## Release v1.13.0 - 2022-09-26(16:25:34 +0000)

### Other

- support loading vendor modules

## Release v1.12.1 - 2022-09-26(13:25:42 +0000)

### Other

- - Add initial tests
- - Implement 11k using hostapd cmd/event

## Release v1.12.0 - 2022-09-22(09:49:27 +0000)

### Other

- Add channel noise function

## Release v1.11.4 - 2022-09-21(07:36:38 +0000)

### Other

- - Implementing wpa_supplicant fsm

## Release v1.11.3 - 2022-09-20(07:39:38 +0000)

### Other

- 6GHz discovery optimization

## Release v1.11.2 - 2022-09-09(09:39:46 +0000)

### Other

- [prpl] remove unused token in pwhm

## Release v1.11.1 - 2022-09-08(13:06:22 +0000)

### Other

- - FastStaReconnect catching to many events

## Release v1.11.0 - 2022-09-08(10:28:10 +0000)

## Release v1.10.1 - 2022-09-08(09:47:20 +0000)

### Other

- : Add public action frame API.

## Release v1.10.0 - 2022-09-07(14:14:35 +0000)

### Other

- improve wpactrl connections of MultiVAP

## Release v1.9.0 - 2022-09-07(10:37:06 +0000)

### Other

- manage secDmn and hostapd crash and stop

## Release v1.8.6 - 2022-09-06(17:13:55 +0000)

### Other

- add missing dependency tag of libswlc

## Release v1.8.5 - 2022-09-06(16:39:30 +0000)

### Other

- handle wps registrar config and triggering

## Release v1.8.4 - 2022-09-05(15:34:45 +0000)

### Other

- Enabling traces for WiFi component

## Release v1.8.3 - 2022-09-02(16:56:11 +0000)

### Other

- Enabling traces for WiFi component

## Release v1.8.2 - 2022-09-02(14:42:33 +0000)

### Other

- Enabling traces for WiFi component

## Release v1.8.1 - 2022-09-02(08:30:37 +0000)

### Other

- - Implementing get endpoint bssid api

## Release v1.8.0 - 2022-09-01(13:02:38 +0000)

## Release v1.7.9 - 2022-09-01(09:50:10 +0000)

### Other

- Implementing endpoint_disconnect api

## Release v1.7.8 - 2022-08-30(17:11:36 +0000)

### Other

- Add FortyMHz Intolerant bit in datamodel per STA

## Release v1.7.7 - 2022-08-30(17:04:52 +0000)

### Other

- emit the AP and the SSID status changed event

## Release v1.7.6 - 2022-08-29(12:52:50 +0000)

### Other

- emit the radio status changed event

## Release v1.7.5 - 2022-08-29(11:31:46 +0000)

### Other

- : Station statistic histograms

## Release v1.7.4 - 2022-08-29(10:01:15 +0000)

### Other

- - Add endpoint create/destroy hook apis

## Release v1.7.3 - 2022-08-29(09:05:17 +0000)

### Other

- [pWHM] getLastAssocReq api adjustment

## Release v1.7.2 - 2022-08-26(16:53:40 +0000)

### Other

- set dynamically vap security config
- [prpl] disable uci wireless through pwhm init script

## Release v1.7.1 - 2022-08-26(09:49:49 +0000)

### Other

- : [pwhm] detect radios by context and not by pci order

## Release v1.7.0 - 2022-08-18(09:42:12 +0000)

### Other

- [prplMesh WHM] the AP AssociatedDevice dm object change events are not sent over the bus

## Release v1.6.1 - 2022-08-18(09:39:14 +0000)

### Other

- schedule fsm actions to apply dynamic conf to hapd
- [prplMesh WHM] add additional info to the pwhm getLastAssocReq api

## Release v1.6.0 - 2022-08-08(16:16:23 +0000)

### Other

- creating/updating the wpa_supplicant configguration file

## Release v1.5.11 - 2022-08-05(10:31:07 +0000)

### Other

- fill missing station stats info

## Release v1.5.10 - 2022-08-01(07:51:03 +0000)

### Other

- - make amx getStationStats work

## Release v1.5.9 - 2022-07-29(11:54:44 +0000)

### Other

- clean up components.h header

## Release v1.5.8 - 2022-07-29(07:43:48 +0000)

### Other

- clean up components.h header

## Release v1.5.7 - 2022-07-28(08:00:27 +0000)

### Other

- : RssiEventing not reboot persistent

## Release v1.5.6 - 2022-07-19(11:46:54 +0000)

### Other

- ChannelChangeReason not properly working

## Release v1.5.5 - 2022-07-13(08:43:45 +0000)

### Other

- - Integrate prplMesh on Dish HW

## Release v1.5.4 - 2022-06-29(16:56:45 +0000)

### Other

- fix stability issues with pwhm amx on wnc board

## Release v1.5.3 - 2022-06-29(12:47:29 +0000)

### Other

- : Wrong disassoc value in BTM

## Release v1.5.2 - 2022-06-29(12:09:15 +0000)

### Other

- 2.4 GHz out of order after changing the channel

## Release v1.5.1 - 2022-06-28(16:23:52 +0000)

### Other

- fix amxc_string_clean crash at boot on wnc

## Release v1.5.0 - 2022-06-27(13:30:11 +0000)

### Other

- - add IEEE80211 frame parsing

## Release v1.4.6 - 2022-06-27(09:29:28 +0000)

### Other

- BSSID and Alias DM params remain empty after boot

## Release v1.4.5 - 2022-06-27(08:47:01 +0000)

### Other

- wld does not start automatically after reboot

## Release v1.4.4 - 2022-06-24(08:49:48 +0000)

### Other

- don't fail when /etc/environment doesn't exist

## Release v1.4.3 - 2022-06-23(08:40:52 +0000)

### Other

- - AutoCommitMgr not triggered on enable 0

## Release v1.4.2 - 2022-06-22(13:06:03 +0000)

### Other

- - [amx] SSID can't be modified when having a numrical value

## Release v1.4.1 - 2022-06-22(12:13:58 +0000)

### Other

- - [amx] Security cannot be modified

