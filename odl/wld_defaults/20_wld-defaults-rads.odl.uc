%populate {
    object WiFi {
        object Radio {
{% for ( let Radio in BD.Radios ) : %}
{% if (Radio.OperatingFrequency == "2.4GHz") : %}
            instance add ("{{Radio.Alias}}") {
                parameter OperatingFrequencyBand = "2.4GHz";
                parameter Enable = 1;
                parameter AutoChannelEnable = 0;
                parameter Channel = 1;
                parameter RegulatoryDomain = "DE";
                parameter AP_Mode = 1;
                parameter AirtimeFairnessEnabled = 1;
                parameter IntelligentAirtimeSchedulingEnable = 1;
                parameter RxPowerSaveEnabled = 1;
                parameter RetryLimit = 7;
                parameter BeaconPeriod=100;
                parameter DTIMPeriod=3;
                parameter TargetWakeTimeEnable = 1;
                parameter RxBeamformingCapsEnabled = "DEFAULT";
                parameter TxBeamformingCapsEnabled = "DEFAULT";
                parameter ObssCoexistenceEnable = 1;
                parameter ProbeRequestNotify = "AlwaysRSSI";
                parameter OperatingStandards = "auto";
                parameter OperatingChannelBandwidth = "20MHz";
                object MACConfig {
                    parameter UseBaseMacOffset = true;
                    parameter BaseMacOffset = 1;
                    parameter UseLocalBitForGuest = true;
                    parameter LocalGuestMacOffset = 65536;
                }
            }
{% elif (Radio.OperatingFrequency == "5GHz") : %}
            instance add ("{{Radio.Alias}}") {
                parameter OperatingFrequencyBand = "5GHz";
                parameter Enable = 1;
                parameter AutoChannelEnable = 0;
                parameter Channel = 36;
                parameter IEEE80211hEnabled = true;
                parameter RegulatoryDomain = "DE";
                parameter AP_Mode = 1;
                parameter WDS_Mode = 1;
                parameter AirtimeFairnessEnabled = 1;
                parameter IntelligentAirtimeSchedulingEnable = 1;
                parameter MultiUserMIMOEnabled = 1;
                parameter RxPowerSaveEnabled = 1;
                parameter RetryLimit = 7;
                parameter BeaconPeriod=100;
                parameter DTIMPeriod=3;
                parameter TargetWakeTimeEnable = 1;
                parameter RxBeamformingCapsEnabled = "DEFAULT";
                parameter TxBeamformingCapsEnabled = "DEFAULT";
                parameter MCS = -1;
                object RadCaps {
                    parameter Enabled = "DFS_AHEAD DELAY_COMMIT";
                }
                parameter ProbeRequestNotify = "AlwaysRSSI";
                parameter OperatingStandards = "auto";
                parameter AutoBandwidthSelectMode="MaxAvailable";
                parameter OperatingChannelBandwidth="Auto";
                object MACConfig {
                    parameter UseBaseMacOffset = true;
                    parameter BaseMacOffset = 2;
                    parameter UseLocalBitForGuest = true;
                    parameter LocalGuestMacOffset = 65536;
                }
            }
{% elif (Radio.OperatingFrequency == "6GHz") : %}
            instance add ("{{Radio.Alias}}") {
                parameter OperatingFrequencyBand = "6GHz";
                parameter Enable = 1;
                parameter AutoChannelEnable = 0;
                parameter Channel = 5;
                parameter IEEE80211hEnabled = true;
                parameter RegulatoryDomain = "DE";
                parameter AP_Mode = 1;
                parameter WDS_Mode = 1;
                parameter AirtimeFairnessEnabled = 1;
                parameter IntelligentAirtimeSchedulingEnable = 1;
                parameter MultiUserMIMOEnabled = 1;
                parameter RxPowerSaveEnabled = 1;
                parameter RetryLimit = 7;
                parameter BeaconPeriod = 100;
                parameter DTIMPeriod = 3;
                parameter HeCapsEnabled = "DL_OFDMA,UL_OFDMA,DL_MUMIMO";
                parameter TargetWakeTimeEnable = 1;
                parameter RxBeamformingCapsEnabled = "DEFAULT";
                parameter TxBeamformingCapsEnabled = "DEFAULT";
                parameter MCS = -1;
                object RadCaps {
                    parameter Enabled = "DFS_AHEAD DELAY_COMMIT";
                }
                parameter ProbeRequestNotify = "AlwaysRSSI";
                parameter OperatingStandards = "auto";
                parameter AutoBandwidthSelectMode="MaxAvailable";
                parameter OperatingChannelBandwidth="Auto";
                object MACConfig {
                    parameter UseBaseMacOffset = true;
                    parameter BaseMacOffset = 3;
                }
            }
{% endif; endfor; %}
        }
    }
}
