#ifndef _XBOX_ONE_DEVICE_H_
#define _XBOX_ONE_DEVICE_H_

#include "Descriptors/XboxOne.h"
#include "USBDevice/DeviceDriver/DeviceDriver.h"

// Xbox One (Brook Clone) Descriptors
// VID: 0x045E (Microsoft)
// PID: 0x02EA (Xbox One Controller)
// Manufacturer: "Brook"

namespace XboxOne {

static const uint8_t DEVICE_DESCRIPTOR[] = {
    18,               // bLength
    TUSB_DESC_DEVICE, // bDescriptorType
    0x00,
    0x02, // bcdUSB 2.00
    0xFF, // bDeviceClass
    0xFF, // bDeviceSubClass
    0xFF, // bDeviceProtocol
    64,   // bMaxPacketSize0
    0x5E,
    0x04, // idVendor (Microsoft)
    0xEA,
    0x02, // idProduct (Xbox One Controller)
    0x19,
    0x01, // bcdDevice (1.19 - Brook FW)
    0x01, // iManufacturer
    0x02, // iProduct
    0x03, // iSerialNumber
    0x01  // bNumConfigurations
};

static const uint8_t CONFIGURATION_DESCRIPTOR[] = {
    // Configuration Descriptor (9 bytes)
    9,                       // bLength
    TUSB_DESC_CONFIGURATION, // bDescriptorType
    0x60, 0x00,              // wTotalLength (96 bytes)
    0x03,                    // bNumInterfaces (3 Interfaces)
    0x01,                    // bConfigurationValue
    0x00,                    // iConfiguration
    0xA0,                    // bmAttributes (Bus Powered, Remote Wakeup)
    0xFA,                    // bMaxPower (500mA)

    // ============ Interface 0: Controller (23 bytes) ============
    9, TUSB_DESC_INTERFACE,
    0x00,                   // bInterfaceNumber
    0x00,                   // bAlternateSetting
    0x02,                   // bNumEndpoints
    0xFF, 0x47, 0xD0, 0x00, // Class/SubClass/Protocol/iInterface

    // Endpoint 2 OUT (Interrupt)
    7, TUSB_DESC_ENDPOINT, 0x02, 0x03, 0x40, 0x00, 0x04,
    // Endpoint 2 IN (Interrupt, 1ms)
    7, TUSB_DESC_ENDPOINT, 0x82, 0x03, 0x40, 0x00, 0x01,

    // ============ Interface 1: Audio Alt 0 (9 bytes) ============
    9, TUSB_DESC_INTERFACE, 0x01, 0x00,
    0x00, // InterfaceNumber=1, Alt=0, 0 Endpoints
    0xFF, 0x47, 0xD0, 0x00,

    // ============ Interface 1: Audio Alt 1 (23 bytes) ============
    9, TUSB_DESC_INTERFACE, 0x01, 0x01,
    0x02, // InterfaceNumber=1, Alt=1, 2 Endpoints
    0xFF, 0x47, 0xD0, 0x00,

    // Endpoint 5 OUT (Isochronous, 228 bytes, 1ms)
    7, TUSB_DESC_ENDPOINT, 0x05, 0x01, 0xE4, 0x00, 0x01,
    // Endpoint 3 IN (Isochronous, 228 bytes, 1ms)
    7, TUSB_DESC_ENDPOINT, 0x83, 0x01, 0xE4, 0x00, 0x01,

    // ============ Interface 2: Bulk Alt 0 (9 bytes) ============
    9, TUSB_DESC_INTERFACE, 0x02, 0x00,
    0x00, // InterfaceNumber=2, Alt=0, 0 Endpoints
    0xFF, 0x47, 0xD0, 0x00,

    // ============ Interface 2: Bulk Alt 1 (23 bytes) ============
    9, TUSB_DESC_INTERFACE, 0x02, 0x01,
    0x02, // InterfaceNumber=2, Alt=1, 2 Endpoints
    0xFF, 0x47, 0xD0, 0x00,

    // Endpoint 4 OUT (Bulk, 64 bytes)
    7, TUSB_DESC_ENDPOINT, 0x04, 0x02, 0x40, 0x00, 0x00,
    // Endpoint 4 IN (Bulk, 64 bytes)
    7, TUSB_DESC_ENDPOINT, 0x84, 0x02, 0x40, 0x00, 0x00};

static const uint16_t STRING_DESCRIPTOR_0[] = {
    0x0304, 0x0409 // English
};

static const uint16_t STRING_DESCRIPTOR_1[] = {
    // "Brook"
    0x030C, 'B', 'r', 'o', 'o', 'k'};

static const uint16_t STRING_DESCRIPTOR_2[] = {
    // "Controller"
    0x0316, 'C', 'o', 'n', 't', 'r', 'o', 'l', 'l', 'e', 'r'};

static const uint16_t STRING_DESCRIPTOR_3[] = {
    // Serial (Arbitrary from Brook dump)
    0x033A, '3', '1', '4', '2', '3', '0', '3', '0', '3',
    '7',    '3', '1', '3', '0', '3', '4', '3', '6', '3',
    '8',    '3', '8', '3', '7', '4', '5', '3', '4'};
// 3142303037313034363838374534

static const uint16_t *const STRING_DESCRIPTORS[] = {
    STRING_DESCRIPTOR_0,
    STRING_DESCRIPTOR_1,
    STRING_DESCRIPTOR_2,
    STRING_DESCRIPTOR_3,
};

} // namespace XboxOne

class XboxOneDevice : public DeviceDriver {
public:
  void initialize() override;
  void process(const uint8_t idx, Gamepad &gamepad) override;
  uint16_t get_report_cb(uint8_t itf, uint8_t report_id,
                         hid_report_type_t report_type, uint8_t *buffer,
                         uint16_t req_len) override;
  void set_report_cb(uint8_t itf, uint8_t report_id,
                     hid_report_type_t report_type, uint8_t const *buffer,
                     uint16_t buffer_size) override;
  bool vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                              tusb_control_request_t const *request) override;
  const uint16_t *get_descriptor_string_cb(uint8_t index,
                                           uint16_t langid) override;
  const uint8_t *get_descriptor_device_cb() override;
  const uint8_t *get_hid_descriptor_report_cb(uint8_t itf) override;
  const uint8_t *get_descriptor_configuration_cb(uint8_t index) override;
  const uint8_t *get_descriptor_device_qualifier_cb() override;

private:
  uint8_t sequence_ = 0;
  XboxOne::InReport in_report_;
};

#endif // _XBOX_ONE_DEVICE_H_
