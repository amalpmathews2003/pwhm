%populate {
    object WiFi {
        object SSID {
{% for ( let Itf in BD.Interfaces ) : if ( Itf.Type == "wireless" ) : %}
            instance add ("{{Itf.Alias}}") {
                parameter SSID = "{{Itf.SSID}}";
                parameter Enable = 0;
{% RadioIndex = BDfn.getRadioIndex(Itf.OperatingFrequency);
if (RadioIndex >= 0) : %}
                parameter LowerLayers = "Device.WiFi.Radio.{{RadioIndex + 1}}.";
{% endif %}
            }
{% endif; endfor; %}
        }
        object AccessPoint {
{% let i = 0 %}
{% for ( let Itf in BD.Interfaces ) : if ( Itf.Type == "wireless" ) : %}
{% i++ %}
{% if ((BDfn.isInterfaceGuest(Itf.Name)) && (Itf.OperatingFrequency == "2.4GHz")) : %}
            instance add ("{{Itf.Alias}}") {
                parameter SSIDReference = "Device.WiFi.SSID.{{i}}.";
                parameter Enable = 0;
                parameter DefaultDeviceType = "Guest";
                object Security {
                    parameter ModesAvailable = "None,WEP-64,WEP-128,WPA-Personal,WPA2-Personal,WPA-WPA2-Personal,WPA3-Personal,WPA2-WPA3-Personal";
                    parameter WEPKey = "123456789ABCD";
                    parameter PreSharedKey = "";
                    parameter KeyPassPhrase = "passwordGuest";
                    parameter RekeyingInterval = 0;
                    parameter SAEPassphrase = "";
                    parameter EncryptionMode = "Default";
                    parameter ModeEnabled = "WPA2-Personal";
                }
                object WPS {
                    parameter Enable = 0;
                    parameter ConfigMethodsEnabled = "PhysicalPushButton,VirtualPushButton,VirtualDisplay,PIN";
                    parameter Configured = 1;
                }
            }
{% elif ((BDfn.isInterfaceGuest(Itf.Name)) && (Itf.OperatingFrequency == "5GHz")) : %}
            instance add ("{{Itf.Alias}}") {
                parameter SSIDReference = "Device.WiFi.SSID.{{i}}.";
                parameter Enable = 0;
                parameter DefaultDeviceType = "Guest";
                object Security {
                    parameter ModesAvailable = "None,WEP-64,WEP-128,WPA-Personal,WPA2-Personal,WPA-WPA2-Personal,WPA3-Personal,WPA2-WPA3-Personal";
                    parameter WEPKey = "123456789ABCD";
                    parameter PreSharedKey = "";
                    parameter KeyPassPhrase = "passwordGuest";
                    parameter RekeyingInterval = 0;
                    parameter SAEPassphrase = "";
                    parameter EncryptionMode = "Default";
                    parameter ModeEnabled = "WPA2-Personal";
                }
                object WPS {
                    parameter Enable = 0;
                    parameter ConfigMethodsEnabled = "PhysicalPushButton,VirtualPushButton,VirtualDisplay,PIN";
                    parameter Configured = 1;
                }
            }
{% elif ((BDfn.isInterfaceGuest(Itf.Name)) && (Itf.OperatingFrequency == "6GHz")) : %}

{% elif (Itf.OperatingFrequency == "2.4GHz") : %}
            instance add ("{{Itf.Alias}}") {
                parameter SSIDReference = "Device.WiFi.SSID.{{i}}.";
                parameter Enable = 0;
                parameter DefaultDeviceType = "Data";
                parameter MultiAPType = "FronthaulBSS,BackhaulBSS";
                object Security {
                    parameter ModesAvailable = "None,OWE,WEP-64,WEP-128,WEP-128iv,WPA-Personal,WPA2-Personal,WPA-WPA2-Personal,WPA3-Personal,WPA2-WPA3-Personal,E-None,WPA-Enterprise,WPA2-Enterprise,WPA-WPA2-Enterprise";
                    parameter WEPKey = "123456789ABCD";
                    parameter PreSharedKey = "";
                    parameter KeyPassPhrase = "password";
                    parameter RekeyingInterval = 0;
                    parameter OWETransitionInterface = "";
                    parameter SAEPassphrase = "";
                    parameter EncryptionMode = "Default";
                    parameter SHA256Enable = 0;
                    parameter RadiusServerIPAddr = "";
                    parameter RadiusServerPort = 1812;
                    parameter RadiusSecret = "";
                    parameter RadiusDefaultSessionTimeout = 0;
                    parameter RadiusNASIdentifier = "";
                    parameter RadiusCalledStationId = "";
                    parameter RadiusChargeableUserId = 0;
                    parameter ModeEnabled = "WPA2-Personal";
                }
                object WPS {
                    parameter Enable = 1;
                    parameter ConfigMethodsEnabled = "PhysicalPushButton,VirtualPushButton,VirtualDisplay,PIN";
                    parameter Configured = 1;
                }
            }
{% elif (Itf.OperatingFrequency == "5GHz") : %}
            instance add ("{{Itf.Alias}}") {
                parameter SSIDReference = "Device.WiFi.SSID.{{i}}.";
                parameter Enable = 0;
                parameter DefaultDeviceType = "Data";
                parameter WDSEnable = 1;
                parameter MultiAPType = "FronthaulBSS,BackhaulBSS";
                object Security {
                    parameter ModesAvailable = "None,OWE,WEP-64,WEP-128,WEP-128iv,WPA-Personal,WPA2-Personal,WPA-WPA2-Personal,WPA3-Personal,WPA2-WPA3-Personal,E-None,WPA-Enterprise,WPA2-Enterprise,WPA-WPA2-Enterprise";
                    parameter WEPKey = "123456789ABCD";
                    parameter PreSharedKey = "";
                    parameter KeyPassPhrase = "password";
                    parameter RekeyingInterval = 0;
                    parameter OWETransitionInterface = "";
                    parameter SAEPassphrase = "";
                    parameter EncryptionMode = "Default";
                    parameter SHA256Enable = 0;
                    parameter RadiusServerIPAddr = "";
                    parameter RadiusServerPort = 1812;
                    parameter RadiusSecret = "";
                    parameter RadiusDefaultSessionTimeout = 0;
                    parameter RadiusNASIdentifier = "";
                    parameter RadiusCalledStationId = "";
                    parameter RadiusChargeableUserId = 0;
                    parameter ModeEnabled = "WPA2-Personal";
                }
                object WPS {
                    parameter Enable = 1;
                    parameter ConfigMethodsEnabled = "PhysicalPushButton,VirtualPushButton,VirtualDisplay,PIN";
                    parameter Configured = 1;
                }
            }
{% elif (Itf.OperatingFrequency == "6GHz") : %}
            instance add ("{{Itf.Alias}}") {
                parameter SSIDReference = "Device.WiFi.SSID.{{i}}.";
                parameter Enable = 1;
                parameter DefaultDeviceType = "Data";
                parameter IEEE80211kEnabled = 1;
                parameter WDSEnable = 1;
                parameter MultiAPType = "FronthaulBSS,BackhaulBSS";
                object Security {
                    parameter ModesAvailable = "WPA3-Personal";
                    parameter WEPKey = "123456789ABCD";
                    parameter PreSharedKey = "";
                    parameter KeyPassPhrase = "password";
                    parameter RekeyingInterval = 0;
                    parameter OWETransitionInterface = "";
                    parameter SAEPassphrase = "";
                    parameter EncryptionMode = "Default";
                    parameter SHA256Enable = 0;
                    parameter RadiusServerIPAddr = "";
                    parameter RadiusServerPort = 1812;
                    parameter RadiusSecret="";
                    parameter RadiusDefaultSessionTimeout = 0;
                    parameter RadiusNASIdentifier="";
                    parameter RadiusCalledStationId = "";
                    parameter RadiusChargeableUserId = 0;
                    parameter ModeEnabled = "WPA3-Personal";
                    parameter SPPAmsdu = 0;
                }
            }
{% endif %}
{% endif; endfor; %}
        }
    }
}
