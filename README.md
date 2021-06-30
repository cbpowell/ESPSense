# ESPSense
Use ESPHome to create virtual TP-Link Kasa HS110 plugs, that report energy usage to your Sense Home Energy Monitor

Similar to the [SenseLink](https://github.com/cbpowell/SenseLink) project, ESPSense is an [ESPHome](https://esphome.io) custom component that emulates the protocol of [TP-Link Kasa HS110](https://www.tp-link.com/us/home-networking/smart-plug/hs110/) energy monitoring plugs. This lets you use your own ESP devices to report energy usage **directly** to your [Sense Home Energy Monitor](https://sense.com/)!

**You should use this tool at your own risk!** Sense is not obligated to provide any support related to issues with this project, and there's no guarantee everything will reliably work, or even work at all. Neither I or Sense can guarantee it won't affect your Sense data, particularly if things go wrong!

# Confirmed Compatible Smart Plugs
One of the more useful cases is flashing other (commercial) energy-monitoring smart plugs with ESPHome, and then using them with ESPSense with no other integration required. Check out [the wiki](https://github.com/cbpowell/ESPSense/wiki) for details on confirmed "conversions" of other plugs, and an [OTA flash guide](https://github.com/cbpowell/ESPSense/wiki/Flashing-ESPHome-via-OTA)!

The focus on the wiki is for plugs that are re-flashable "over the air" for simplicity, but if you're comfortable with soldering (and opening the plug) there are  an incredible number of compatible plugs/devices compatible with ESPHome.

# Usage
Place the `espsense.h` [custom component](https://esphome.io/custom/custom_component.html) in your ESPHome build directory, and then modify/create your ESPHome YAML definition to include:
1. the `custom_component` details and lambda, to tell ESPSense which ESPHome sensor to utilize for power data (note: this can also be a [template sensor](https://esphome.io/components/sensor/template.html) that returns a wattage value!)
2. an `includes:` directive, calling out the `espsense.h` file
3. a `libraries:` directive, that specifies to include the libraries `ArduinoJson-esphome@5.13.3` and `ESPAsyncUDP` for ESP8266-based devices. **Note**: for ESP32-based devices do *not* include `ESPAsyncUDP`, the core library `AsyncUDP` is utilized instead.

rom the included example YAML file:

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
    - "ESPAsyncUDP"   # do not include if using an ESP32 board or device!
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

## Advanced Usage
If you're using a device that measures power/voltage/current of more than one thing, ESPSense also supports reporting back multiple "plugs" to Sense. In the custom component lambda, you can configure each plug and add it to the ESPSense component. The plug `voltage_sid` and `current_sid` sensor ID values are optional, and ESPSense will fall back to using the specified static voltage and calculating current from power / voltage, respectively, if not specified.
```yaml
custom_component:
  # Create ESPSense instance, passing the ID of the sensor it should retrieve
  # power data from
- lambda: |-
    // Define "plugs"
    ESPSensePlug plug1 = ESPSensePlug(id(power_sensor1), "50:c7:bf:f6:2b:01", "plug1", 120.0);
    ESPSensePlug plug2 = ESPSensePlug(id(power_sensor2), "50:c7:bf:f6:2b:02", "plug2", 120.0);
    // Plug1 also measures voltage
    plug1.voltage_sid = voltage_sensor1; // ID of voltage sensor
    // Add to ESPSense
    auto sense = new ESPSense();
    sense->addPlug(plug1);
    sense->addPlug(plug2);
    return {sense};
```
Sense does not currently care about plug voltage or current readings, but this is implemented to support data collection by things other than Sense, or in case Sense does eventually implement it!


Copyright 2020, Charles Powell
