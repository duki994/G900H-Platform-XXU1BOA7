// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"

namespace extensions {
namespace image_writer {

using testing::_;
using testing::Lt;
using testing::AnyNumber;
using testing::AtLeast;

class ImageWriterFromFileTest : public ImageWriterUnitTestBase {
};

TEST_F(ImageWriterFromFileTest, InvalidFile) {
  MockOperationManager manager;

  scoped_refptr<WriteFromFileOperation> op = new WriteFromFileOperation(
    manager.AsWeakPtr(),
    kDummyExtensionId,
    test_image_path_,
    test_device_path_.AsUTF8Unsafe());

  base::DeleteFile(test_image_path_, false);

  EXPECT_CALL(manager, OnProgress(kDummyExtensionId, _, _)).Times(0);
  EXPECT_CALL(manager, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager, OnError(kDummyExtensionId,
                               image_writer_api::STAGE_UNKNOWN,
                               0,
                               error::kImageInvalid)).Times(1);

  op->Start();

  base::RunLoop().RunUntilIdle();
}

// Runs the entire WriteFromFile operation.
TEST_F(ImageWriterFromFileTest, WriteFromFileEndToEnd) {
  MockOperationManager manager;

  scoped_refptr<WriteFromFileOperation> op =
      new WriteFromFileOperation(manager.AsWeakPtr(),
                                 kDummyExtensionId,
                                 test_image_path_,
                                 test_device_path_.AsUTF8Unsafe());
#if !defined(OS_CHROMEOS)
  scoped_refptr<FakeImageWriterClient> client = FakeImageWriterClient::Create();
  op->SetUtilityClientForTesting(client);
#endif

  EXPECT_CALL(manager,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 100))
      .Times(AtLeast(1));

#if !defined(OS_CHROMEOS)
  // Chrome OS doesn't verify.
  EXPECT_CALL(
      manager,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, _))
      .Times(AnyNumber());
  EXPECT_CALL(
      manager,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(
      manager,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 100))
      .Times(AtLeast(1));
#endif

  EXPECT_CALL(manager, OnComplete(kDummyExtensionId)).Times(1);
  EXPECT_CALL(manager, OnError(kDummyExtensionId, _, _, _)).Times(0);

  op->Start();

  base::RunLoop().RunUntilIdle();
#if !defined(OS_CHROMEOS)
  client->Progress(0);
  client->Progress(50);
  client->Progress(100);
  client->Success();
  base::RunLoop().RunUntilIdle();
  client->Progress(0);
  client->Progress(50);
  client->Progress(100);
  client->Success();
  base::RunLoop().RunUntilIdle();
#endif
}

}  // namespace image_writer
}  // namespace extensions
