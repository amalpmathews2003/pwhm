# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]


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

