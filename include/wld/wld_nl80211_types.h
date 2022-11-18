/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/
/*
 * This file includes nl80211 type definitions used in api and event callbacks
 */

#ifndef INCLUDE_WLD_WLD_NL80211_TYPES_H_
#define INCLUDE_WLD_WLD_NL80211_TYPES_H_

#include <stdint.h>

/*
 * @brief value for undefined nl80211 wiphy/iface ID
 * (eg. when not included in msg, or when unknown)
 * (iface net id is greater than 0)
 * (wiphy id is positive)
 */
#define WLD_NL80211_ID_UNDEF ((uint32_t) -1)
/*
 * @brief value for any nl80211 wiphy/iface ID
 * (eg. when need to match any received wiphy/iface id)
 */
#define WLD_NL80211_ID_ANY ((uint32_t) -2)

#include "wld.h"
#include <swl/swl_common_trilean.h>
#include <swl/swl_common_mcs.h>

typedef struct {
    uint32_t chanWidth;       //channel width in MHz
    uint32_t ctrlFreq;        //frequency of the control channel in MHz
    uint32_t centerFreq1;     //Center frequency of the first part of the channel for bw > 20
    uint32_t centerFreq2;     //Center frequency of the second part of the channel, bw 80+80
} wld_nl80211_chanSpec_t;

typedef struct {
    uint32_t wiphy;                  //wiphy id
    uint32_t ifIndex;                //net dev index
    char name[IFNAMSIZ];             //net dev name
    swl_macBin_t mac;                //interface mac
    bool isMain;                     //flag when interface is main (primVap, mainEP)
    bool isAp;                       //flag when interface is accesspoint
    bool isSta;                      //flag when interface is endpoint
    bool use4Mac;                    //enabled 4mac mode
    wld_nl80211_chanSpec_t chanSpec; //channel info
    uint32_t txPower;                //current tx power in mbm
    char ssid[SSID_NAME_LEN];        //bss/ess ssid
} wld_nl80211_ifaceInfo_t;

typedef enum {
    WLD_NL80211_CHAN_STATUS_UNKNOWN, //unknown status
    WLD_NL80211_CHAN_DISABLED,       //channel disabled for this regulatory domain
    WLD_NL80211_CHAN_USABLE,         //channel might be used but requires to be cleared (CAC)
    WLD_NL80211_CHAN_UNAVAILABLE,    //channel marked as not available because a radar has been detected on it.
    WLD_NL80211_CHAN_AVAILABLE,      //channel is available for transmission (also means dfs cleared)
    WLD_NL80211_CHAN_STATUS_MAX,
} wld_nl80211_chanStatus_e;
extern const char* g_str_wld_nl80211_chan_status_bf_cap[];

typedef struct {
    uint32_t ctrlFreq;               //control channel frequency in MHz
    bool isDfs;                      //flag set when channel is DFS
    wld_nl80211_chanStatus_e status; //channel status (including DFS)
    uint32_t dfsTime;                //time in miliseconds for how long this channel is in this DFS state.
    uint32_t dfsCacTime;             //DFS CAC time in milliseconds
    uint32_t maxTxPwr;               //Maximum transmission power in dBm.
} wld_nl80211_chanDesc_t;

typedef struct {
    swl_freqBand_e freqBand;                                 //frequency band
    swl_radioStandard_m radStdsMask;                         //supported radio standards mask
    swl_bandwidth_m chanWidthMask;                           //supported channel bandwidths
    uint32_t nSSMax;                                         //max number of spatial streams
    uint32_t nChans;                                         //number of available channels
    wld_nl80211_chanDesc_t chans[WLD_MAX_POSSIBLE_CHANNELS]; //array of available channels
    wld_rad_bf_cap_m bfCapsSupported[COM_DIR_MAX];           //which beamforming capabilities are available
    swl_mcs_t mcsStds[SWL_MCS_STANDARD_MAX];                 //support mcs standards (each indexed on its relative enum)
} wld_nl80211_bandDef_t;

typedef struct {
    bool channelSwitch; // support Channel Switch Announcement
    bool survey;        // support Survey API
    bool WMMCapability; // support WME
} wld_nl80211_wiphySuppCmds;

