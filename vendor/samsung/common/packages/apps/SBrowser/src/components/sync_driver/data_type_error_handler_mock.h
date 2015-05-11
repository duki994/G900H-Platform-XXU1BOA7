// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_ERROR_HANDLER_MOCK_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_ERROR_HANDLER_MOCK_H__

#include "components/sync_driver/data_type_controller.h"
#include "sync/internal_api/public/base/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class DataTypeErrorHandlerMock : public DataTypeErrorHandler {
 public:
  DataTypeErrorHandlerMock();
  virtual ~DataTypeErrorHandlerMock();
  MOCK_METHOD2(OnSingleDatatypeUnrecoverableError,
               void(const tracked_objects::Location&, const std::string&));
  MOCK_METHOD3(CreateAndUploadError,
                   syncer::SyncError(const tracked_objects::Location&,
                             const std::string&,
                             syncer::ModelType));

};

}  // namesspace browser_sync

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_ERROR_HANDLER_MOCK_H__
