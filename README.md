# ESPSense
Use ESPHome to create virtual TP-Link Kasa HS110 plugs, that report energy usage to your Sense Home Energy Monitor

Similar to the [SenseLink](https://github.com/cbpowell/SenseLink) project, ESPSense is an [ESPHome](https://esphome.io) custom component that emulates the protocol of [TP-Link Kasa HS110](https://www.tp-link.com/us/home-networking/smart-plug/hs110/) energy monitoring plugs. This lets you use your own ESP devices to report energy usage **directly** to your [Sense Home Energy Monitor](https://sense.com/)!

**You should use this tool at your own risk!** Sense is not obligated to provide any support related to issues with this project, and there's no guarantee everything will reliably work, or even work at all. Neither I or Sense can guarantee it won't affect your Sense data, particularly if things go wrong!

# Confirmed Compatible Smart Plugs
One of the more useful cases is flashing other (commercial) energy-monitoring smart plugs with ESPHome, and then using them with ESPSense with no other integration required. Check out [the wiki](https://github.com/cbpowell/ESPSense/wiki) for details on confirmed "conversions" of other plugs, and an [OTA flash guide](https://github.com/cbpowell/ESPSense/wiki/Flashing-ESPHome-via-OTA)!

The focus on the wiki is for plugs that are re-flashable "over the air" for simplicity, but if you're comfortable with soldering (and opening the plug) there are  an incredible number of compatible plugs/devices compatible with ESPHome.

# Usage
Place the `espsense.h` [custom component](https://esphome.io/custom/custom_component.html) in your ESPHome build directory, and add the `custom_component` details and lambda to your YAML as per the ESPHome directions - from the included example YAML file:

```yaml
esphome:
  name: espsense
  ...
  # Include espsense.h custom component file
  includes:
    - espsense.h
  # Uses the libraries:
  # - ESPAsyncUDP, to handle UDP communication
  # - ArduinoJson-esphomelib, which might already be included if using the ESPHome webserver
  libraries:
    - "ESPAsyncUDP"
    - "ArduinoJson-esphomelib@5.13.3"
    

# Template sensor as an example
sensor:
  - platform: template
    name: Test Sensor
    id: test_sensor
    unit_of_measurement: W
  
custom_component:
  # Create ESPSense instance, passing the ID of the sensor it should retrieve
  # power data from
- lambda: |-
    float voltage = 120.0;
    auto sensor_power = new ESPSense(id(test_sensor), voltage);
    return {sensor_power};
```

Note that whatever sensor you tell ESPSense to monitor is assumed to report a **state in the units of watts!** If you want to report the power usage of a device indirectly (such as scaled on another parameter, or simply if on or off), you'll need to create a template sensor in ESPHome to calculate/report the wattage.

By default, the MAC address of your ESP device will be reported to Sense. You can configure that manually if desired. See the `espsense.h` file for the alternate constructor methods.

# Todo
1. Currently ESPSense can only report power usage of a single sensor - adding multiple instances as additional `custom_components` doesn't work (I think because the UDP port is consumed by the first instance). I plan to add the ability to define an array of plugs, similar to the way SenseLink works.

Copyright 2020, Charles Powell
