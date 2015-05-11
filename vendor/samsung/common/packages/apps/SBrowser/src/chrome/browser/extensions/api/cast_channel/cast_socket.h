// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_SOCKET_H_

#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/common/extensions/api/cast_channel.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_log.h"
#include "url/gurl.h"

namespace net {
class AddressList;
class CertVerifier;
class SSLClientSocket;
class StreamSocket;
class TCPClientSocket;
class TransportSecurityState;
}

namespace extensions {
namespace api {
namespace cast_channel {

class CastMessage;

// Size (in bytes) of the largest allowed message payload on the wire (without
// the header).
extern const uint32 kMaxMessageSize;

// Size (in bytes) of the message header.
extern const uint32 kMessageHeaderSize;

// This class implements a channel between Chrome and a Cast device using a TCP
// socket. The channel may be unauthenticated (cast://) or authenticated
// (casts://). All CastSocket objects must be used only on the IO thread.
//
// NOTE: Not called "CastChannel" to reduce confusion with the generated API
// code.
class CastSocket : public ApiResource,
                   public base::SupportsWeakPtr<CastSocket> {
 public:
  // Object to be informed of incoming messages and errors.
  class Delegate {
   public:
    // An error occurred on the channel.
    // It is fine to delete the socket in this callback.
    virtual void OnError(const CastSocket* socket,
                         ChannelError error) = 0;
    // A message was received on the channel.
    // Do NOT delete the socket in this callback.
    virtual void OnMessage(const CastSocket* socket,
                           const MessageInfo& message) = 0;
   protected:
    virtual ~Delegate() {}
  };

  // Creates a new CastSocket to |url|. |owner_extension_id| is the id of the
  // extension that opened the socket.
  CastSocket(const std::string& owner_extension_id,
             const GURL& url,
             CastSocket::Delegate* delegate,
             net::NetLog* net_log);
  virtual ~CastSocket();

  // The URL for the channel.
  const GURL& url() const;

  // Whether to perform receiver authentication.
  bool auth_required() const { return auth_required_; }

  // Channel id for the ApiResourceManager.
  int id() const { return channel_id_; }

  // Sets the channel id.
  void set_id(int channel_id) { channel_id_ = channel_id; }

  // Returns the state of the channel.
  ReadyState ready_state() const { return ready_state_; }

  // Returns the last error that occurred on this channel, or
  // CHANNEL_ERROR_NONE if no error has occurred.
  ChannelError error_state() const { return error_state_; }

  // Connects the channel to the peer. If successful, the channel will be in
  // READY_STATE_OPEN.
  // It is fine to delete the CastSocket object in |callback|.
  virtual void Connect(const net::CompletionCallback& callback);

  // Sends a message over a connected channel. The channel must be in
  // READY_STATE_OPEN.
  //
  // Note that if an error occurs the following happens:
  // 1. Completion callbacks for all pending writes are invoked with error.
  // 2. Delegate::OnError is called once.
  // 3. Castsocket is closed.
  //
  // DO NOT delete the CastSocket object in write completion callback.
  // But it is fine to delete the socket in Delegate::OnError
  virtual void SendMessage(const MessageInfo& message,
                           const net::CompletionCallback& callback);

  // Closes the channel. On completion, the channel will be in
  // READY_STATE_CLOSED.
  // It is fine to delete the CastSocket object in |callback|.
  virtual void Close(const net::CompletionCallback& callback);

  // Fills |channel_info| with the status of this channel.
  virtual void FillChannelInfo(ChannelInfo* channel_info) const;

 private:
  friend class ApiResourceManager<CastSocket>;
  friend class CastSocketTest;

  static const char* service_name() {
    return "CastSocketManager";
  }

  // Internal connection states.
  enum ConnectionState {
    CONN_STATE_NONE,
    CONN_STATE_TCP_CONNECT,
    CONN_STATE_TCP_CONNECT_COMPLETE,
    CONN_STATE_SSL_CONNECT,
    CONN_STATE_SSL_CONNECT_COMPLETE,
    CONN_STATE_AUTH_CHALLENGE_SEND,
    CONN_STATE_AUTH_CHALLENGE_SEND_COMPLETE,
    CONN_STATE_AUTH_CHALLENGE_REPLY_COMPLETE,
  };

  // Internal write states.
  enum WriteState {
    WRITE_STATE_NONE,
    WRITE_STATE_WRITE,
    WRITE_STATE_WRITE_COMPLETE,
    WRITE_STATE_DO_CALLBACK,
    WRITE_STATE_ERROR,
  };

  // Internal read states.
  enum ReadState {
    READ_STATE_NONE,
    READ_STATE_READ,
    READ_STATE_READ_COMPLETE,
    READ_STATE_DO_CALLBACK,
    READ_STATE_ERROR,
  };

  // Creates an instance of TCPClientSocket.
  virtual scoped_ptr<net::TCPClientSocket> CreateTcpSocket();
  // Creates an instance of SSLClientSocket with the given underlying |socket|.
  virtual scoped_ptr<net::SSLClientSocket> CreateSslSocket(
      scoped_ptr<net::StreamSocket> socket);
  // Returns IPEndPoint for the URL to connect to.
  const net::IPEndPoint& ip_endpoint() const { return ip_endpoint_; }
  // Extracts peer certificate from SSLClientSocket instance when the socket
  // is in cert error state.
  // Returns whether certificate is successfully extracted.
  virtual bool ExtractPeerCert(std::string* cert);
  // Verifies whether the challenge reply received from the peer is valid:
  // 1. Signature in the reply is valid.
  // 2. Certificate is rooted to a trusted CA.
  virtual bool VerifyChallengeReply();

  /////////////////////////////////////////////////////////////////////////////
  // Following methods work together to implement the following flow:
  // 1. Create a new TCP socket and connect to it
  // 2. Create a new SSL socket and try connecting to it
  // 3. If connection fails due to invalid cert authority, then extract the
  //    peer certificate from the error.
  // 4. Whitelist the peer certificate and try #1 and #2 again.
  // 5. If SSL socket is connected successfully, and if protocol is casts://
  //    then issue an auth challenge request.
  // 6. Validate the auth challenge response.
  //
  // Main method that performs connection state transitions.
  void DoConnectLoop(int result);
  // Each of the below Do* method is executed in the corresponding
  // connection state. For example when connection state is TCP_CONNECT
  // DoTcpConnect is called, and so on.
  int DoTcpConnect();
  int DoTcpConnectComplete(int result);
  int DoSslConnect();
  int DoSslConnectComplete(int result);
  int DoAuthChallengeSend();
  int DoAuthChallengeSendComplete(int result);
  int DoAuthChallengeReplyComplete(int result);
  /////////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////////
  // Following methods work together to implement write flow.
  //
  // Main method that performs write flow state transitions.
  void DoWriteLoop(int result);
  // Each of the below Do* method is executed in the corresponding
  // write state. For example when write state is WRITE_STATE_WRITE_COMPLETE
  // DowriteComplete is called, and so on.
  int DoWrite();
  int DoWriteComplete(int result);
  int DoWriteCallback();
  int DoWriteError(int result);
  /////////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////////
  // Following methods work together to implement read flow.
  //
  // Main method that performs write flow state transitions.
  void DoReadLoop(int result);
  // Each of the below Do* method is executed in the corresponding
  // write state. For example when write state is READ_STATE_READ_COMPLETE
  // DoReadComplete is called, and so on.
  int DoRead();
  int DoReadComplete(int result);
  int DoReadCallback();
  int DoReadError(int result);
  /////////////////////////////////////////////////////////////////////////////

  // Runs the external connection callback and resets it.
  void DoConnectCallback(int result);
  // Verifies that the URL is a valid cast:// or casts:// URL and sets url_ to
  // the result.
  bool ParseChannelUrl(const GURL& url);
  // Adds |message| to the write queue and starts the write loop if needed.
  void SendCastMessageInternal(const CastMessage& message,
                               const net::CompletionCallback& callback);
  void PostTaskToStartConnectLoop(int result);
  void PostTaskToStartReadLoop();
  void StartReadLoop();
  // Parses the contents of header_read_buffer_ and sets current_message_size_
  // to the size of the body of the message.
  bool ProcessHeader();
  // Parses the contents of body_read_buffer_ and sets current_message_ to
  // the message received.
  bool ProcessBody();
  // Closes socket, updating the error state and signaling the delegate that
  // |error| has occurred.
  void CloseWithError(ChannelError error);
  // Serializes the content of message_proto (with a header) to |message_data|.
  static bool Serialize(const CastMessage& message_proto,
                        std::string* message_data);

  virtual bool CalledOnValidThread() const;

  base::ThreadChecker thread_checker_;

  // The id of the channel.
  int channel_id_;

  // The URL of the peer (cast:// or casts://).
  GURL url_;
  // Delegate to inform of incoming messages and errors.
  Delegate* delegate_;
  // True if receiver authentication should be performed.
  bool auth_required_;
  // The IP endpoint of the peer.
  net::IPEndPoint ip_endpoint_;

  // IOBuffer for reading the message header.
  scoped_refptr<net::GrowableIOBuffer> header_read_buffer_;
  // IOBuffer for reading the message body.
  scoped_refptr<net::GrowableIOBuffer> body_read_buffer_;
  // IOBuffer to currently read into.
  scoped_refptr<net::GrowableIOBuffer> current_read_buffer_;
  // The number of bytes in the current message body.
  uint32 current_message_size_;
  // Last message received on the socket.
  scoped_ptr<CastMessage> current_message_;

  // The NetLog for this service.
  net::NetLog* net_log_;
  // The NetLog source for this service.
  net::NetLog::Source net_log_source_;

  // CertVerifier is owned by us but should be deleted AFTER SSLClientSocket
  // since in some cases the destructor of SSLClientSocket may call a method
  // to cancel a cert verification request.
  scoped_ptr<net::CertVerifier> cert_verifier_;
  scoped_ptr<net::TransportSecurityState> transport_security_state_;

  // Owned ptr to the underlying TCP socket.
  scoped_ptr<net::TCPClientSocket> tcp_socket_;
  // Owned ptr to the underlying SSL socket.
  scoped_ptr<net::SSLClientSocket> socket_;
  // Certificate of the peer. This field may be empty if the peer
  // certificate is not yet fetched.
  std::string peer_cert_;
  // Reply received from the receiver to a challenge request.
  scoped_ptr<CastMessage> challenge_reply_;

  // Callback invoked when the socket is connected.
  net::CompletionCallback connect_callback_;

  // Connection flow state machine state.
  ConnectionState connect_state_;
  // Write flow state machine state.
  WriteState write_state_;
  // Read flow state machine state.
  ReadState read_state_;
  // The last error encountered by the channel.
  ChannelError error_state_;
  // The current status of the channel.
  ReadyState ready_state_;

  // Message header struct. If fields are added, be sure to update
  // kMessageHeaderSize in the .cc.
  struct MessageHeader {
    MessageHeader();
    // Sets the message size.
    void SetMessageSize(size_t message_size);
    // Prepends this header to |str|.
    void PrependToString(std::string* str);
    // Reads |header| from the beginning of |buffer|.
    static void ReadFromIOBuffer(net::GrowableIOBuffer* buffer,
                                 MessageHeader* header);
    std::string ToString();
    // The size of the following protocol message in bytes, in host byte order.
    uint32 message_size;
  };

  // Holds a message to be written to the socket. |callback| is invoked when the
  // message is fully written or an error occurrs.
  struct WriteRequest {
    explicit WriteRequest(const net::CompletionCallback& callback);
    ~WriteRequest();
    // Sets the content of the request by serializing |message| into |io_buffer|
    // and prepending the header.  Must only be called once.
    bool SetContent(const CastMessage& message_proto);

    net::CompletionCallback callback;
    scoped_refptr<net::DrainableIOBuffer> io_buffer;
  };
  // Queue of pending writes. The message at the front of the queue is the one
  // being written.
  std::queue<WriteRequest> write_queue_;

  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestCastURLs);
  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestRead);
  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestReadMany);
  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestFullSecureConnectionFlowAsync);
  DISALLOW_COPY_AND_ASSIGN(CastSocket);
};

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_SOCKET_H_
