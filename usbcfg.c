/*
    ChibiOS - Copyright (C) 2006..2016 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"

#include "hal_usb_msd.h"
#include "usbcfg.h"

/*
 * must be 64 for full speed and 512 for high speed
 */
#define USB_MSD_EP_SIZE                 64U

/*
 * USB Device Descriptor.
 */
static const uint8_t vcom_device_descriptor_data[18] = {
  USB_DESC_DEVICE       (0x0200,        /* bcdUSB (2.0).                    */
                         0x00,          /* bDeviceClass.                    */
                         0x00,          /* bDeviceSubClass.                 */
                         0x00,          /* bDeviceProtocol.                 */
                         0x40,          /* bMaxPacketSize.                  */
                         USB_VID,       /* idVendor.                        */
                         USB_PID,       /* idProduct.                       */
                         0x0100,        /* bcdDevice.                       */
                         1,             /* iManufacturer.                   */
                         2,             /* iProduct.                        */
                         3,             /* iSerialNumber.                   */
                         1)             /* bNumConfigurations.              */
};

/*
 * Device Descriptor wrapper.
 */
static const USBDescriptor vcom_device_descriptor = {
  sizeof vcom_device_descriptor_data,
  vcom_device_descriptor_data
};

/* Configuration Descriptor tree.*/
static const uint8_t vcom_configuration_descriptor_data[67] = {
    /* Configuration Descriptor.*/
    USB_DESC_CONFIGURATION(0x0020,        /* wTotalLength.                    */
                           0x01,          /* bNumInterfaces.                  */
                           0x01,          /* bConfigurationValue.             */
                           0,             /* iConfiguration.                  */
                           0x80,          /* bmAttributes (bus powered).      */
                           50),           /* bMaxPower in 2mA units (100mA).  */
    /* Interface Descriptor.*/
    USB_DESC_INTERFACE    (0x00,          /* bInterfaceNumber.                */
                           0x00,          /* bAlternateSetting.               */
                           0x02,          /* bNumEndpoints.                   */
                           0x08,          /* bInterfaceClass (Mass Storage)   */
                           0x06,          /* bInterfaceSubClass (SCSI
                                             Transparent storage class)       */
                           0x50,          /* bInterfaceProtocol (Bulk Only)   */
                           0),            /* iInterface. (none)               */
    /* Mass Storage Data In Endpoint Descriptor.*/
    USB_DESC_ENDPOINT     (USB_MSD_DATA_EP | 0x80,
                           0x02,          /* bmAttributes (Bulk).             */
                           USB_MSD_EP_SIZE,            /* wMaxPacketSize.                  */
                           0x00),         /* bInterval. 1ms                   */
    /* Mass Storage Data Out Endpoint Descriptor.*/
    USB_DESC_ENDPOINT     (USB_MSD_DATA_EP,
                           0x02,          /* bmAttributes (Bulk).             */
                           USB_MSD_EP_SIZE,            /* wMaxPacketSize.                  */
                           0x00)          /* bInterval. 1ms                   */
};

/*
 * Configuration Descriptor wrapper.
 */
static const USBDescriptor vcom_configuration_descriptor = {
  sizeof vcom_configuration_descriptor_data,
  vcom_configuration_descriptor_data
};

/*
 * U.S. English language identifier.
 */
static const uint8_t vcom_string0[] = {
  USB_DESC_BYTE(4),                     /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  USB_DESC_WORD(0x0409)                 /* wLANGID (U.S. English).          */
};

/*
 * Vendor string.
 */
static const uint8_t vcom_string1[] = {
  USB_DESC_BYTE(14),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'S', 0, 't', 0, 'r', 0, 'i', 0, 's', 0, 'o', 0,
};

/*
 * Device Description string.
 */
static const uint8_t vcom_string2[] = {
  USB_DESC_BYTE(46),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'U', 0, 'F', 0, '2', 0, '-', 0, 'C', 0, 'h', 0, 'i', 0, 'b', 0,
  'i', 0, 'O', 0, 'S', 0, ' ', 0, 'B', 0, 'o', 0, 'o', 0, 't', 0,
  'l', 0, 'o', 0, 'a', 0, 'd', 0, 'e', 0, 'r', 0
};

