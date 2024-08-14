%populate {
    object WiFi {
{% let BssId = 0 %}
        object SSID {
{% for ( let Itf in BD.Interfaces ) : if ( BDfn.isInterfaceWirelessEp(Itf.Name) ) : %}
{% RadioIndex = BDfn.getRadioIndex(Itf.OperatingFrequency); if (RadioIndex >= 0) : %}
{% BssId = BDfn.getInterfaceIndex(Itf.Name, "wireless") %}
            instance add ({{BssId + 1}}, "{{Itf.Alias}}") {
                parameter LowerLayers = "Device.WiFi.Radio.{{RadioIndex + 1}}.";
            }
{% endif %}
{% endif; endfor; %}
        }
        object EndPoint {
{% for ( let Itf in BD.Interfaces ) : if ( BDfn.isInterfaceWirelessEp(Itf.Name) ) : %}
{% RadioIndex = BDfn.getRadioIndex(Itf.OperatingFrequency); if (RadioIndex >= 0) : %}
{% BssId = BDfn.getInterfaceIndex(Itf.Name, "wireless") %}
            instance add ("{{Itf.Alias}}") {
                parameter SSIDReference = "Device.WiFi.SSID.{{BssId + 1}}.";
                parameter Enable = 0;
                parameter MultiAPEnable = 1;
                object WPS {
                    parameter Enable = 1;
                    parameter ConfigMethodsEnabled = "PhysicalPushButton,VirtualPushButton,VirtualDisplay,PIN";
                }
            }
{% endif %}
{% endif; endfor; %}
        }
    }
}
