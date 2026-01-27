#include <algorithm>
#include <cstring>

#include "USBDevice/DeviceDriver/XInput/XboxOne.h"
#include "device/usbd_pvt.h"

// Internal TinyUSB Driver Logic for Xbox One (Vendor Specific)
namespace tud_xboxone {

static constexpr uint16_t ENDPOINT_SIZE = 64;

// GIP Protocol Constants
static constexpr uint8_t GIP_CMD_ACK = 0x01;
static constexpr uint8_t GIP_CMD_ANNOUNCE = 0x02;
static constexpr uint8_t GIP_CMD_IDENTIFY = 0x04;
static constexpr uint8_t GIP_CMD_POWER = 0x05;
static constexpr uint8_t GIP_CMD_AUTHENTICATE = 0x06;
static constexpr uint8_t GIP_CMD_VIRTUAL_KEY = 0x07;
static constexpr uint8_t GIP_CMD_RUMBLE = 0x09;
static constexpr uint8_t GIP_CMD_LED = 0x0A;
static constexpr uint8_t GIP_CMD_INPUT = 0x20;

static constexpr uint8_t GIP_OPT_ACK = 0x10;
static constexpr uint8_t GIP_OPT_INTERNAL = 0x20;

// GIP Device Descriptor Response (0x04) - sent when host requests identify
static const uint8_t IDENTIFY_RESPONSE[] = {
    GIP_CMD_IDENTIFY, // command = 0x04
    GIP_OPT_INTERNAL, // flags: internal=1
    0x00,             // sequence (updated when sending)
    0x14,             // length (20 bytes payload)
    // Device descriptor payload
    0x00, 0x01, // Descriptor version
    0x5E, 0x04, // VendorID (045E - Microsoft) LE
    0xEA, 0x02, // ProductID (02EA - Xbox One) LE
    0x19, 0x01, // Firmware version (1.19)
    0x00, 0x01, // Hardware version (1.0)
    // Interface GUIDs / Class info (simplified)
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// GIP Announce Packet (sent by device on connection)
// Based on Xbox One controller structure
static const uint8_t ANNOUNCE_PACKET[] = {
    GIP_CMD_ANNOUNCE, // command
    GIP_OPT_INTERNAL, // client=0, needsAck=0, internal=1, chunk=0
    0x00,             // sequence
    0x14,             // length (20 bytes payload)
    // Payload - Device identification
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Serial/ID (8 bytes)
    0x00, 0x00,                                     // VendorID (filled by host)
    0x00, 0x00,                                     // ProductID
    0x00, 0x01,                                     // Firmware version
    0x00, 0x01,                                     // Hardware version
    0x00, 0x00, 0x00, 0x00                          // Reserved
};

static uint8_t endpoint_in_ = 0xFF;
static uint8_t endpoint_out_ = 0xFF;
static uint8_t ep_in_buffer_[ENDPOINT_SIZE];
static uint8_t ep_out_buffer_[ENDPOINT_SIZE];
static bool announced_ = false;

static void init(void) {
  endpoint_in_ = 0xFF;
  endpoint_out_ = 0xFF;
  announced_ = false;
  std::memset(ep_out_buffer_, 0, ENDPOINT_SIZE);
  std::memset(ep_in_buffer_, 0, ENDPOINT_SIZE);
}

static bool deinit(void) {
  init();
  return true;
}

static void reset(uint8_t rhport) { init(); }

static uint16_t open(uint8_t rhport,
                     tusb_desc_interface_t const *itf_descriptor,
                     uint16_t max_length) {
  uint16_t driver_length =
      sizeof(tusb_desc_interface_t) +
      (itf_descriptor->bNumEndpoints * sizeof(tusb_desc_endpoint_t));

  // Safety check mostly
  TU_VERIFY(max_length >= driver_length, 0);

  uint8_t const *current_descriptor = tu_desc_next(itf_descriptor);
  uint8_t found_endpoints = 0;

  // Only open endpoints for the Controller Interface (0)
  if (itf_descriptor->bInterfaceNumber == 0) {
    while ((found_endpoints < itf_descriptor->bNumEndpoints) &&
           (driver_length <= max_length)) {
      tusb_desc_endpoint_t const *endpoint_descriptor =
          (tusb_desc_endpoint_t const *)current_descriptor;
      if (TUSB_DESC_ENDPOINT == tu_desc_type(endpoint_descriptor)) {
        TU_ASSERT(usbd_edpt_open(rhport, endpoint_descriptor));

        if (tu_edpt_dir(endpoint_descriptor->bEndpointAddress) == TUSB_DIR_IN) {
          endpoint_in_ = endpoint_descriptor->bEndpointAddress;
        } else {
          endpoint_out_ = endpoint_descriptor->bEndpointAddress;
        }
        ++found_endpoints;
      }
      current_descriptor = tu_desc_next(current_descriptor);
    }
  }
  return driver_length;
}

static bool control_xfer_cb(uint8_t rhport, uint8_t stage,
                            tusb_control_request_t const *request) {
  return true;
}

static bool xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                    uint32_t xferred_bytes) {
  if (ep_addr == endpoint_out_ && xferred_bytes > 0) {
    uint8_t cmd = ep_out_buffer_[0];
    uint8_t flags = ep_out_buffer_[1];
    uint8_t seq = ep_out_buffer_[2];

    // Handle different GIP commands from host
    switch (cmd) {
    case GIP_CMD_IDENTIFY: {
      // Host requests device descriptor - send IDENTIFY response
      if (!usbd_edpt_busy(rhport, endpoint_in_)) {
        usbd_edpt_claim(rhport, endpoint_in_);
        std::memcpy(ep_in_buffer_, IDENTIFY_RESPONSE,
                    sizeof(IDENTIFY_RESPONSE));
        ep_in_buffer_[2] = seq; // Update sequence
        usbd_edpt_xfer(rhport, endpoint_in_, ep_in_buffer_,
                       sizeof(IDENTIFY_RESPONSE));
        usbd_edpt_release(rhport, endpoint_in_);
      }
      break;
    }
    default:
      // For commands that need ACK, send ACK
      if (flags & GIP_OPT_ACK) {
        uint8_t ack[5] = {GIP_CMD_ACK, GIP_OPT_INTERNAL, seq, 0x01, cmd};
        if (!usbd_edpt_busy(rhport, endpoint_in_)) {
          usbd_edpt_claim(rhport, endpoint_in_);
          std::memcpy(ep_in_buffer_, ack, sizeof(ack));
          usbd_edpt_xfer(rhport, endpoint_in_, ep_in_buffer_, sizeof(ack));
          usbd_edpt_release(rhport, endpoint_in_);
        }
      }
      break;
    }

    // Prepare to receive next packet
    usbd_edpt_xfer(rhport, endpoint_out_, ep_out_buffer_, ENDPOINT_SIZE);
  }
  return true;
}

static const usbd_class_driver_t driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "XBOXONE",
#else
    .name = NULL,
#endif
    .init = init,
    .deinit = deinit,
    .reset = reset,
    .open = open,
    .control_xfer_cb = control_xfer_cb,
    .xfer_cb = xfer_cb,
    .sof = NULL};

