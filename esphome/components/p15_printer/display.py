from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_client, display
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA

CODEOWNERS = ["@your_username"]
DEPENDENCIES = ["ble_client"]

CONF_PAPER_WIDTH = "paper_width"
CONF_PAPER_HEIGHT = "paper_height"

p15_printer_ns = cg.esphome_ns.namespace("p15_printer")
P15Printer = p15_printer_ns.class_(
    "P15Printer",
    display.DisplayBuffer,
    ble_client.BLEClientNode,
)

# Action for printing
P15PrintAction = p15_printer_ns.class_("P15PrintAction", automation.Action)

CONFIG_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(P15Printer),
            cv.Optional(CONF_PAPER_WIDTH, default=12): cv.int_range(min=1, max=100),
            cv.Optional(CONF_PAPER_HEIGHT, default=40): cv.int_range(min=1, max=500),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await ble_client.register_ble_node(var, config)

    cg.add(var.set_paper_width(config[CONF_PAPER_WIDTH]))
    cg.add(var.set_paper_height(config[CONF_PAPER_HEIGHT]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
