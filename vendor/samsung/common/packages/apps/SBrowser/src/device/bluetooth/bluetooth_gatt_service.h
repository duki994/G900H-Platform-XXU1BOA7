// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_GATT_SERVICE_H_
#define DEVICE_BLUETOOTH_GATT_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_utils.h"

namespace device {

class BluetoothGattCharacteristic;
class BluetoothGattDescriptor;

// BluetoothGattService represents a local or remote GATT service. A GATT
// service is hosted by a peripheral and represents a collection of data in
// the form of GATT characteristics and a set of included GATT services if this
// service is what is called "a primary service".
//
// Instances of the BluetoothGattService class are used for two functions:
//   1. To represent GATT attribute hierarchies that have been received from a
//      remote Bluetooth GATT peripheral. Such BluetoothGattService instances
//      are constructed and owned by a BluetoothDevice.
//
//   2. To represent a locally hosted GATT attribute hierarchy when the local
//      adapter is used in the "peripheral" role. Such instances are meant to be
//      constructed directly and registered. Once registered, a GATT attribute
//      hierarchy will be visible to remote devices in the "central" role.
class BluetoothGattService {
 public:
  // The Delegate class is used to send certain events that need to be handled
  // when the device is in peripheral mode. The delegate handles read and write
  // requests that are issued from remote clients.
  class Delegate {
   public:
    // Callbacks used for communicating GATT request responses.
    typedef base::Callback<void(const std::vector<uint8>)> ValueCallback;
    typedef base::Closure ErrorCallback;

    // Called when a remote device in the central role requests to read the
    // value of the characteristic |characteristic| starting at offset |offset|.
    // This method is only called if the characteristic was specified as
    // readable and any authentication and authorization challanges were
    // satisfied by the remote device.
    //
    // To respond to the request with success and return the requested value,
    // the delegate must invoke |callback| with the value. Doing so will
    // automatically update the value property of |characteristic|. To respond
    // to the request with failure (e.g. if an invalid offset was given),
    // delegates must invoke |error_callback|. If neither callback parameter is
    // invoked, the request will time out and result in an error. Therefore,
    // delegates MUST invoke either |callback| or |error_callback|.
    virtual void OnCharacteristicReadRequest(
        const BluetoothGattService* service,
        const BluetoothGattCharacteristic* characteristic,
        int offset,
        const ValueCallback& callback,
        const ErrorCallback& error_callback) = 0;

    // Called when a remote device in the central role requests to write the
    // value of the characteristic |characteristic| starting at offset |offset|.
    // This method is only called if the characteristic was specified as
    // writeable and any authentication and authorization challanges were
    // satisfied by the remote device.
    //
    // To respond to the request with success the delegate must invoke
    // |callback| with the new value of the characteristic. Doing so will
    // automatically update the value property of |characteristic|. To respond
    // to the request with failure (e.g. if an invalid offset was given),
    // delegates must invoke |error_callback|. If neither callback parameter is
    // invoked, the request will time out and result in an error. Therefore,
    // delegates MUST invoke either |callback| or |error_callback|.
    virtual void OnCharacteristicWriteRequest(
        const BluetoothGattService* service,
        const BluetoothGattCharacteristic* characteristic,
        const std::vector<uint8>& value,
        int offset,
        const ValueCallback& callback,
        const ErrorCallback& error_callback) = 0;

    // Called when a remote device in the central role requests to read the
    // value of the descriptor |descriptor| starting at offset |offset|.
    // This method is only called if the characteristic was specified as
    // readable and any authentication and authorization challanges were
    // satisfied by the remote device.
    //
    // To respond to the request with success and return the requested value,
    // the delegate must invoke |callback| with the value. Doing so will
    // automatically update the value property of |descriptor|. To respond
    // to the request with failure (e.g. if an invalid offset was given),
    // delegates must invoke |error_callback|. If neither callback parameter is
    // invoked, the request will time out and result in an error. Therefore,
    // delegates MUST invoke either |callback| or |error_callback|.
    virtual void OnDescriptorReadRequest(
        const BluetoothGattService* service,
        const BluetoothGattDescriptor* descriptor,
        int offset,
        const ValueCallback& callback,
        const ErrorCallback& error_callback) = 0;

