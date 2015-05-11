// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_socket.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "chrome/browser/extensions/api/cast_channel/cast_channel.pb.h"
#include "chrome/browser/extensions/api/cast_channel/cast_message_util.h"
#include "net/base/address_list.h"
#include "net/base/capturing_net_log.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/ssl_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::A;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

namespace {
const char* kTestData[4] = {
    "Hello, World!",
    "Goodbye, World!",
    "Hello, Sky!",
    "Goodbye, Volcano!",
};
}  // namespace

namespace extensions {
namespace api {
namespace cast_channel {

// Fills in |message| with a string message.
static void CreateStringMessage(const std::string& namespace_,
                                const std::string& source_id,
                                const std::string& destination_id,
                                const std::string& data,
                                MessageInfo* message) {
  message->namespace_ = namespace_;
  message->source_id = source_id;
  message->destination_id = destination_id;
  message->data.reset(new base::StringValue(data));
}

// Fills in |message| with a binary message.
static void CreateBinaryMessage(const std::string& namespace_,
                                const std::string& source_id,
                                const std::string& destination_id,
                                const std::string& data,
                                MessageInfo* message) {
  message->namespace_ = namespace_;
  message->source_id = source_id;
  message->destination_id = destination_id;
  message->data.reset(base::BinaryValue::CreateWithCopiedBuffer(
      data.c_str(), data.size()));
}

// Returns the size of the body (in bytes) of the given serialized message.
static size_t ComputeBodySize(const std::string& msg) {
  return msg.length() - kMessageHeaderSize;
}

class MockCastSocketDelegate : public CastSocket::Delegate {
 public:
  MOCK_METHOD2(OnError, void(const CastSocket* socket,
                             ChannelError error));
  MOCK_METHOD2(OnMessage, void(const CastSocket* socket,
                               const MessageInfo& message));
};

class MockTCPSocket : public net::TCPClientSocket {
 public:
  explicit MockTCPSocket(const net::MockConnect& connect_data) :
      TCPClientSocket(net::AddressList(), NULL, net::NetLog::Source()),
      connect_data_(connect_data) { }

  virtual int Connect(const net::CompletionCallback& callback) OVERRIDE {
    if (connect_data_.mode == net::ASYNC) {
      CHECK_NE(connect_data_.result, net::ERR_IO_PENDING);
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, connect_data_.result));
      return net::ERR_IO_PENDING;
    } else {
      return connect_data_.result;
    }
  }

  virtual bool SetKeepAlive(bool enable, int delay) OVERRIDE {
    // Always return true in tests
    return true;
  }

  virtual bool SetNoDelay(bool no_delay) OVERRIDE {
    // Always return true in tests
    return true;
  }

  MOCK_METHOD3(Read,
               int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD3(Write,
               int(net::IOBuffer*, int, const net::CompletionCallback&));

  virtual void Disconnect() OVERRIDE {
    // Do nothing in tests
  }

 private:
  net::MockConnect connect_data_;
};

class CompleteHandler {
 public:
  CompleteHandler() {}
  MOCK_METHOD1(OnCloseComplete, void(int result));
  MOCK_METHOD1(OnConnectComplete, void(int result));
  MOCK_METHOD1(OnWriteComplete, void(int result));
 private:
  DISALLOW_COPY_AND_ASSIGN(CompleteHandler);
};

class TestCastSocket : public CastSocket {
 public:
  static scoped_ptr<TestCastSocket> Create(
      MockCastSocketDelegate* delegate) {
    return scoped_ptr<TestCastSocket>(
        new TestCastSocket(delegate, "cast://192.0.0.1:8009"));
  }

  static scoped_ptr<TestCastSocket> CreateSecure(
      MockCastSocketDelegate* delegate) {
    return scoped_ptr<TestCastSocket>(
        new TestCastSocket(delegate, "casts://192.0.0.1:8009"));
  }

  explicit TestCastSocket(MockCastSocketDelegate* delegate,
                          const std::string& url) :
      CastSocket("abcdefg", GURL(url), delegate,
                 &capturing_net_log_),
      ip_(CreateIPEndPoint()),
      connect_index_(0),
      extract_cert_result_(true),
      verify_challenge_result_(true) {
  }

