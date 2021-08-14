# ESPSense
Use ESPHome to create virtual TP-Link Kasa HS110 plugs, that report energy usage to your Sense Home Energy Monitor

Similar to the [SenseLink](https://github.com/cbpowell/SenseLink) project, ESPSense is an [ESPHome](https://esphome.io) custom component that emulates the protocol of [TP-Link Kasa HS110](https://www.tp-link.com/us/home-networking/smart-plug/hs110/) energy monitoring plugs. This lets you use your own ESP devices to report energy usage **directly** to your [Sense Home Energy Monitor](https://sense.com/)!

**You should use this tool at your own risk!** Sense is not obligated to provide any support related to issues with this project, and there's no guarantee everything will reliably work, or even work at all. Neither I or Sense can guarantee it won't affect your Sense data, particularly if things go wrong!

# Confirmed Compatible Smart Plugs
One of the more useful cases is flashing other (commercial) energy-monitoring smart plugs with ESPHome, and then using them with ESPSense with no other integration required. Check out [the wiki](https://github.com/cbpowell/ESPSense/wiki) for details on confirmed "conversions" of other plugs, and an [OTA flash guide](https://github.com/cbpowell/ESPSense/wiki/Flashing-ESPHome-via-OTA)!

The focus on the wiki is for plugs that are re-flashable "over the air" for simplicity, but if you're comfortable with soldering (and opening the plug) there are  an incredible number of compatible plugs/devices compatible with ESPHome.

# Usage
Modify/create your ESPHome YAML definition to include:
1. an `external_component` directive, that specifies this component
2. a top level `espsense` directive, to configure the ESPSense component by specifying which ESPHome sensor(s) to utilize for power data for each plug (note: these can also be [template sensors](https://esphome.io/components/sensor/template.html) that return a wattage value!)

From the included example YAML file:

```yaml
external_components:
  # Pull the esphome component in from this GitHub repo
  - source: github://cbpowell/ESPSense
    components: [ espsense ]

# Template sensor as an example
sensor:
  - platform: template
    name: Test Sensor
    id: test_sensor
    unit_of_measurement: W
  
espsense:
  # You can define up to 10 "plugs" to report to Sense
  # Power value can come from any of the following:
  #   * A power sensor (in Watts)
  #   * Calculated from a current sensor (in Amps) and a voltage sensor (in Volts)
  #   * Calculated from a current sensor (in Amps) and a fixed voltage value
  plugs:
    - name: espsense
      power_sensor: test_sensor
      # current_sensor: some_current_sensor
      # voltage_sensor: some_voltage_sensor
      # voltage: 120.0
      # encrypt: false
      # mac_address: 35:4B:91:A1:FE:CC
```
### Power Sensor
Note that whatever sensor you tell ESPSense to monitor is assumed to report a **state in the units of watts!** If you want to report the power usage of a device indirectly (such as scaled on another parameter, or simply if on or off), you'll need to create a template sensor in ESPHome to calculate/report the wattage.

### MAC Address
By default, the first plug defined will use the hardware MAC address of your device, if no MAC is explicitly configured. If additional plugs are defined (on the same hardware device) and no specific MAC is configured for those, a MAC address will be automatically generated for each from a hash of the provided plug name.

### Voltage and Current
Sense does not currently care about plug voltage or current readings, but this is implemented to support data collection by things other than Sense, or in case Sense does eventually implement it!

### Encryption
TP-Link plugs use a light "encryption" of the transmitted data, and the Sense monitor does expect to receive the data in encrypted form, so generally you will want to leave the `encrypt` setting as default (true). However you can specify to disable encryption if desired, which could be utilize for your own custom data collection approaches.


Copyright 2020, Charles Powell
