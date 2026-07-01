from operator import xor
from functools import reduce

import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_PORT,
)
from esphome import pins
from esphome.components.network import IPAddress
from enum import IntEnum, Enum
from esphome.components import uart, socket, sensor, binary_sensor
from esphome.types import ConfigType

AUTO_LOAD = ["sensor", "binary_sensor", "climate"]
DEPENDENCIES = ["logger"]

nibegw_ns = cg.esphome_ns.namespace("nibegw")
NibeGwComponent = nibegw_ns.class_("NibeGwComponent", cg.Component, uart.UARTDevice)
NibeGwEcs = nibegw_ns.class_("NibeGwEcs", cg.Component)

CONF_DIR_PIN = "dir_pin"
CONF_TARGET = "target"
CONF_TARGET_PORT = "port"
CONF_TARGET_IP = "ip"
CONF_ACKNOWLEDGE = "acknowledge"
CONF_UDP = "udp"

CONF_ACKNOWLEDGE_MODBUS40 = "modbus40"
CONF_ACKNOWLEDGE_RMU40 = "rmu40"
CONF_ACKNOWLEDGE_SMS40 = "sms40"
CONF_READ_PORT = "read_port"
CONF_WRITE_PORT = "write_port"
CONF_PORTS = "ports"
CONF_SOURCE = "source"
CONF_ADDRESS = "address"
CONF_TOKEN = "token"
CONF_COMMAND = "command"
CONF_DATA = "data"
CONF_CONSTANTS = "constants"
CONF_ECS = "ecs"
CONF_BT2_RAW = "bt2_raw"
CONF_BT3_RAW = "bt3_raw"
CONF_BT50_RAW = "bt50_raw"
CONF_BT2_RAW_DEFAULT = "bt2_raw_default"
CONF_BT3_RAW_DEFAULT = "bt3_raw_default"
CONF_BT50_RAW_DEFAULT = "bt50_raw_default"
CONF_MSG1_BINARY_SENSOR = "msg1_binary_sensor"
CONF_MSG1_BINARY_BYTE = "msg1_binary_byte"
CONF_MSG1_BINARY_MASK = "msg1_binary_mask"


class Addresses(IntEnum):
    AXC40 = 0x05
    POOL310 = 0x06
    SCA35 = 0x0A
    MODBUS40 = 0x20
    SMS40 = 0x16
    RMU40_S1 = 0x19
    RMU40_S2 = 0x1A
    RMU40_S3 = 0x1B
    RMU40_S4 = 0x1C
    DEH500 = 0x27
    DEH310 = 0x27
    EME20 = 0xA4
    ECS_S2 = 0x02
    ECS_S3 = 0x03

class Token(IntEnum):
    MODBUS_READ = 0x69
    MODBUS_WRITE = 0x6B
    RMU_WRITE = 0x60
    RMU_DATA = 0x63
    ACCESSORY = 0xEE
    ECS_DATA_REQ = 0x90
    ECS_DATA_MSG_1 = 0x55
    ECS_DATA_MSG_2 = 0xA0
    ECS_DATA_A3 = 0xA3
    ECS_DATA_A4 = 0xA4
    ECS_DATA_AF = 0xAF

def addresses_string(value):
    try:
        return Addresses[value].value
    except KeyError:
        raise ValueError(f"{value} is not a valid member of Address")


def real_enum(enum: Enum):
    return cv.enum({name: item.value for name, item in enum.__members__.items()})


def _consume_nibegw_sockets(config: ConfigType) -> ConfigType:
    """Register socket needs for nibegw component."""
    # MQTT needs 1 socket for the broker connection
    udp = config[CONF_UDP]
    socket_count = len(udp[CONF_PORTS])
    socket.consume_sockets(socket_count, "nibegw")(config)
    return config


def _upgrade_ports(config: ConfigType) -> ConfigType:
    udp = config[CONF_UDP]
    if port_number := udp[CONF_WRITE_PORT]:
        udp[CONF_PORTS].insert(
            0,
            PORTS_SCHEMA(
                {
                    CONF_PORT: port_number,
                    CONF_ADDRESS: Addresses.MODBUS40.value,
                    CONF_TOKEN: Token.MODBUS_WRITE.value,
                }
            ),
        )
    if port_number := udp[CONF_READ_PORT]:
        udp[CONF_PORTS].insert(
            0,
            PORTS_SCHEMA(
                {
                    CONF_PORT: port_number,
                    CONF_ADDRESS: Addresses.MODBUS40.value,
                    CONF_TOKEN: Token.MODBUS_READ.value,
                }
            ),
        )

    return config


CONSTANTS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.Any(real_enum(Addresses), int),
        cv.Required(CONF_TOKEN): cv.Any(real_enum(Token), int),
        cv.Optional(CONF_COMMAND): cv.Any(real_enum(Token), int),
        cv.Required(CONF_DATA): [int],
    }
)

TARGET_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TARGET_IP): cv.ipv4address,
        cv.Optional(CONF_TARGET_PORT, default=9999): cv.port,
    }
)

PORTS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PORT): cv.port,
        cv.Required(CONF_ADDRESS): cv.Any(real_enum(Addresses), int),
        cv.Required(CONF_TOKEN): cv.Any(real_enum(Token), int),
    }
)