typedef struct {
    bool dfsOffload; // driver will do all DFS-related actions by itself.
    bool sae;        // driver supports Simultaneous Authentication of Equals (SAE) with user space SME
    bool sae_pwe;    // driver supports SAE PWE derivation in WPA3-Personal networks which are using SAE authentication.
} wld_nl80211_wiphySuppFeatures;

#define WLD_NL80211_CIPHERS_MAX 14                  //(Cf: IEEE80211 Table 9-180—Cipher suite selectors: defined suite types 0..14)
typedef struct {
    uint32_t genId;                                 //info generation id (unique for all wiphy info received chunks)
    uint32_t wiphy;                                 //wiphy id
    char name[IFNAMSIZ];                            //wiphy name
    uint16_t maxScanSsids;                          //max number of scan ssid filters
    uint16_t maxScanIeLen;                          //max length of extra IE used in scan
    int32_t nrAntenna[COM_DIR_MAX];                 //number of antennas available. -1 means undefined
    int32_t nrActiveAntenna[COM_DIR_MAX];           //number of antennas active. -1 means undefined
    bool suppUAPSD;                                 //flag set when the device supports uapsd when working as AP. */
    bool suppAp;                                    //flag set when device supports AP mode
    bool suppEp;                                    //flag set when device supports Endpoint mode (aka. managed)
    uint32_t nApMax;                                //max number of BSS (AP) that can be created
    uint32_t nEpMax;                                //max number of EP (managed) that can be created
    uint32_t nStaMax;                               //max number of clients that can be associated (<=0 if unknown)
    uint32_t nSSMax;                                //max number of spatial streams of all supported bands
    swl_freqBand_m freqBandsMask;                   //available freq band mask
    wld_nl80211_bandDef_t bands[SWL_FREQ_BAND_MAX]; //available freq bands (each indexed on its relative enum)
    uint32_t nCipherSuites;                         //number of supported cipher suites
    uint32_t cipherSuites[WLD_NL80211_CIPHERS_MAX]; //supported cipher suites (Cf: IEEE80211 Table 9-180—Cipher suite selectors)
    wld_nl80211_wiphySuppFeatures suppFeatures;     //supported optional nl80211 features by the driver
    wld_nl80211_wiphySuppCmds suppCmds;             //supported optional nl80211 commands by the driver
} wld_nl80211_wiphyInfo_t;

typedef struct {
    uint32_t bitrate;  // total bitrate (kbps) (u16/u32)
    swl_mcs_t mcsInfo; // mcs info
    uint8_t heDcm;     // HE DCM value (u8, 0/1)
} wld_nl80211_rateInfo_t;

typedef struct {
    swl_trl_e authorized;    // station is authorized (802.1X)
    swl_trl_e authenticated; // station is authenticated
    swl_trl_e associated;    // station is associated:
                             // used to transition a previously added station into associated state
    swl_trl_e wme;           // station is WME/QoS capable
    swl_trl_e mfp;           // station uses management frame protection
} wld_nl80211_stationFlags_t;

typedef struct {
    swl_macBin_t macAddr;                     // station's address MAC
    uint32_t inactiveTime;                    // time since last activity (u32, milliseconds)
    uint64_t rxBytes;                         // total received bytes
    uint64_t txBytes;                         // total transmitted bytes
    uint32_t rxPackets;                       // total received packet (MSDUs and MMPDUs)
    uint32_t txPackets;                       // total transmitted packets (MSDUs and MMPDUs)
    uint32_t txRetries;                       // total retries (MPDUs)
    uint32_t txFailed;                        // total failed packets
    int8_t rssiDbm;                           // signal strength of last received PPDU (dBm)
    int8_t rssiAvgDbm;                        // signal strength average (dBm)
    wld_nl80211_rateInfo_t txRate;            // tx rate (kbps), nested attributes
    wld_nl80211_rateInfo_t rxRate;            // rx rate (kbps), nested attributes

    uint32_t connectedTime;                   // time since the station is last connected (u32, seconds)
    wld_nl80211_stationFlags_t flags;         // retrieved optional station flags
    uint32_t nrSignalChains;                  // nb chains on which signal is detected
    int8_t rssiDbmByChain[MAX_NR_ANTENNA];    // per-chain signal strength of last PPDU (dBm)
    int8_t rssiAvgDbmByChain[MAX_NR_ANTENNA]; // per-chain signal strength average (dBm)

} wld_nl80211_stationInfo_t;

#endif /* INCLUDE_WLD_WLD_NL80211_TYPES_H_ */