  static net::IPEndPoint CreateIPEndPoint() {
    net::IPAddressNumber number;
    number.push_back(192);
    number.push_back(0);
    number.push_back(0);
    number.push_back(1);
    return net::IPEndPoint(number, 8009);
  }

  virtual ~TestCastSocket() {
  }

  // Helpers to set mock results for various operations.
  void SetupTcp1Connect(net::IoMode mode, int result) {
    tcp_connect_data_[0].reset(new net::MockConnect(mode, result));
  }
  void SetupSsl1Connect(net::IoMode mode, int result) {
    ssl_connect_data_[0].reset(new net::MockConnect(mode, result));
  }
  void SetupTcp2Connect(net::IoMode mode, int result) {
    tcp_connect_data_[1].reset(new net::MockConnect(mode, result));
  }
  void SetupSsl2Connect(net::IoMode mode, int result) {
    ssl_connect_data_[1].reset(new net::MockConnect(mode, result));
  }
  void AddWriteResult(const net::MockWrite& write) {
    writes_.push_back(write);
  }
  void AddWriteResult(net::IoMode mode, int result) {
    AddWriteResult(net::MockWrite(mode, result));
  }
  void AddWriteResultForMessage(net::IoMode mode, const std::string& msg) {
    AddWriteResult(mode, msg.size());
  }
  void AddWriteResultForMessage(net::IoMode mode,
                                const std::string& msg,
                                size_t ch_size) {
    size_t msg_size = msg.size();
    for (size_t offset = 0; offset < msg_size; offset += ch_size) {
      if (offset + ch_size > msg_size)
        ch_size = msg_size - offset;
      AddWriteResult(mode, ch_size);
    }
  }

  void AddReadResult(const net::MockRead& read) {
    reads_.push_back(read);
  }
  void AddReadResult(net::IoMode mode, int result) {
    AddReadResult(net::MockRead(mode, result));
  }
  void AddReadResult(net::IoMode mode, const char* data, int data_len) {
    AddReadResult(net::MockRead(mode, data, data_len));
  }
  void AddReadResultForMessage(net::IoMode mode, const std::string& msg) {
    size_t body_size = ComputeBodySize(msg);
    const char* data = msg.c_str();
    AddReadResult(mode, data, kMessageHeaderSize);
    AddReadResult(mode, data + kMessageHeaderSize, body_size);
  }
  void AddReadResultForMessage(net::IoMode mode,
                               const std::string& msg,
                               size_t ch_size) {
    size_t msg_size = msg.size();
    const char* data = msg.c_str();
    for (size_t offset = 0; offset < msg_size; offset += ch_size) {
      if (offset + ch_size > msg_size)
        ch_size = msg_size - offset;
      AddReadResult(mode, data + offset, ch_size);
    }
  }

  void SetExtractCertResult(bool value) {
    extract_cert_result_ = value;
  }
  void SetVerifyChallengeResult(bool value) {
    verify_challenge_result_ = value;
  }

 private:
  virtual scoped_ptr<net::TCPClientSocket> CreateTcpSocket() OVERRIDE {
    net::MockConnect* connect_data = tcp_connect_data_[connect_index_].get();
    connect_data->peer_addr = ip_;
    return scoped_ptr<net::TCPClientSocket>(new MockTCPSocket(*connect_data));
  }

  virtual scoped_ptr<net::SSLClientSocket> CreateSslSocket(
      scoped_ptr<net::StreamSocket> socket) OVERRIDE {
    net::MockConnect* connect_data = ssl_connect_data_[connect_index_].get();
    connect_data->peer_addr = ip_;
    ++connect_index_;

    ssl_data_.reset(new net::StaticSocketDataProvider(
        reads_.data(), reads_.size(), writes_.data(), writes_.size()));
    ssl_data_->set_connect_data(*connect_data);
    // NOTE: net::MockTCPClientSocket inherits from net::SSLClientSocket !!
    return scoped_ptr<net::SSLClientSocket>(
        new net::MockTCPClientSocket(
            net::AddressList(), &capturing_net_log_, ssl_data_.get()));
  }

  virtual bool ExtractPeerCert(std::string* cert) OVERRIDE {
    if (extract_cert_result_)
      cert->assign("dummy_test_cert");
    return extract_cert_result_;
  }

  virtual bool VerifyChallengeReply() OVERRIDE {
    return verify_challenge_result_;
  }