static uint8_t descriptor_serial_string[] = {
  USB_DESC_BYTE(50),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  '0', 0, '0', 0, '0', 0, '0', 0,
  '0', 0, '0', 0, '0', 0, '0', 0,
  '0', 0, '0', 0, '0', 0, '0', 0,
  '0', 0, '0', 0, '0', 0, '0', 0,
  '0', 0, '0', 0, '0', 0, '0', 0,
  '0', 0, '0', 0, '0', 0, '0', 0
};

static const USBDescriptor descriptor_serial = {
   sizeof descriptor_serial_string, descriptor_serial_string,
};

/*
 * Strings wrappers array.
 */
static const USBDescriptor vcom_strings[] = {
  {sizeof vcom_string0, vcom_string0},
  {sizeof vcom_string1, vcom_string1},
  {sizeof vcom_string2, vcom_string2},
  {sizeof descriptor_serial_string, descriptor_serial_string}
};

void inttohex(uint32_t v, unsigned char *p){
  int nibble;
  for (nibble = 0;nibble<8;nibble++){
    unsigned char c = (v>>(28-nibble*4))&0xF;
    if (c<10) c=c+'0';
    else c=c+'A'-10;
    *p = c;
    p += 2;
  }
}

/*
 * Handles the GET_DESCRIPTOR callback. All required descriptors must be
 * handled here.
 */
static const USBDescriptor *get_descriptor(USBDriver *usbp,
                                           uint8_t dtype,
                                           uint8_t dindex,
                                           uint16_t lang) {
  (void)usbp;
  (void)lang;
  switch (dtype) {
  case USB_DESCRIPTOR_DEVICE:
    return &vcom_device_descriptor;
  case USB_DESCRIPTOR_CONFIGURATION:
    return &vcom_configuration_descriptor;
  case USB_DESCRIPTOR_STRING:
    if (dindex == 3) {
      // use microcontroller unique ID as serial number (little-endian)
      inttohex(((uint32_t*)UID_BASE)[2],&descriptor_serial_string[2]);
      inttohex(((uint32_t*)UID_BASE)[1],&descriptor_serial_string[2+16]);
      inttohex(((uint32_t*)UID_BASE)[0],&descriptor_serial_string[2+32]);
      return &descriptor_serial;
    }
    if (dindex < 4)
      return &vcom_strings[dindex];
  }
  return NULL;
}

/**
 * @brief   IN EP1 state.
 */
static USBInEndpointState ep1instate;

/**
 * @brief   OUT EP1 state.
 */
static USBOutEndpointState ep1outstate;

/**
 * @brief   EP1 initialization structure (both IN and OUT).
 */
static const USBEndpointConfig ep1config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  NULL,
  NULL,
  USB_MSD_EP_SIZE,
  USB_MSD_EP_SIZE,
  &ep1instate,
  &ep1outstate,
  4,
  NULL
};

/*
 * Handles the USB driver global events.
 */
static void usb_event(USBDriver *usbp, usbevent_t event) {

  switch (event) {
  case USB_EVENT_RESET:
    return;
  case USB_EVENT_ADDRESS:
    return;
  case USB_EVENT_CONFIGURED:
    chSysLockFromISR();
    if (usbp->state == USB_ACTIVE) {
      /* Enables the endpoints specified into the configuration.
         Note, this callback is invoked from an ISR so I-Class functions
         must be used.*/
      usbInitEndpointI(usbp, USBD1_DATA_REQUEST_EP, &ep1config);
    } else if (usbp->state == USB_SELECTED) {
      usbDisableEndpointsI(usbp);
    }
    chSysUnlockFromISR();
    return;
  case USB_EVENT_UNCONFIGURED:
    return;
  case USB_EVENT_SUSPEND:
    return;
  case USB_EVENT_WAKEUP:
    return;
  case USB_EVENT_STALLED:
    return;
  }
  return;
}

/*
 * USB driver configuration.
 */
const USBConfig usbcfg = {
  usb_event,
  get_descriptor,
  msd_request_hook,
  NULL
};

