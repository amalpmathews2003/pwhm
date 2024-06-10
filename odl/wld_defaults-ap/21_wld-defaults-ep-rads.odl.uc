%populate {
    object WiFi {
        object Radio {
{% for ( let Itf in BD.Interfaces ) : if ( BDfn.isInterfaceWirelessEp(Itf.Name) ) : %}
{% if (Itf.OperatingFrequency != "") : %}
{% RadioIndex = BDfn.getRadioIndex(Itf.OperatingFrequency);
if (RadioIndex >= 0) : %}
            object '{{BD.Radios[RadioIndex].Alias}}' {
                parameter STA_Mode = 1;
                parameter STASupported_Mode = 1;
            }
{% endif %}
{% endif %}
{% endif; endfor; %}
        }
    }
}
