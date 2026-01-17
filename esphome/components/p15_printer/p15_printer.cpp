#ifdef USE_ESP32

#include "p15_printer.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace p15_printer {

static const char *const TAG = "p15_printer";

// P15 Printer Commands
static const uint8_t CMD_PREFIX[] = {0x1F, 0x70, 0x02, 0x06};
static const uint8_t CMD_WAKEUP[15] = {0};
static const uint8_t CMD_ENABLE[] = {0x10, 0xFF, 0xF1, 0x02};
static const uint8_t CMD_STOP[] = {0x1D, 0x0C, 0x10, 0xFF, 0xF1, 0x45};

void P15Printer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up P15 Printer...");
  this->init_buffer_();
}

void P15Printer::init_buffer_() {
  this->stride_ = (this->paper_width_dots_ + 7) / 8;
  uint32_t buffer_size = this->stride_ * this->paper_height_dots_;
  this->init_internal_(buffer_size);
  if (this->buffer_ != nullptr) {
    memset(this->buffer_, 0, buffer_size);
  }
  ESP_LOGD(TAG, "Buffer initialized: %dx%d dots, stride=%d, size=%u bytes", this->paper_width_dots_,
           this->paper_height_dots_, this->stride_, buffer_size);
}

void P15Printer::dump_config() {
  ESP_LOGCONFIG(TAG, "P15 Thermal Printer:");
  ESP_LOGCONFIG(TAG, "  Paper Size: %dmm x %dmm (%dx%d dots)", this->paper_width_mm_, this->paper_height_mm_,
                this->paper_width_dots_, this->paper_height_dots_);
  ESP_LOGCONFIG(TAG, "  Buffer Stride: %d bytes/row", this->stride_);
  LOG_DISPLAY("  ", "Display", this);
}

void P15Printer::update() {
  // Call the display writer lambda to update the buffer
  this->do_update_();
  // NOTE: We do NOT automatically send to printer here!
  // User must call send_to_printer() explicitly
}

void P15Printer::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= this->paper_width_dots_ || y < 0 || y >= this->paper_height_dots_) {
    return;
  }
  if (this->buffer_ == nullptr) {
    return;
  }

  int byte_idx = y * this->stride_ + (x / 8);
  int bit_idx = 7 - (x % 8);

  if (color.is_on()) {
    this->buffer_[byte_idx] |= (1 << bit_idx);
  } else {
    this->buffer_[byte_idx] &= ~(1 << bit_idx);
  }
}

void P15Printer::send_data_(const uint8_t *data, size_t len) {
  if (!this->is_connected() || this->char_handle_tx_ == 0) {
    ESP_LOGW(TAG, "Cannot send data: not connected or characteristic not found");
    return;
  }

  // Use fixed chunk size for BLE transmission (safe default)
  size_t chunk_size = 20;

  for (size_t i = 0; i < len; i += chunk_size) {
    size_t current_len = std::min(chunk_size, len - i);
    esp_err_t status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                                this->char_handle_tx_, current_len, const_cast<uint8_t *>(data + i),
                                                ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);

    if (status != ESP_OK) {
      ESP_LOGW(TAG, "Failed to write BLE data: %d", status);
      return;
    }
    delay(5);  // Small delay between chunks
  }
}

void P15Printer::send_to_printer() {
  if (!this->is_connected()) {
    ESP_LOGW(TAG, "Cannot print: not connected to printer");
    return;
  }

  if (this->buffer_ == nullptr) {
    ESP_LOGW(TAG, "Cannot print: buffer not initialized");
    return;
  }

  ESP_LOGI(TAG, "Sending bitmap to printer...");

  // Send prefix and wakeup commands
  this->send_data_(CMD_PREFIX, sizeof(CMD_PREFIX));
  this->send_data_(CMD_WAKEUP, sizeof(CMD_WAKEUP));
  this->send_data_(CMD_ENABLE, sizeof(CMD_ENABLE));

  // Build bitmap header: GS v 0 m xL xH yL yH
  // xL xH = bytes per row (stride)
  // yL yH = number of rows (height in dots)
  uint16_t x_bytes = this->stride_;
  uint16_t y_dots = this->paper_height_dots_;

  uint8_t bmp_header[] = {0x1D,
                          0x76,
                          0x30,
                          0x00,
                          static_cast<uint8_t>(x_bytes & 0xFF),
                          static_cast<uint8_t>((x_bytes >> 8) & 0xFF),
                          static_cast<uint8_t>(y_dots & 0xFF),
                          static_cast<uint8_t>((y_dots >> 8) & 0xFF)};

  this->send_data_(bmp_header, sizeof(bmp_header));

  // Send bitmap data
  size_t buffer_size = this->stride_ * this->paper_height_dots_;
  this->send_data_(this->buffer_, buffer_size);

  // Send stop command
  this->send_data_(CMD_STOP, sizeof(CMD_STOP));

  ESP_LOGI(TAG, "Bitmap sent to printer (%zu bytes)", buffer_size);
}

bool P15Printer::discover_characteristics_() {
  auto *chr = this->parent_->get_characteristic(P15_SERVICE_UUID, P15_CHAR_TX_UUID);
  if (chr == nullptr) {
    ESP_LOGW(TAG, "TX characteristic not found, not a P15 printer?");
    return false;
  }
  this->char_handle_tx_ = chr->handle;
  ESP_LOGD(TAG, "Found TX characteristic, handle=0x%04X", this->char_handle_tx_);
  return true;
}

void P15Printer::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                     esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected to P15 printer");
      } else {
        ESP_LOGW(TAG, "Failed to connect to P15 printer: %d", param->open.status);
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGI(TAG, "Disconnected from P15 printer");
      this->char_handle_tx_ = 0;
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Service discovery failed: %d", param->search_cmpl.status);
        break;
      }

      if (this->discover_characteristics_()) {
        ESP_LOGI(TAG, "P15 printer ready");
        this->node_state = espbt::ClientState::ESTABLISHED;
      }
      break;
    }

    default:
      break;
  }
}

}  // namespace p15_printer
}  // namespace esphome

#endif  // USE_ESP32