// Helper functions
bool send_report(const uint8_t *report, uint16_t len) {
  if (tud_ready() && (endpoint_in_ != 0xFF) &&
      (!usbd_edpt_busy(BOARD_TUD_RHPORT, endpoint_in_))) {
    usbd_edpt_claim(BOARD_TUD_RHPORT, endpoint_in_);
    usbd_edpt_xfer(BOARD_TUD_RHPORT, endpoint_in_, (uint8_t *)report, len);
    usbd_edpt_release(BOARD_TUD_RHPORT, endpoint_in_);
    return true;
  }
  return false;
}

void send_announce() {
  if (!announced_ && tud_ready() && (endpoint_in_ != 0xFF)) {
    send_report(ANNOUNCE_PACKET, sizeof(ANNOUNCE_PACKET));
    announced_ = true;
  }
}

void receive_report() {
  if (tud_ready() && (endpoint_out_ != 0xFF) &&
      (!usbd_edpt_busy(BOARD_TUD_RHPORT, endpoint_out_))) {
    usbd_edpt_claim(BOARD_TUD_RHPORT, endpoint_out_);
    usbd_edpt_xfer(BOARD_TUD_RHPORT, endpoint_out_, ep_out_buffer_,
                   ENDPOINT_SIZE);
    usbd_edpt_release(BOARD_TUD_RHPORT, endpoint_out_);
  }
}

} // namespace tud_xboxone

// Device Driver Implementation

void XboxOneDevice::initialize() {
  class_driver_ = tud_xboxone::driver;

  // Clear reports
  std::memset(&in_report_, 0, sizeof(XboxOne::InReport));

  // Initialize standard GIP header for Input packets
  in_report_.header.command = 0x20;
  in_report_.header.client = 0;
  in_report_.header.needsAck = 0;
  in_report_.header.internal = 0;
  in_report_.header.chunkStart = 0;
  in_report_.header.chunked = 0;
  in_report_.header.sequence = 0;
  in_report_.header.length = 14;

  // Prime the OUT endpoint to receive data (Rumble, Auth, etc.)
  // Note: This relies on tusb calling reset/open where we set up the next
  // transfer, but we can't call it here easily as endpoints aren't open yet.
}

