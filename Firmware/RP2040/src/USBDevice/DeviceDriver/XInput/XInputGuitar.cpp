#include <cstring>

#include "USBDevice/DeviceDriver/XInput/XInputGuitar.h"
#include "USBDevice/DeviceDriver/XInput/tud_xinput/tud_xinput.h"


void XInputGuitarDevice::initialize() {
  class_driver_ = *tud_xinput::class_driver();
}

void XInputGuitarDevice::process(const uint8_t idx, Gamepad &gamepad) {
  if (gamepad.new_pad_in()) {
    in_report_.buttons[0] = 0;
    in_report_.buttons[1] = 0;
    in_report_.trigger_l = 0;
    in_report_.trigger_r = 0;

    Gamepad::PadIn gp_in = gamepad.get_pad_in();

    // Strum mapping: D-Pad Up/Down -> Strum Up/Down
    if (gp_in.dpad & Gamepad::DPAD_UP)
      in_report_.buttons[0] |= XInputGuitar::Buttons0::DPAD_UP;
    if (gp_in.dpad & Gamepad::DPAD_DOWN)
      in_report_.buttons[0] |= XInputGuitar::Buttons0::DPAD_DOWN;
    if (gp_in.dpad & Gamepad::DPAD_LEFT)
      in_report_.buttons[0] |= XInputGuitar::Buttons0::DPAD_LEFT;
    if (gp_in.dpad & Gamepad::DPAD_RIGHT)
      in_report_.buttons[0] |= XInputGuitar::Buttons0::DPAD_RIGHT;

    // Control buttons
    if (gp_in.buttons & Gamepad::BUTTON_BACK)
      in_report_.buttons[0] |= XInputGuitar::Buttons0::BACK;
    if (gp_in.buttons & Gamepad::BUTTON_START)
      in_report_.buttons[0] |= XInputGuitar::Buttons0::START;
    if (gp_in.buttons & Gamepad::BUTTON_SYS)
      in_report_.buttons[1] |= XInputGuitar::Buttons1::HOME;

    // Guitar Hero 3 Controller Mode Fret Mapping:
    // Green  = LT
    // Red    = LB
    // Yellow = RB
    // Blue   = RT
    // Orange = A

    // PS3 Guitar buttons: Green=A, Red=B, Yellow=Y, Blue=X, Orange=LB
    if (gp_in.buttons & Gamepad::BUTTON_A)
      in_report_.trigger_l = 255; // Green -> LT
    if (gp_in.buttons & Gamepad::BUTTON_B)
      in_report_.buttons[1] |= XInputGuitar::Buttons1::LB; // Red -> LB
    if (gp_in.buttons & Gamepad::BUTTON_Y)
      in_report_.trigger_r = 255; // Yellow -> RT
    if (gp_in.buttons & Gamepad::BUTTON_X)
      in_report_.buttons[1] |= XInputGuitar::Buttons1::RB; // Blue -> RB
    if (gp_in.buttons & Gamepad::BUTTON_LB)
      in_report_.buttons[1] |= XInputGuitar::Buttons1::A; // Orange -> A

    // Whammy bar
    in_report_.joystick_lx = gp_in.joystick_lx;
    in_report_.joystick_ly = Range::invert(gp_in.joystick_ly);
    in_report_.joystick_rx = gp_in.joystick_rx;
    in_report_.joystick_ry = Range::invert(gp_in.joystick_ry);

    if (tud_suspended()) {
      tud_remote_wakeup();
    }

    tud_xinput::send_report((uint8_t *)&in_report_,
                            sizeof(XInputGuitar::InReport));
  }

  if (tud_xinput::receive_report(reinterpret_cast<uint8_t *>(&out_report_),
                                 sizeof(XInputGuitar::OutReport)) &&
      out_report_.report_id == 0x00) {
    Gamepad::PadOut gp_out;
    gp_out.rumble_l = out_report_.rumble_l;
    gp_out.rumble_r = out_report_.rumble_r;
    gamepad.set_pad_out(gp_out);
  }
}

uint16_t XInputGuitarDevice::get_report_cb(uint8_t itf, uint8_t report_id,
                                           hid_report_type_t report_type,
                                           uint8_t *buffer, uint16_t reqlen) {
  std::memcpy(buffer, &in_report_, sizeof(XInputGuitar::InReport));
  return sizeof(XInputGuitar::InReport);
}

void XInputGuitarDevice::set_report_cb(uint8_t itf, uint8_t report_id,
                                       hid_report_type_t report_type,
                                       uint8_t const *buffer,
                                       uint16_t bufsize) {}

bool XInputGuitarDevice::vendor_control_xfer_cb(
    uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  return false;
}

const uint16_t *XInputGuitarDevice::get_descriptor_string_cb(uint8_t index,
                                                             uint16_t langid) {
  const char *value =
      reinterpret_cast<const char *>(XInputGuitar::DESC_STRING[index]);
  return get_string_descriptor(value, index);
}

const uint8_t *XInputGuitarDevice::get_descriptor_device_cb() {
  return XInputGuitar::DESC_DEVICE;
}

const uint8_t *XInputGuitarDevice::get_hid_descriptor_report_cb(uint8_t itf) {
  return nullptr;
}

const uint8_t *
XInputGuitarDevice::get_descriptor_configuration_cb(uint8_t index) {
  return XInputGuitar::DESC_CONFIGURATION;
}

const uint8_t *XInputGuitarDevice::get_descriptor_device_qualifier_cb() {
  return nullptr;
}
