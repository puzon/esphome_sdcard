#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "p15_printer.h"

namespace esphome {
namespace p15_printer {

template<typename... Ts> class P15PrintAction : public Action<Ts...>, public Parented<P15Printer> {
  void play(const Ts &...x) override { this->parent_->send_to_printer(); }
};

}  // namespace p15_printer
}  // namespace esphome