    // Called when a remote device in the central role requests to write the
    // value of the descriptor |descriptor| starting at offset |offset|.
    // This method is only called if the characteristic was specified as
    // writeable and any authentication and authorization challanges were
    // satisfied by the remote device.
    //
    // To respond to the request with success the delegate must invoke
    // |callback| with the new value of the descriptor. Doing so will
    // automatically update the value property of |descriptor|. To respond
    // to the request with failure (e.g. if an invalid offset was given),
    // delegates must invoke |error_callback|. If neither callback parameter is
    // invoked, the request will time out and result in an error. Therefore,
    // delegates MUST invoke either |callback| or |error_callback|.
    virtual void OnDescriptorWriteRequest(
        const BluetoothGattService* service,
        const BluetoothGattDescriptor* descriptor,
        const std::vector<uint8>& value,
        int offset,
        const ValueCallback& callback,
        const ErrorCallback& error_callback) = 0;
  };

  // Interface for observing changes from a BluetoothGattService. Properties
  // of remote services are received asynchronously. The Observer interface can
  // be used to be notified when the initial values of a service are received
  // as well as when successive changes occur during its life cycle.
  class Observer {
   public:
    // Called when the UUID of |service| have changed.
    virtual void UuidChanged(
        BluetoothGattService* service,
        const bluetooth_utils::UUID& uuid) {}

    // Called when the services included by |service| have changed.
    virtual void IncludedServicesChanged(
        BluetoothGattService* service,
        const std::vector<BluetoothGattService*>& included_services) {}

    // Called when the characteristics that belong to |service| have changed.
    virtual void CharacteristicsChanged(
        BluetoothGattService* service,
        const std::vector<BluetoothGattCharacteristic*>& characteristics) {}
  };

  // The ErrorCallback is used by methods to asynchronously report errors.
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  // Constructs a BluetoothGattService that can be locally hosted when the local
  // adapter is in the peripheral role. The resulting object can then be made
  // available by calling the "Register" method. This method constructs a
  // service with UUID |uuid|. Whether the constructed service is primary or
  // secondary is determined by |is_primary|. |delegate| is used to send certain
  // peripheral role events. If |delegate| is NULL, then this service will
  // employ a default behavior when responding to read and write requests based
  // on the cached value of its characteristics and descriptors at a given time.
  static BluetoothGattService* Create(const bluetooth_utils::UUID& uuid,
                                      bool is_primary,
                                      Delegate* delegate);

  // The Bluetooth-specific UUID of the service.
  virtual const bluetooth_utils::UUID& GetUuid() const = 0;

  // Returns true, if this service hosted locally. If false, then this service
  // represents a remote GATT service.
  virtual bool IsLocal() const = 0;

  // Indicates whether the type of this service is primary or secondary. A
  // primary service describes the primary function of the peripheral that
  // hosts it, while a secondary service only makes sense in the presence of a
  // primary service. A primary service may include other primary or secondary
  // services.
  virtual bool IsPrimary() const = 0;

  // List of characteristics that belong to this service.
  virtual const std::vector<BluetoothGattCharacteristic*>&
      GetCharacteristics() const = 0;

  // List of GATT services that are included by this service.
  virtual const std::vector<BluetoothGattService*>&
      GetIncludedServices() const = 0;

  // Adds and removes observers for events on this GATT service. If monitoring
  // multiple services, check the |service| parameter of observer methods to
  // determine which service is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Adds characteristics and included services to the local attribute hierarchy
  // represented by this service. These methods only make sense for local
  // services and won't have an effect if this instance represents a remote
  // GATT service and will return false. While ownership of added
  // characteristics are taken over by the service, ownership of an included
  // service is not taken.
  virtual bool AddCharacteristic(
      BluetoothGattCharacteristic* characteristic) = 0;
  virtual bool AddService(BluetoothGattService* service) = 0;

  // Registers this GATT service. Calling Register will make this service and
  // all of its associated attributes available on the local adapters GATT
  // database and the service UUID will be advertised to nearby devices if the
  // local adapter is discoverable. Call Unregister to make this service no
  // longer available.
  //
  // These methods only make sense for services that are local and will hence
  // fail if this instance represents a remote GATT service. |callback| is
  // called to denote success and |error_callback| to denote failure.
  virtual void Register(const base::Closure& callback,
                        const ErrorCallback& error_callback) = 0;
  virtual void Unregister(const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

 protected:
  BluetoothGattService();
  virtual ~BluetoothGattService();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothGattService);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_GATT_SERVICE_H_