UDP_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TARGET, []): cv.ensure_list(TARGET_SCHEMA),
        cv.Optional(CONF_READ_PORT, default=9999): cv.port,
        cv.Optional(CONF_WRITE_PORT, default=10000): cv.port,
        cv.Optional(CONF_SOURCE, []): cv.ensure_list(cv.ipv4address),
        cv.Optional(CONF_PORTS, []): cv.ensure_list(PORTS_SCHEMA),
    }
)

ECS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NibeGwEcs),
        cv.Optional(CONF_ADDRESS, default=Addresses.ECS_S3.value): cv.Any(
            real_enum(Addresses), int
        ),
        cv.Optional(CONF_BT2_RAW): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_BT3_RAW): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_BT50_RAW): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_BT2_RAW_DEFAULT, default=704): cv.int_range(min=0, max=1023),
        cv.Optional(CONF_BT3_RAW_DEFAULT, default=724): cv.int_range(min=0, max=1023),
        cv.Optional(CONF_BT50_RAW_DEFAULT, default=1023): cv.int_range(min=0, max=1023),
        cv.Optional(CONF_MSG1_BINARY_SENSOR): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_MSG1_BINARY_BYTE, default=0): cv.int_range(min=0, max=127),
        cv.Optional(CONF_MSG1_BINARY_MASK, default=0): cv.int_range(min=0, max=255),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NibeGwComponent),
            cv.Optional(CONF_ACKNOWLEDGE, default=[]): [
                cv.Any(addresses_string, cv.Coerce(int))
            ],
            cv.Required(CONF_UDP): UDP_SCHEMA,
            cv.Optional(CONF_DIR_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_CONSTANTS, default=[]): cv.ensure_list(CONSTANTS_SCHEMA),
            cv.Optional(CONF_ECS, default=[]): cv.ensure_list(ECS_SCHEMA),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
    _upgrade_ports,
    _consume_nibegw_sockets,
)


async def to_code(config):
    if dir_pin := config.get(CONF_DIR_PIN):
        dir_pin_data = await cg.gpio_pin_expression(dir_pin)
    else:
        dir_pin_data = 0

    var = cg.new_Pvariable(
        config[CONF_ID],
        dir_pin_data,
    )
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if udp := config.get(CONF_UDP):
        for target in udp[CONF_TARGET]:
            cg.add(
                var.add_target(
                    IPAddress(str(target[CONF_TARGET_IP])), target[CONF_TARGET_PORT]
                )
            )

        for port in udp[CONF_PORTS]:
            cg.add(
                var.add_socket_request(
                    port[CONF_ADDRESS], port[CONF_TOKEN], port[CONF_PORT]
                )
            )

        for source in udp[CONF_SOURCE]:
            cg.add(var.add_source_ip(IPAddress(str(source))))

    if config[CONF_ACKNOWLEDGE]:
        for address in config[CONF_ACKNOWLEDGE]:
            cg.add(var.add_acknowledge(address))

    def xor8(data: bytes) -> int:
        chksum = reduce(xor, data)
        if chksum == 0x5C:
            chksum = 0xC5
        return chksum

    def generate_request(command: int, data: list[int]) -> list[int]:
        packet = [0xC0, command, len(data), *data]
        packet.append(xor8(packet))
        return packet

    for request in config[CONF_CONSTANTS]:
        cmd = request.get(CONF_COMMAND, request[CONF_TOKEN])

        if hasattr(cmd, "enum_value"):
            cmd = cmd.enum_value

        data = generate_request(
            cmd,
            request[CONF_DATA],
        )
        cg.add(var.set_request(request[CONF_ADDRESS], request[CONF_TOKEN], data))

    for ecs in config[CONF_ECS]:
        ecs_var = cg.new_Pvariable(ecs[CONF_ID])
        await cg.register_component(ecs_var, ecs)
        cg.add(ecs_var.set_gw(var))
        cg.add(ecs_var.set_address(ecs[CONF_ADDRESS]))
        cg.add(ecs_var.set_bt2_raw_default(ecs[CONF_BT2_RAW_DEFAULT]))
        cg.add(ecs_var.set_bt3_raw_default(ecs[CONF_BT3_RAW_DEFAULT]))
        cg.add(ecs_var.set_bt50_raw_default(ecs[CONF_BT50_RAW_DEFAULT]))
        cg.add(ecs_var.set_msg1_binary_byte(ecs[CONF_MSG1_BINARY_BYTE]))
        cg.add(ecs_var.set_msg1_binary_mask(ecs[CONF_MSG1_BINARY_MASK]))

        if CONF_BT2_RAW in ecs:
            sens = await cg.get_variable(ecs[CONF_BT2_RAW])
            cg.add(ecs_var.set_bt2_raw_sensor(sens))

        if CONF_BT3_RAW in ecs:
            sens = await cg.get_variable(ecs[CONF_BT3_RAW])
            cg.add(ecs_var.set_bt3_raw_sensor(sens))

        if CONF_BT50_RAW in ecs:
            sens = await cg.get_variable(ecs[CONF_BT50_RAW])
            cg.add(ecs_var.set_bt50_raw_sensor(sens))

        if CONF_MSG1_BINARY_SENSOR in ecs:
            sens = await cg.get_variable(ecs[CONF_MSG1_BINARY_SENSOR])
            cg.add(ecs_var.set_msg1_binary_sensor(sens))
