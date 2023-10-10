%populate {
    object WiFi {
        object SSID {
{% for ( let Itf in BD.Interfaces ) : if ( Itf.Type == "wireless" ) : %}
{% if ((Itf.OperatingFrequency == "2.4GHz") || (Itf.OperatingFrequency == "5GHz") || (Itf.OperatingFrequency == "6GHz")) : %}
            instance add ("{{Itf.Alias}}") {
{% if ( Itf.SSID != "" ) : %}
                parameter SSID = "{{Itf.SSID}}";
{% endif %}
                parameter Enable = 0;
{% RadioIndex = BDfn.getRadioIndex(Itf.OperatingFrequency);
if (RadioIndex >= 0) : %}
                parameter LowerLayers = "Device.WiFi.Radio.{{RadioIndex + 1}}.";
{% endif %}
            }
{% endif %}
{% endif; endfor; %}
        }
        object AccessPoint {
{% let i = 0 %}
{% for ( let Itf in BD.Interfaces ) : if ( Itf.Type == "wireless" ) : %}
{% if ((Itf.OperatingFrequency == "2.4GHz") || (Itf.OperatingFrequency == "5GHz") || (Itf.OperatingFrequency == "6GHz")) : %}
{% i++ %}
{% if ( Itf.SSID != "" ) : %}
            instance add ("{{Itf.Alias}}") {
                parameter SSIDReference = "Device.WiFi.SSID.{{i}}.";
                parameter Enable = 0;
{% if (BDfn.isInterfaceGuest(Itf.Name)) : %}
                parameter DefaultDeviceType = "Guest";
{% elif (BDfn.isInterfaceLan(Itf.Name)) : %}
                parameter IEEE80211kEnabled = 1;
                parameter MultiAPType = "FronthaulBSS,BackhaulBSS";
{% if ((Itf.OperatingFrequency == "5GHz") || (Itf.OperatingFrequency == "6GHz")) : %}
                parameter WDSEnable = 1;
{% endif %}
{% endif %}
                object Security {
                    parameter RekeyingInterval = 0;
{% if (Itf.OperatingFrequency == "6GHz") : %}
                    parameter ModesAvailable = "WPA3-Personal";
                    parameter ModeEnabled = "WPA3-Personal";
                    parameter SAEPassphrase = "";
                    parameter SPPAmsdu = 0;
{% else %}
                    parameter ModeEnabled = "WPA2-Personal";
{% endif %}
{% if (BDfn.isInterfaceGuest(Itf.Name)) : %}
                    parameter KeyPassPhrase = "passwordGuest";
{% else %}
                    parameter KeyPassPhrase = "password";
{% endif %}
                }
                object WPS {
                    parameter ConfigMethodsEnabled = "PhysicalPushButton,VirtualPushButton,Display,VirtualDisplay,PIN";
                    parameter Configured = 1;
{% if (BDfn.isInterfaceLan(Itf.Name)) : %}
                    parameter Enable = 1;
{% endif %}
                }
            }
{% endif %}
{% endif %}
{% endif; endfor; %}
        }
    }
}
