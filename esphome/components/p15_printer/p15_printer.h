#pragma once

#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/component.h"

#include <esp_gattc_api.h>

namespace esphome {
namespace p15_printer {

namespace espbt = esphome::esp32_ble_tracker;

// P15 Printer BLE UUIDs
static const espbt::ESPBTUUID P15_SERVICE_UUID = espbt::ESPBTUUID::from_raw("0000ff00-0000-1000-8000-00805f9b34fb");
static const espbt::ESPBTUUID P15_CHAR_TX_UUID = espbt::ESPBTUUID::from_raw("0000ff02-0000-1000-8000-00805f9b34fb");

class P15Printer : public display::DisplayBuffer, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  /// Override update to NOT automatically send to printer
  void update() override;

  /// Send the current display buffer to the printer via BLE
  void send_to_printer();

  /// Check if connected to printer
  bool is_connected() const { return this->node_state == espbt::ClientState::ESTABLISHED; }

  /// Set paper dimensions in millimeters
  void set_paper_width(int width_mm) {
    this->paper_width_mm_ = width_mm;
    this->paper_width_dots_ = width_mm * 8;
  }
  void set_paper_height(int height_mm) {
    this->paper_height_mm_ = height_mm;
    this->paper_height_dots_ = height_mm * 8;
  }

  /// Get paper dimensions
  int get_paper_width_mm() const { return this->paper_width_mm_; }
  int get_paper_height_mm() const { return this->paper_height_mm_; }

  /// BLEClientNode overrides
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  /// Display type - binary (1-bit monochrome)
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

 protected:
  /// DisplayBuffer implementation
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_height_internal() override { return this->paper_height_dots_; }
  int get_width_internal() override { return this->paper_width_dots_; }

  /// Initialize the display buffer
  void init_buffer_();

  /// Send data to printer via BLE with chunking
  void send_data_(const uint8_t *data, size_t len);

  /// Discover BLE characteristics
  bool discover_characteristics_();

  // Paper configuration
  int paper_width_mm_{12};
  int paper_height_mm_{40};
  int paper_width_dots_{96};    // 12mm * 8 dots/mm
  int paper_height_dots_{320};  // 40mm * 8 dots/mm

  // BLE handles
  uint16_t char_handle_tx_{0};

  // Buffer stride (bytes per row)
  int stride_{0};
};

}  // namespace p15_printer
}  // namespace esphome

#endif  // USE_ESP32