  net::CapturingNetLog capturing_net_log_;
  net::IPEndPoint ip_;
  // Simulated connect data
  scoped_ptr<net::MockConnect> tcp_connect_data_[2];
  scoped_ptr<net::MockConnect> ssl_connect_data_[2];
  // Simulated read / write data
  std::vector<net::MockWrite> writes_;
  std::vector<net::MockRead> reads_;
  scoped_ptr<net::SocketDataProvider> ssl_data_;
  // Number of times Connect method is called
  size_t connect_index_;
  // Simulated result of peer cert extraction.
  bool extract_cert_result_;
  // Simulated result of verifying challenge reply.
  bool verify_challenge_result_;
};

class CastSocketTest : public testing::Test {
 public:
  CastSocketTest() {}
  virtual ~CastSocketTest() {}

  virtual void SetUp() OVERRIDE {
    // Create a few test messages
    for (size_t i = 0; i < arraysize(test_messages_); i++) {
      CreateStringMessage("urn:cast", "1", "2", kTestData[i],
                          &test_messages_[i]);
      ASSERT_TRUE(MessageInfoToCastMessage(
          test_messages_[i], &test_protos_[i]));
      ASSERT_TRUE(CastSocket::Serialize(test_protos_[i], &test_proto_strs_[i]));
    }

    // Create a test auth request.
    CastMessage request;
    CreateAuthChallengeMessage(&request);
    ASSERT_TRUE(CastSocket::Serialize(request, &auth_request_));

    // Create a test auth reply.
    MessageInfo reply;
    CreateBinaryMessage("urn:x-cast:com.google.cast.tp.deviceauth",
                        "sender-0",
                        "receiver-0",
                        "abcd",
                        &reply);
    CastMessage reply_msg;
    ASSERT_TRUE(MessageInfoToCastMessage(reply, &reply_msg));
    ASSERT_TRUE(CastSocket::Serialize(reply_msg, &auth_reply_));
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_CALL(handler_, OnCloseComplete(net::OK));
    socket_->Close(base::Bind(&CompleteHandler::OnCloseComplete,
                              base::Unretained(&handler_)));
  }

  void CreateCastSocket() {
    socket_ = TestCastSocket::Create(&mock_delegate_);
  }

  void CreateCastSocketSecure() {
    socket_ = TestCastSocket::CreateSecure(&mock_delegate_);
  }

  // Sets up CastSocket::Connect to succeed.
  // Connecting the socket also starts the read loop; so we add a mock
  // read result that returns IO_PENDING and callback is never fired.
  void ConnectHelper() {
    socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
    socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::OK);
    socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

    EXPECT_CALL(handler_, OnConnectComplete(net::OK));
    socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                                base::Unretained(&handler_)));
    RunPendingTasks();
  }

 protected:
  // Runs all pending tasks in the message loop.
  void RunPendingTasks() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  MockCastSocketDelegate mock_delegate_;
  scoped_ptr<TestCastSocket> socket_;
  CompleteHandler handler_;
  MessageInfo test_messages_[arraysize(kTestData)];
  CastMessage test_protos_[arraysize(kTestData)];
  std::string test_proto_strs_[arraysize(kTestData)];
  std::string auth_request_;
  std::string auth_reply_;
};

// Tests URL parsing and validation.
TEST_F(CastSocketTest, TestCastURLs) {
  CreateCastSocket();
  EXPECT_TRUE(socket_->ParseChannelUrl(GURL("cast://192.0.0.1:8009")));
  EXPECT_FALSE(socket_->auth_required());
  EXPECT_EQ(socket_->ip_endpoint_.ToString(), "192.0.0.1:8009");

  EXPECT_TRUE(socket_->ParseChannelUrl(GURL("casts://192.0.0.1:12345")));
  EXPECT_TRUE(socket_->auth_required());
  EXPECT_EQ(socket_->ip_endpoint_.ToString(), "192.0.0.1:12345");

  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("http://192.0.0.1:12345")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast:192.0.0.1:12345")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast:///192.0.0.1:12345")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://:12345")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://abcd:8009")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://192.0.0.1:abcd")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("foo")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast:")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast::")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://192.0.0.1")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://:")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://192.0.0.1:")));
}

