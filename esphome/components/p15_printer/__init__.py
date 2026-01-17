from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@your_username"]
DEPENDENCIES = ["ble_client"]

p15_printer_ns = cg.esphome_ns.namespace("p15_printer")
P15Printer = p15_printer_ns.class_("P15Printer")

# Action for printing
P15PrintAction = p15_printer_ns.class_("P15PrintAction", automation.Action)

# Action schema for p15_printer.print
P15_PRINT_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(P15Printer),
    }
)


@automation.register_action(
    "p15_printer.print", P15PrintAction, P15_PRINT_ACTION_SCHEMA
)
async def p15_print_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
