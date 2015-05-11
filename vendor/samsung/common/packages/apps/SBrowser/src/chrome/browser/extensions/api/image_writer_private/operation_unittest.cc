// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {
namespace image_writer {

namespace {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Gt;
using testing::Lt;

// This class gives us a generic Operation with the ability to set or inspect
// the current path to the image file.
class OperationForTest : public Operation {
 public:
  OperationForTest(base::WeakPtr<OperationManager> manager_,
                   const ExtensionId& extension_id,
                   const std::string& device_path)
      : Operation(manager_, extension_id, device_path) {}

  virtual void StartImpl() OVERRIDE {}

  // Expose internal stages for testing.
  void Unzip(const base::Closure& continuation) {
    Operation::Unzip(continuation);
  }

  void Write(const base::Closure& continuation) {
    Operation::Write(continuation);
  }

  void VerifyWrite(const base::Closure& continuation) {
    Operation::VerifyWrite(continuation);
  }

  // Helpers to set-up state for intermediate stages.
  void SetImagePath(const base::FilePath image_path) {
    image_path_ = image_path;
  }

  base::FilePath GetImagePath() { return image_path_; }

 private:
  virtual ~OperationForTest() {};
};

class ImageWriterOperationTest : public ImageWriterUnitTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    ImageWriterUnitTestBase::SetUp();

    // Create the zip file.
    base::FilePath image_dir = temp_dir_.path().AppendASCII("zip");
    ASSERT_TRUE(base::CreateDirectory(image_dir));
    ASSERT_TRUE(base::CreateTemporaryFileInDir(image_dir, &image_path_));

    FillFile(image_path_, kImagePattern, kTestFileSize);

    zip_file_ = temp_dir_.path().AppendASCII("test_image.zip");
    ASSERT_TRUE(zip::Zip(image_dir, zip_file_, true));

    // Operation setup.
    operation_ = new OperationForTest(manager_.AsWeakPtr(),
                                      kDummyExtensionId,
                                      test_device_path_.AsUTF8Unsafe());
    client_ = FakeImageWriterClient::Create();
    operation_->SetImagePath(test_image_path_);
  }

  virtual void TearDown() OVERRIDE {
    // Ensure all callbacks have been destroyed and cleanup occurs.
    client_->Shutdown();
    operation_->Cancel();

    ImageWriterUnitTestBase::TearDown();
  }

  base::FilePath image_path_;
  base::FilePath zip_file_;

  MockOperationManager manager_;
  scoped_refptr<FakeImageWriterClient> client_;
  scoped_refptr<OperationForTest> operation_;
};

} // namespace

// Unizpping a non-zip should do nothing.
TEST_F(ImageWriterOperationTest, UnzipNonZipFile) {
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId, _, _)).Times(0);

  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId, _, _)).Times(0);
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);

  operation_->Start();
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &OperationForTest::Unzip, operation_, base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterOperationTest, UnzipZipFile) {
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_UNZIP, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_UNZIP, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_UNZIP, 100))
      .Times(AtLeast(1));

  operation_->SetImagePath(zip_file_);

  operation_->Start();
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &OperationForTest::Unzip, operation_, base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::ContentsEqual(image_path_, operation_->GetImagePath()));
}

#if defined(OS_LINUX)
TEST_F(ImageWriterOperationTest, WriteImageToDevice) {
#if !defined(OS_CHROMEOS)
  operation_->SetUtilityClientForTesting(client_);
#endif

  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 100))
      .Times(AtLeast(1));

  operation_->Start();
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &OperationForTest::Write, operation_, base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();

#if !defined(OS_CHROMEOS)
  client_->Progress(0);
  client_->Progress(kTestFileSize / 2);
  client_->Progress(kTestFileSize);
  client_->Success();

  base::RunLoop().RunUntilIdle();
#endif
}
#endif

#if !defined(OS_CHROMEOS)
// Chrome OS doesn't support verification in the ImageBurner, so these two tests
// are skipped.

TEST_F(ImageWriterOperationTest, VerifyFileSuccess) {
  operation_->SetUtilityClientForTesting(client_);

  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, _))
      .Times(AtLeast(1));
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 100))
      .Times(AtLeast(1));

  FillFile(test_device_path_, kImagePattern, kTestFileSize);

  operation_->Start();
  content::BrowserThread::PostTask(content::BrowserThread::FILE,
                                   FROM_HERE,
                                   base::Bind(&OperationForTest::VerifyWrite,
                                              operation_,
                                              base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();

  client_->Progress(0);
  client_->Progress(kTestFileSize / 2);
  client_->Progress(kTestFileSize);
  client_->Success();

  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterOperationTest, VerifyFileFailure) {
  operation_->SetUtilityClientForTesting(client_);

  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, _))
      .Times(AnyNumber());
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 100))
      .Times(0);
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(
      manager_,
      OnError(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, _, _))
      .Times(1);

  FillFile(test_device_path_, kDevicePattern, kTestFileSize);

  operation_->Start();
  content::BrowserThread::PostTask(content::BrowserThread::FILE,
                                   FROM_HERE,
                                   base::Bind(&OperationForTest::VerifyWrite,
                                              operation_,
                                              base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();

  client_->Progress(0);
  client_->Progress(kTestFileSize / 2);
  client_->Error(error::kVerificationFailed);

  base::RunLoop().RunUntilIdle();
}
#endif

// Tests that on creation the operation_ has the expected state.
TEST_F(ImageWriterOperationTest, Creation) {
  EXPECT_EQ(0, operation_->GetProgress());
  EXPECT_EQ(image_writer_api::STAGE_UNKNOWN, operation_->GetStage());
}

}  // namespace image_writer
}  // namespace extensions