// Tests connecting and closing the socket.
TEST_F(CastSocketTest, TestConnectAndClose) {
  CreateCastSocket();
  ConnectHelper();
  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());

  EXPECT_CALL(handler_, OnCloseComplete(net::OK));
  socket_->Close(base::Bind(&CompleteHandler::OnCloseComplete,
                            base::Unretained(&handler_)));
  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection succeeds (async)
TEST_F(CastSocketTest, TestConnect) {
  CreateCastSocket();
  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::OK);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection fails with cert error (async)
// - Cert is extracted successfully
// - Second TCP connection succeeds (async)
// - Second SSL connection succeeds (async)
TEST_F(CastSocketTest, TestConnectTwoStep) {
  CreateCastSocket();
  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);
  socket_->SetupTcp2Connect(net::ASYNC, net::OK);
  socket_->SetupSsl2Connect(net::ASYNC, net::OK);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection fails with cert error (async)
// - Cert is extracted successfully
// - Second TCP connection succeeds (async)
// - Second SSL connection fails (async)
// - The flow should NOT be tried again
TEST_F(CastSocketTest, TestConnectMaxTwoAttempts) {
  CreateCastSocket();
  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);
  socket_->SetupTcp2Connect(net::ASYNC, net::OK);
  socket_->SetupSsl2Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CERT_AUTHORITY_INVALID));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Tests that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection fails with cert error (async)
// - Cert is extracted successfully
// - Second TCP connection succeeds (async)
// - Second SSL connection succeeds (async)
// - Challenge request is sent (async)
// - Challenge response is received (async)
// - Credentials are verified successfuly
TEST_F(CastSocketTest, TestConnectFullSecureFlowAsync) {
  CreateCastSocketSecure();

  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);
  socket_->SetupTcp2Connect(net::ASYNC, net::OK);
  socket_->SetupSsl2Connect(net::ASYNC, net::OK);
  socket_->AddWriteResultForMessage(net::ASYNC, auth_request_);
  socket_->AddReadResultForMessage(net::ASYNC, auth_reply_);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Same as TestFullSecureConnectionFlowAsync, but operations are synchronous.
TEST_F(CastSocketTest, TestConnectFullSecureFlowSync) {
  CreateCastSocketSecure();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::ERR_CERT_AUTHORITY_INVALID);
  socket_->SetupTcp2Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl2Connect(net::SYNCHRONOUS, net::OK);
  socket_->AddWriteResultForMessage(net::SYNCHRONOUS, auth_request_);
  socket_->AddReadResultForMessage(net::SYNCHRONOUS, auth_reply_);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test connection error - TCP connect fails (async)
TEST_F(CastSocketTest, TestConnectTcpConnectErrorAsync) {
  CreateCastSocketSecure();

  socket_->SetupTcp1Connect(net::ASYNC, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_FAILED));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - TCP connect fails (sync)
TEST_F(CastSocketTest, TestConnectTcpConnectErrorSync) {
  CreateCastSocketSecure();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_FAILED));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - SSL connect fails (async)
TEST_F(CastSocketTest, TestConnectSslConnectErrorAsync) {
  CreateCastSocketSecure();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_FAILED));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - SSL connect fails (async)
TEST_F(CastSocketTest, TestConnectSslConnectErrorSync) {
  CreateCastSocketSecure();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_FAILED));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - cert extraction error (async)
TEST_F(CastSocketTest, TestConnectCertExtractionErrorAsync) {
  CreateCastSocket();
  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);
  // Set cert extraction to fail
  socket_->SetExtractCertResult(false);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CERT_AUTHORITY_INVALID));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - cert extraction error (sync)
TEST_F(CastSocketTest, TestConnectCertExtractionErrorSync) {
  CreateCastSocket();
  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::ERR_CERT_AUTHORITY_INVALID);
  // Set cert extraction to fail
  socket_->SetExtractCertResult(false);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CERT_AUTHORITY_INVALID));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - challenge send fails
TEST_F(CastSocketTest, TestConnectChallengeSendError) {
  CreateCastSocketSecure();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::OK);
  socket_->AddWriteResult(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_FAILED));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - challenge reply receive fails
TEST_F(CastSocketTest, TestConnectChallengeReplyReceiveError) {
  CreateCastSocketSecure();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::OK);
  socket_->AddWriteResultForMessage(net::ASYNC, auth_request_);
  socket_->AddReadResult(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_FAILED));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - challenge reply verification fails
