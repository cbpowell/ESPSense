import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_NAME, CONF_VOLTAGE, CONF_MAC_ADDRESS

AUTO_LOAD = ["json"]

CONF_PLUGS = "plugs"
CONF_POWER_SENSOR = "power_sensor"
CONF_CURRENT_SENSOR = "current_sensor"
CONF_VOLTAGE_SENSOR = "voltage_sensor"
CONF_ENCRYPT = "encrypt"

json_ns = cg.esphome_ns.namespace("json")

espsense_ns = cg.esphome_ns.namespace("espsense")
ESPSense = espsense_ns.class_("ESPSense", cg.Component)
ESPSensePlug = espsense_ns.class_("ESPSensePlug")

def validate_plug_config(config):
    if CONF_POWER_SENSOR in config:
        return config
    elif CONF_CURRENT_SENSOR in config and CONF_VOLTAGE_SENSOR in config:
        return config
    elif CONF_CURRENT_SENSOR in config and CONF_VOLTAGE in config:
        return config

    raise cv.Invalid('invalid plug config')
    
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPSense),
            cv.Required(CONF_PLUGS): cv.All(
                cv.ensure_list(
                    {
                        cv.GenerateID(): cv.declare_id(ESPSensePlug),
                        cv.Required(CONF_NAME): cv.string,
                        cv.Optional(CONF_POWER_SENSOR): cv.use_id(sensor.Sensor),
                        cv.Optional(CONF_CURRENT_SENSOR): cv.use_id(sensor.Sensor),
                        cv.Optional(CONF_VOLTAGE_SENSOR): cv.use_id(sensor.Sensor),
                        cv.Optional(CONF_VOLTAGE, default="120.0"): cv.positive_float,
                        cv.Optional(CONF_ENCRYPT, default="true"): cv.boolean,
                        cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
                    },
                    validate_plug_config,
                ),
                cv.Length(min=1)
            )
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for plug_config in config[CONF_PLUGS]:
        plug_var = cg.new_Pvariable(plug_config[CONF_ID])
        cg.add(plug_var.set_name(plug_config[CONF_NAME]))
        if CONF_POWER_SENSOR in plug_config:
            power_sensor = await cg.get_variable(plug_config[CONF_POWER_SENSOR])
            cg.add(plug_var.set_power_sensor(power_sensor))
        if CONF_CURRENT_SENSOR in plug_config:
            current_sensor = await cg.get_variable(plug_config[CONF_CURRENT_SENSOR])
            cg.add(plug_var.set_current_sensor(current_sensor))
        if CONF_VOLTAGE_SENSOR in plug_config:
            voltage_sensor = await cg.get_variable(plug_config[CONF_VOLTAGE_SENSOR])
            cg.add(plug_var.set_voltage_sensor(voltage_sensor))
        if CONF_MAC_ADDRESS in plug_config:
            cg.add(plug_var.set_mac_address(plug_config[CONF_MAC_ADDRESS]))
        cg.add(plug_var.set_voltage(plug_config[CONF_VOLTAGE]))
        cg.add(plug_var.set_encrypt(plug_config[CONF_ENCRYPT]))
        cg.add(var.addPlug(plug_var))