void XboxOneDevice::process(const uint8_t idx, Gamepad &gamepad) {
  // Send announce packet on first call (after USB is ready)
  tud_xboxone::send_announce();

  // Always keep receiving on OUT endpoint
  tud_xboxone::receive_report();

  if (gamepad.new_pad_in()) {
    Gamepad::PadIn gp_in = gamepad.get_pad_in();

    // Increment Sequence
    in_report_.header.sequence++;
    sequence_ = in_report_.header.sequence;

    // Clear buttons
    in_report_.buttons[0] = 0;
    in_report_.buttons[1] = 0;

    // Map D-Pad
    if (gp_in.dpad & Gamepad::DPAD_UP)
      in_report_.buttons[1] |= XboxOne::Buttons1::DPAD_UP;
    if (gp_in.dpad & Gamepad::DPAD_DOWN)
      in_report_.buttons[1] |= XboxOne::Buttons1::DPAD_DOWN;
    if (gp_in.dpad & Gamepad::DPAD_LEFT)
      in_report_.buttons[1] |= XboxOne::Buttons1::DPAD_LEFT;
    if (gp_in.dpad & Gamepad::DPAD_RIGHT)
      in_report_.buttons[1] |= XboxOne::Buttons1::DPAD_RIGHT;

    // Map Buttons 0 (Main buttons)
    if (gp_in.buttons & Gamepad::BUTTON_START)
      in_report_.buttons[0] |= XboxOne::Buttons0::START;
    if (gp_in.buttons & Gamepad::BUTTON_BACK)
      in_report_.buttons[0] |= XboxOne::Buttons0::BACK;
    if (gp_in.buttons & Gamepad::BUTTON_A)
      in_report_.buttons[0] |= XboxOne::Buttons0::A;
    if (gp_in.buttons & Gamepad::BUTTON_B)
      in_report_.buttons[0] |= XboxOne::Buttons0::B;
    if (gp_in.buttons & Gamepad::BUTTON_X)
      in_report_.buttons[0] |= XboxOne::Buttons0::X;
    if (gp_in.buttons & Gamepad::BUTTON_Y)
      in_report_.buttons[0] |= XboxOne::Buttons0::Y;
    if (gp_in.buttons & Gamepad::BUTTON_SYS)
      in_report_.buttons[0] |= XboxOne::Buttons0::GUIDE;
    if (gp_in.buttons & Gamepad::BUTTON_MISC)
      in_report_.buttons[0] |= XboxOne::Buttons0::SYNC;

    // Map Buttons 1 (Shoulders / Thumbsticks)
    if (gp_in.buttons & Gamepad::BUTTON_LB)
      in_report_.buttons[1] |= XboxOne::Buttons1::LB;
    if (gp_in.buttons & Gamepad::BUTTON_RB)
      in_report_.buttons[1] |= XboxOne::Buttons1::RB;
    if (gp_in.buttons & Gamepad::BUTTON_L3)
      in_report_.buttons[1] |= XboxOne::Buttons1::L3;
    if (gp_in.buttons & Gamepad::BUTTON_R3)
      in_report_.buttons[1] |= XboxOne::Buttons1::R3;

    // Axis
    in_report_.trigger_l = gp_in.trigger_l
                           << 2; // Scale 8-bit to 10-bit (approx)
    in_report_.trigger_r = gp_in.trigger_r << 2;

    in_report_.joystick_lx = gp_in.joystick_lx;
    in_report_.joystick_ly = Range::invert(gp_in.joystick_ly);
    in_report_.joystick_rx = gp_in.joystick_rx;
    in_report_.joystick_ry = Range::invert(gp_in.joystick_ry);

    // Send Report (18 bytes)
    // Header (4) + Buttons (2) + Triggers (4) + Joysticks (8) = 18
    tud_xboxone::send_report((uint8_t *)&in_report_, 18);
  }
}

uint16_t XboxOneDevice::get_report_cb(uint8_t itf, uint8_t report_id,
                                      hid_report_type_t report_type,
                                      uint8_t *buffer, uint16_t req_len) {
  return 0;
}

void XboxOneDevice::set_report_cb(uint8_t itf, uint8_t report_id,
                                  hid_report_type_t report_type,
                                  uint8_t const *buffer, uint16_t buffer_size) {
}

bool XboxOneDevice::vendor_control_xfer_cb(
    uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  return tud_xboxone::driver.control_xfer_cb(rhport, stage, request);
}

const uint16_t *XboxOneDevice::get_descriptor_string_cb(uint8_t index,
                                                        uint16_t langid) {
  const char *value =
      reinterpret_cast<const char *>(XboxOne::STRING_DESCRIPTORS[index]);
  return get_string_descriptor(value, index);
}

const uint8_t *XboxOneDevice::get_descriptor_device_cb() {
  return XboxOne::DEVICE_DESCRIPTOR;
}

const uint8_t *XboxOneDevice::get_hid_descriptor_report_cb(uint8_t itf) {
  return nullptr;
}

const uint8_t *XboxOneDevice::get_descriptor_configuration_cb(uint8_t index) {
  return XboxOne::CONFIGURATION_DESCRIPTOR;
}

const uint8_t *XboxOneDevice::get_descriptor_device_qualifier_cb() {
  return nullptr;
}