TEST_F(CastSocketTest, TestConnectChallengeVerificationFails) {
  CreateCastSocketSecure();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::OK);
  socket_->AddWriteResultForMessage(net::ASYNC, auth_request_);
  socket_->AddReadResultForMessage(net::ASYNC, auth_reply_);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);
  socket_->SetVerifyChallengeResult(false);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_FAILED));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test write success - single message (async)
TEST_F(CastSocketTest, TestWriteAsync) {
  CreateCastSocket();
  socket_->AddWriteResultForMessage(net::ASYNC, test_proto_strs_[0]);
  ConnectHelper();

  EXPECT_CALL(handler_, OnWriteComplete(test_proto_strs_[0].size()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - single message (sync)
TEST_F(CastSocketTest, TestWriteSync) {
  CreateCastSocket();
  socket_->AddWriteResultForMessage(net::SYNCHRONOUS, test_proto_strs_[0]);
  ConnectHelper();

  EXPECT_CALL(handler_, OnWriteComplete(test_proto_strs_[0].size()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - single message sent in multiple chunks (async)
TEST_F(CastSocketTest, TestWriteChunkedAsync) {
  CreateCastSocket();
  socket_->AddWriteResultForMessage(net::ASYNC, test_proto_strs_[0], 2);
  ConnectHelper();

  EXPECT_CALL(handler_, OnWriteComplete(test_proto_strs_[0].size()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - single message sent in multiple chunks (sync)
TEST_F(CastSocketTest, TestWriteChunkedSync) {
  CreateCastSocket();
  socket_->AddWriteResultForMessage(net::SYNCHRONOUS, test_proto_strs_[0], 2);
  ConnectHelper();

  EXPECT_CALL(handler_, OnWriteComplete(test_proto_strs_[0].size()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - multiple messages (async)
TEST_F(CastSocketTest, TestWriteManyAsync) {
  CreateCastSocket();
  for (size_t i = 0; i < arraysize(test_messages_); i++) {
    size_t msg_size = test_proto_strs_[i].size();
    socket_->AddWriteResult(net::ASYNC, msg_size);
    EXPECT_CALL(handler_, OnWriteComplete(msg_size));
  }
  ConnectHelper();

  for (size_t i = 0; i < arraysize(test_messages_); i++) {
    socket_->SendMessage(test_messages_[i],
                         base::Bind(&CompleteHandler::OnWriteComplete,
                                    base::Unretained(&handler_)));
  }
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - multiple messages (sync)
TEST_F(CastSocketTest, TestWriteManySync) {
  CreateCastSocket();
  for (size_t i = 0; i < arraysize(test_messages_); i++) {
    size_t msg_size = test_proto_strs_[i].size();
    socket_->AddWriteResult(net::SYNCHRONOUS, msg_size);
    EXPECT_CALL(handler_, OnWriteComplete(msg_size));
  }
  ConnectHelper();

  for (size_t i = 0; i < arraysize(test_messages_); i++) {
    socket_->SendMessage(test_messages_[i],
                         base::Bind(&CompleteHandler::OnWriteComplete,
                                    base::Unretained(&handler_)));
  }
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write error - not connected
TEST_F(CastSocketTest, TestWriteErrorNotConnected) {
  CreateCastSocket();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));

  EXPECT_EQ(cast_channel::READY_STATE_NONE, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write error - very large message
TEST_F(CastSocketTest, TestWriteErrorLargeMessage) {
  CreateCastSocket();
  ConnectHelper();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  size_t size = kMaxMessageSize + 1;
  test_messages_[0].data.reset(
      new base::StringValue(std::string(size, 'a')));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());

}

// Test write error - network error (sync)
TEST_F(CastSocketTest, TestWriteNetworkErrorSync) {
  CreateCastSocket();
  socket_->AddWriteResult(net::SYNCHRONOUS, net::ERR_FAILED);
  ConnectHelper();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(), cast_channel::CHANNEL_ERROR_SOCKET_ERROR));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test write error - network error (async)
TEST_F(CastSocketTest, TestWriteErrorAsync) {
  CreateCastSocket();
  socket_->AddWriteResult(net::ASYNC, net::ERR_FAILED);
  ConnectHelper();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(), cast_channel::CHANNEL_ERROR_SOCKET_ERROR));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test write error - 0 bytes written should be considered an error
TEST_F(CastSocketTest, TestWriteErrorZeroBytesWritten) {
  CreateCastSocket();
  socket_->AddWriteResult(net::SYNCHRONOUS, 0);
  ConnectHelper();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(), cast_channel::CHANNEL_ERROR_SOCKET_ERROR));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test that when an error occurrs in one write, write callback is invoked for
// all pending writes with the error
TEST_F(CastSocketTest, TestWriteErrorWithMultiplePendingWritesAsync) {
  CreateCastSocket();
  socket_->AddWriteResult(net::ASYNC, net::ERR_SOCKET_NOT_CONNECTED);
  ConnectHelper();

  const int num_writes = arraysize(test_messages_);
  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_SOCKET_NOT_CONNECTED))
      .Times(num_writes);
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(), cast_channel::CHANNEL_ERROR_SOCKET_ERROR));
  for (int i = 0; i < num_writes; i++) {
    socket_->SendMessage(test_messages_[i],
                         base::Bind(&CompleteHandler::OnWriteComplete,
                                    base::Unretained(&handler_)));
  }
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test read success - single message (async)
TEST_F(CastSocketTest, TestReadAsync) {
  CreateCastSocket();
  socket_->AddReadResultForMessage(net::ASYNC, test_proto_strs_[0]);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - single message (sync)
TEST_F(CastSocketTest, TestReadSync) {
  CreateCastSocket();
  socket_->AddReadResultForMessage(net::SYNCHRONOUS, test_proto_strs_[0]);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - single message received in multiple chunks (async)
TEST_F(CastSocketTest, TestReadChunkedAsync) {
  CreateCastSocket();
  socket_->AddReadResultForMessage(net::ASYNC, test_proto_strs_[0], 2);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - single message received in multiple chunks (sync)
TEST_F(CastSocketTest, TestReadChunkedSync) {
  CreateCastSocket();
  socket_->AddReadResultForMessage(net::SYNCHRONOUS, test_proto_strs_[0], 2);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - multiple messages (async)
TEST_F(CastSocketTest, TestReadManyAsync) {
  CreateCastSocket();
  size_t num_reads = arraysize(test_proto_strs_);
  for (size_t i = 0; i < num_reads; i++)
    socket_->AddReadResultForMessage(net::ASYNC, test_proto_strs_[i]);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()))
      .Times(num_reads);
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - multiple messages (sync)
TEST_F(CastSocketTest, TestReadManySync) {
  CreateCastSocket();
  size_t num_reads = arraysize(test_proto_strs_);
  for (size_t i = 0; i < num_reads; i++)
    socket_->AddReadResultForMessage(net::SYNCHRONOUS, test_proto_strs_[i]);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()))
      .Times(num_reads);
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read error - network error (async)
TEST_F(CastSocketTest, TestReadErrorAsync) {
  CreateCastSocket();
  socket_->AddReadResult(net::ASYNC, net::ERR_SOCKET_NOT_CONNECTED);
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_SOCKET_ERROR));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test read error - network error (sync)
TEST_F(CastSocketTest, TestReadErrorSync) {
  CreateCastSocket();
  socket_->AddReadResult(net::SYNCHRONOUS, net::ERR_SOCKET_NOT_CONNECTED);
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_SOCKET_ERROR));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test read error - header parse error
TEST_F(CastSocketTest, TestReadHeaderParseError) {
  CreateCastSocket();
  uint32 body_size = base::HostToNet32(kMaxMessageSize + 1);
  // TODO(munjal): Add a method to cast_message_util.h to serialize messages
  char header[sizeof(body_size)];
  memcpy(&header, &body_size, arraysize(header));
  socket_->AddReadResult(net::SYNCHRONOUS, header, arraysize(header));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_INVALID_MESSAGE));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_INVALID_MESSAGE,
            socket_->error_state());
}

// Test read error - body parse error
TEST_F(CastSocketTest, TestReadBodyParseError) {
  CreateCastSocket();
  char body[] = "some body";
  uint32 body_size = base::HostToNet32(arraysize(body));
  char header[sizeof(body_size)];
  memcpy(&header, &body_size, arraysize(header));
  socket_->AddReadResult(net::SYNCHRONOUS, header, arraysize(header));
  socket_->AddReadResult(net::SYNCHRONOUS, body, arraysize(body));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_INVALID_MESSAGE));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_INVALID_MESSAGE,
            socket_->error_state());
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
