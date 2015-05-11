// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_win.h"

#include <cstdlib>
#include <string>

#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_connection_win.h"
#include "device/hid/hid_service.h"
#include "net/base/io_buffer.h"

#if defined(OS_WIN)

#define INITGUID

#include <windows.h>
#include <hidclass.h>

extern "C" {

#include <hidsdi.h>
#include <hidpi.h>

}

#include <setupapi.h>
#include <winioctl.h>
#include "base/win/scoped_handle.h"

#endif  // defined(OS_WIN)

// Setup API is required to enumerate HID devices.
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace device {
namespace {

const char kHIDClass[] = "HIDClass";

}  // namespace

HidServiceWin::HidServiceWin() {
  initialized_ = Enumerate();
}
HidServiceWin::~HidServiceWin() {}

bool HidServiceWin::Enumerate() {
  BOOL res;
  HDEVINFO device_info_set;
  SP_DEVINFO_DATA devinfo_data;
  SP_DEVICE_INTERFACE_DATA device_interface_data;

  memset(&devinfo_data, 0, sizeof(SP_DEVINFO_DATA));
  devinfo_data.cbSize = sizeof(SP_DEVINFO_DATA);
  device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  device_info_set = SetupDiGetClassDevs(
      &GUID_DEVINTERFACE_HID,
      NULL,
      NULL,
      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

  if (device_info_set == INVALID_HANDLE_VALUE)
    return false;

  for (int device_index = 0;
      SetupDiEnumDeviceInterfaces(device_info_set,
                                  NULL,
                                  &GUID_DEVINTERFACE_HID,
                                  device_index,
                                  &device_interface_data);
      device_index++) {
    DWORD required_size = 0;

    // Determime the required size of detail struct.
    SetupDiGetDeviceInterfaceDetailA(device_info_set,
                                     &device_interface_data,
                                     NULL,
                                     0,
                                     &required_size,
                                     NULL);

    scoped_ptr_malloc<SP_DEVICE_INTERFACE_DETAIL_DATA_A>
        device_interface_detail_data(
            reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(
                malloc(required_size)));
    device_interface_detail_data->cbSize =
        sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

    // Get the detailed data for this device.
    res = SetupDiGetDeviceInterfaceDetailA(device_info_set,
                                           &device_interface_data,
                                           device_interface_detail_data.get(),
                                           required_size,
                                           NULL,
                                           NULL);
    if (!res)
      continue;

    // Enumerate device info. Looking for Setup Class "HIDClass".
    for (DWORD i = 0;
        SetupDiEnumDeviceInfo(device_info_set, i, &devinfo_data);
        i++) {
      char class_name[256] = {0};
      res = SetupDiGetDeviceRegistryPropertyA(device_info_set,
                                              &devinfo_data,
                                              SPDRP_CLASS,
                                              NULL,
                                              (PBYTE) class_name,
                                              sizeof(class_name) - 1,
                                              NULL);
      if (!res)
        break;
      if (memcmp(class_name, kHIDClass, sizeof(kHIDClass)) == 0) {
        char driver_name[256] = {0};
        // Get bounded driver.
        res = SetupDiGetDeviceRegistryPropertyA(device_info_set,
                                                &devinfo_data,
                                                SPDRP_DRIVER,
                                                NULL,
                                                (PBYTE) driver_name,
                                                sizeof(driver_name) - 1,
                                                NULL);
        if (res) {
          // Found the drive.
          break;
        }
      }
    }

    if (!res)
      continue;

    PlatformAddDevice(device_interface_detail_data->DevicePath);
  }

  return true;
}

void HidServiceWin::PlatformAddDevice(std::string device_path) {
  HidDeviceInfo device_info;
  device_info.device_id = device_path;

  // Try to open the device.
  base::win::ScopedHandle device_handle(
      CreateFileA(device_path.c_str(),
                  0,
                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                  NULL,
                  OPEN_EXISTING,
                  FILE_FLAG_OVERLAPPED,
                  0));
  if (!device_handle.IsValid())
    return;

  // Get VID/PID pair.
  HIDD_ATTRIBUTES attrib = {0};
  attrib.Size = sizeof(HIDD_ATTRIBUTES);
  if (!HidD_GetAttributes(device_handle.Get(), &attrib))
    return;

  device_info.vendor_id = attrib.VendorID;
  device_info.product_id = attrib.ProductID;

  for (ULONG i = 32;
      HidD_SetNumInputBuffers(device_handle.Get(), i);
      i <<= 1);

  // Get usage and usage page (optional).
  PHIDP_PREPARSED_DATA preparsed_data;
  if (HidD_GetPreparsedData(device_handle.Get(), &preparsed_data) &&
      preparsed_data) {
    HIDP_CAPS capabilities;
    if (HidP_GetCaps(preparsed_data, &capabilities) == HIDP_STATUS_SUCCESS) {
      device_info.usage = capabilities.Usage;
      device_info.usage_page = capabilities.UsagePage;
      device_info.input_report_size = capabilities.InputReportByteLength;
      device_info.output_report_size = capabilities.OutputReportByteLength;
      device_info.feature_report_size = capabilities.FeatureReportByteLength;
    }
    // Detect if the device supports report ids.
    if (capabilities.NumberInputValueCaps > 0) {
      scoped_ptr<HIDP_VALUE_CAPS[]> value_caps(
          new HIDP_VALUE_CAPS[capabilities.NumberInputValueCaps]);
      USHORT value_caps_length = capabilities.NumberInputValueCaps;
      if (HidP_GetValueCaps(HidP_Input, &value_caps[0], &value_caps_length,
                            preparsed_data) == HIDP_STATUS_SUCCESS) {
        device_info.has_report_id = (value_caps[0].ReportID != 0);
      }
    }
    HidD_FreePreparsedData(preparsed_data);
  }

  // Get the serial number
  wchar_t str_property[512] = { 0 };
  if (HidD_GetSerialNumberString(device_handle.Get(),
                                 str_property,
                                 sizeof(str_property))) {
    device_info.serial_number = base::SysWideToUTF8(str_property);
  }

  if (HidD_GetProductString(device_handle.Get(),
                            str_property,
                            sizeof(str_property))) {
    device_info.product_name = base::SysWideToUTF8(str_property);
  }

  HidService::AddDevice(device_info);
}

void HidServiceWin::PlatformRemoveDevice(std::string device_path) {
  HidService::RemoveDevice(device_path);
}

void HidServiceWin::GetDevices(std::vector<HidDeviceInfo>* devices) {
  Enumerate();
  HidService::GetDevices(devices);
}

scoped_refptr<HidConnection> HidServiceWin::Connect(std::string device_id) {
  if (!ContainsKey(devices_, device_id)) return NULL;
  scoped_refptr<HidConnectionWin> connection(
      new HidConnectionWin(devices_[device_id]));
  if (!connection->available()) {
    LOG_GETLASTERROR(ERROR) << "Failed to open device.";
    return NULL;
  }
  return connection;
}

}  // namespace device
