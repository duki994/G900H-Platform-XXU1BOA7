// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_socket.h"

#include <string.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "chrome/browser/extensions/api/cast_channel/cast_auth_util.h"
#include "chrome/browser/extensions/api/cast_channel/cast_channel.pb.h"
#include "chrome/browser/extensions/api/cast_channel/cast_message_util.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_info.h"

// Assumes |url_| of type GURL is available in the current scope.
#define VLOG_WITH_URL(level) VLOG(level) << "[" + url_.spec() + "] "

namespace {

// Allowed schemes for Cast device URLs.
const char kCastInsecureScheme[] = "cast";
const char kCastSecureScheme[] = "casts";

// The default keepalive delay.  On Linux, keepalives probes will be sent after
// the socket is idle for this length of time, and the socket will be closed
// after 9 failed probes.  So the total idle time before close is 10 *
// kTcpKeepAliveDelaySecs.
const int kTcpKeepAliveDelaySecs = 10;

}  // namespace

namespace extensions {

static base::LazyInstance<
  ProfileKeyedAPIFactory<ApiResourceManager<api::cast_channel::CastSocket> > >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
ProfileKeyedAPIFactory<ApiResourceManager<api::cast_channel::CastSocket> >*
ApiResourceManager<api::cast_channel::CastSocket>::GetFactoryInstance() {
  return g_factory.Pointer();
}

namespace api {
namespace cast_channel {

const uint32 kMaxMessageSize = 65536;
// Don't use sizeof(MessageHeader) because of alignment; instead, sum the
// sizeof() for the fields.
const uint32 kMessageHeaderSize = sizeof(uint32);

CastSocket::CastSocket(const std::string& owner_extension_id,
                       const GURL& url,
                       CastSocket::Delegate* delegate,
                       net::NetLog* net_log) :
    ApiResource(owner_extension_id),
    channel_id_(0),
    url_(url),
    delegate_(delegate),
    auth_required_(false),
    current_message_size_(0),
    current_message_(new CastMessage()),
    net_log_(net_log),
    connect_state_(CONN_STATE_NONE),
    write_state_(WRITE_STATE_NONE),
    read_state_(READ_STATE_NONE),
    error_state_(CHANNEL_ERROR_NONE),
    ready_state_(READY_STATE_NONE) {
  DCHECK(net_log_);
  net_log_source_.type = net::NetLog::SOURCE_SOCKET;
  net_log_source_.id = net_log_->NextID();

  // Reuse these buffers for each message.
  header_read_buffer_ = new net::GrowableIOBuffer();
  header_read_buffer_->SetCapacity(kMessageHeaderSize);
  body_read_buffer_ = new net::GrowableIOBuffer();
  body_read_buffer_->SetCapacity(kMaxMessageSize);
  current_read_buffer_ = header_read_buffer_;
}

CastSocket::~CastSocket() { }

const GURL& CastSocket::url() const {
  return url_;
}

scoped_ptr<net::TCPClientSocket> CastSocket::CreateTcpSocket() {
  net::AddressList addresses(ip_endpoint_);
  return scoped_ptr<net::TCPClientSocket>(
      new net::TCPClientSocket(addresses, net_log_, net_log_source_));
  // Options cannot be set on the TCPClientSocket yet, because the
  // underlying platform socket will not be created until Bind()
  // or Connect() is called.
}

scoped_ptr<net::SSLClientSocket> CastSocket::CreateSslSocket(
    scoped_ptr<net::StreamSocket> socket) {
  net::SSLConfig ssl_config;
  // If a peer cert was extracted in a previous attempt to connect, then
  // whitelist that cert.
  if (!peer_cert_.empty()) {
    net::SSLConfig::CertAndStatus cert_and_status;
    cert_and_status.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
    cert_and_status.der_cert = peer_cert_;
    ssl_config.allowed_bad_certs.push_back(cert_and_status);
  }

  cert_verifier_.reset(net::CertVerifier::CreateDefault());
  transport_security_state_.reset(new net::TransportSecurityState);
  net::SSLClientSocketContext context;
  // CertVerifier and TransportSecurityState are owned by us, not the
  // context object.
  context.cert_verifier = cert_verifier_.get();
  context.transport_security_state = transport_security_state_.get();

  scoped_ptr<net::ClientSocketHandle> connection(new net::ClientSocketHandle);
  connection->SetSocket(socket.Pass());
  net::HostPortPair host_and_port = net::HostPortPair::FromIPEndPoint(
      ip_endpoint_);

  return net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
      connection.Pass(), host_and_port, ssl_config, context);
}

bool CastSocket::ExtractPeerCert(std::string* cert) {
  DCHECK(cert);
  DCHECK(peer_cert_.empty());
  net::SSLInfo ssl_info;
  if (!socket_->GetSSLInfo(&ssl_info) || !ssl_info.cert.get())
    return false;
  bool result = net::X509Certificate::GetDEREncoded(
     ssl_info.cert->os_cert_handle(), cert);
  if (result)
    VLOG_WITH_URL(1) << "Successfully extracted peer certificate: " << *cert;
  return result;
}

bool CastSocket::VerifyChallengeReply() {
  return AuthenticateChallengeReply(*challenge_reply_.get(), peer_cert_);
}

void CastSocket::Connect(const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  VLOG_WITH_URL(1) << "Connect readyState = " << ready_state_;
  if (ready_state_ != READY_STATE_NONE) {
    callback.Run(net::ERR_CONNECTION_FAILED);
    return;
  }
  if (!ParseChannelUrl(url_)) {
    callback.Run(net::ERR_CONNECTION_FAILED);
    return;
  }

  ready_state_ = READY_STATE_CONNECTING;
  connect_callback_ = callback;
  connect_state_ = CONN_STATE_TCP_CONNECT;
  DoConnectLoop(net::OK);
}

void CastSocket::PostTaskToStartConnectLoop(int result) {
  DCHECK(CalledOnValidThread());
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CastSocket::DoConnectLoop, AsWeakPtr(), result));
}

// This method performs the state machine transitions for connection flow.
// There are two entry points to this method:
// 1. Connect method: this starts the flow
// 2. Callback from network operations that finish asynchronously
void CastSocket::DoConnectLoop(int result) {
  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // correct state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    ConnectionState state = connect_state_;
    // Default to CONN_STATE_NONE, which breaks the processing loop if any
    // handler fails to transition to another state to continue processing.
    connect_state_ = CONN_STATE_NONE;
    switch (state) {
      case CONN_STATE_TCP_CONNECT:
        rv = DoTcpConnect();
        break;
      case CONN_STATE_TCP_CONNECT_COMPLETE:
        rv = DoTcpConnectComplete(rv);
        break;
      case CONN_STATE_SSL_CONNECT:
        DCHECK_EQ(net::OK, rv);
        rv = DoSslConnect();
        break;
      case CONN_STATE_SSL_CONNECT_COMPLETE:
        rv = DoSslConnectComplete(rv);
        break;
      case CONN_STATE_AUTH_CHALLENGE_SEND:
        rv = DoAuthChallengeSend();
        break;
      case CONN_STATE_AUTH_CHALLENGE_SEND_COMPLETE:
        rv = DoAuthChallengeSendComplete(rv);
        break;
      case CONN_STATE_AUTH_CHALLENGE_REPLY_COMPLETE:
        rv = DoAuthChallengeReplyComplete(rv);
        break;
      default:
        NOTREACHED() << "BUG in connect flow. Unknown state: " << state;
        break;
    }
  } while (rv != net::ERR_IO_PENDING && connect_state_ != CONN_STATE_NONE);
  // Get out of the loop either when:
  // a. A network operation is pending, OR
  // b. The Do* method called did not change state

  // Connect loop is finished: if there is no pending IO invoke the callback.
  if (rv != net::ERR_IO_PENDING)
    DoConnectCallback(rv);
}

int CastSocket::DoTcpConnect() {
  VLOG_WITH_URL(1) << "DoTcpConnect";
  connect_state_ = CONN_STATE_TCP_CONNECT_COMPLETE;
  tcp_socket_ = CreateTcpSocket();
  return tcp_socket_->Connect(
      base::Bind(&CastSocket::DoConnectLoop, AsWeakPtr()));
}

int CastSocket::DoTcpConnectComplete(int result) {
  VLOG_WITH_URL(1) << "DoTcpConnectComplete: " << result;
  if (result == net::OK) {
    // Enable TCP protocol-level keep-alive.
    bool result = tcp_socket_->SetKeepAlive(true, kTcpKeepAliveDelaySecs);
    LOG_IF(WARNING, !result) << "Failed to SetKeepAlive.";
    connect_state_ = CONN_STATE_SSL_CONNECT;
  }
  return result;
}

int CastSocket::DoSslConnect() {
  VLOG_WITH_URL(1) << "DoSslConnect";
  connect_state_ = CONN_STATE_SSL_CONNECT_COMPLETE;
  socket_ = CreateSslSocket(tcp_socket_.PassAs<net::StreamSocket>());
  return socket_->Connect(
      base::Bind(&CastSocket::DoConnectLoop, AsWeakPtr()));
}

int CastSocket::DoSslConnectComplete(int result) {
  VLOG_WITH_URL(1) << "DoSslConnectComplete: " << result;
  if (result == net::ERR_CERT_AUTHORITY_INVALID &&
             peer_cert_.empty() &&
             ExtractPeerCert(&peer_cert_)) {
    connect_state_ = CONN_STATE_TCP_CONNECT;
  } else if (result == net::OK && auth_required_) {
    connect_state_ = CONN_STATE_AUTH_CHALLENGE_SEND;
  }
  return result;
}

int CastSocket::DoAuthChallengeSend() {
  VLOG_WITH_URL(1) << "DoAuthChallengeSend";
  connect_state_ = CONN_STATE_AUTH_CHALLENGE_SEND_COMPLETE;
  CastMessage challenge_message;
  CreateAuthChallengeMessage(&challenge_message);
  VLOG_WITH_URL(1) << "Sending challenge: "
                   << CastMessageToString(challenge_message);
  // Post a task to send auth challenge so that DoWriteLoop is not nested inside
  // DoConnectLoop. This is not strictly necessary but keeps the write loop
  // code decoupled from connect loop code.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CastSocket::SendCastMessageInternal, AsWeakPtr(),
                 challenge_message,
                 base::Bind(&CastSocket::DoConnectLoop, AsWeakPtr())));
  // Always return IO_PENDING since the result is always asynchronous.
  return net::ERR_IO_PENDING;
}

int CastSocket::DoAuthChallengeSendComplete(int result) {
  VLOG_WITH_URL(1) << "DoAuthChallengeSendComplete: " << result;
  if (result < 0)
    return result;
  connect_state_ = CONN_STATE_AUTH_CHALLENGE_REPLY_COMPLETE;
  // Post a task to start read loop so that DoReadLoop is not nested inside
  // DoConnectLoop. This is not strictly necessary but keeps the read loop
  // code decoupled from connect loop code.
  PostTaskToStartReadLoop();
  // Always return IO_PENDING since the result is always asynchronous.
  return net::ERR_IO_PENDING;
}

int CastSocket::DoAuthChallengeReplyComplete(int result) {
  VLOG_WITH_URL(1) << "DoAuthChallengeReplyComplete: " << result;
  if (result < 0)
    return result;
  if (!VerifyChallengeReply())
    return net::ERR_FAILED;
  VLOG_WITH_URL(1) << "Auth challenge verification succeeded";
  return net::OK;
}

void CastSocket::DoConnectCallback(int result) {
  ready_state_ = (result == net::OK) ? READY_STATE_OPEN : READY_STATE_CLOSED;
  error_state_ = (result == net::OK) ?
      CHANNEL_ERROR_NONE : CHANNEL_ERROR_CONNECT_ERROR;
  if (result == net::OK)  // Start the read loop
    PostTaskToStartReadLoop();
  base::ResetAndReturn(&connect_callback_).Run(result);
}

void CastSocket::Close(const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  VLOG_WITH_URL(1) << "Close ReadyState = " << ready_state_;
  tcp_socket_.reset();
  socket_.reset();
  cert_verifier_.reset();
  transport_security_state_.reset();
  ready_state_ = READY_STATE_CLOSED;
  callback.Run(net::OK);
  // |callback| can delete |this|
}

void CastSocket::SendMessage(const MessageInfo& message,
                             const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (ready_state_ != READY_STATE_OPEN) {
    callback.Run(net::ERR_FAILED);
    return;
  }
  CastMessage message_proto;
  if (!MessageInfoToCastMessage(message, &message_proto)) {
    callback.Run(net::ERR_FAILED);
    return;
  }

  SendCastMessageInternal(message_proto, callback);
}

void CastSocket::SendCastMessageInternal(
    const CastMessage& message,
    const net::CompletionCallback& callback) {
  WriteRequest write_request(callback);
  if (!write_request.SetContent(message)) {
    callback.Run(net::ERR_FAILED);
    return;
  }

  write_queue_.push(write_request);
  if (write_state_ == WRITE_STATE_NONE) {
    write_state_ = WRITE_STATE_WRITE;
    DoWriteLoop(net::OK);
  }
}

void CastSocket::DoWriteLoop(int result) {
  DCHECK(CalledOnValidThread());
  VLOG_WITH_URL(1) << "DoWriteLoop queue size: " << write_queue_.size();

  if (write_queue_.empty()) {
    write_state_ = WRITE_STATE_NONE;
    return;
  }

  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // write state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    WriteState state = write_state_;
    write_state_ = WRITE_STATE_NONE;
    switch (state) {
      case WRITE_STATE_WRITE:
        rv = DoWrite();
        break;
      case WRITE_STATE_WRITE_COMPLETE:
        rv = DoWriteComplete(rv);
        break;
      case WRITE_STATE_DO_CALLBACK:
        rv = DoWriteCallback();
        break;
      case WRITE_STATE_ERROR:
        rv = DoWriteError(rv);
        break;
      default:
        NOTREACHED() << "BUG in write flow. Unknown state: " << state;
        break;
    }
  } while (!write_queue_.empty() &&
           rv != net::ERR_IO_PENDING &&
           write_state_ != WRITE_STATE_NONE);

  // If write loop is done because the queue is empty then set write
  // state to NONE
  if (write_queue_.empty())
    write_state_ = WRITE_STATE_NONE;

  // Write loop is done - if the result is ERR_FAILED then close with error.
  if (rv == net::ERR_FAILED)
    CloseWithError(error_state_);
}

int CastSocket::DoWrite() {
  DCHECK(!write_queue_.empty());
  WriteRequest& request = write_queue_.front();

  VLOG_WITH_URL(2) << "WriteData byte_count = " << request.io_buffer->size()
                   << " bytes_written " << request.io_buffer->BytesConsumed();

  write_state_ = WRITE_STATE_WRITE_COMPLETE;

  return socket_->Write(
      request.io_buffer.get(),
      request.io_buffer->BytesRemaining(),
      base::Bind(&CastSocket::DoWriteLoop, AsWeakPtr()));
}

int CastSocket::DoWriteComplete(int result) {
  DCHECK(!write_queue_.empty());
  if (result <= 0) {  // NOTE that 0 also indicates an error
    error_state_ = CHANNEL_ERROR_SOCKET_ERROR;
    write_state_ = WRITE_STATE_ERROR;
    return result == 0 ? net::ERR_FAILED : result;
  }

  // Some bytes were successfully written
  WriteRequest& request = write_queue_.front();
  scoped_refptr<net::DrainableIOBuffer> io_buffer = request.io_buffer;
  io_buffer->DidConsume(result);
  if (io_buffer->BytesRemaining() == 0)  // Message fully sent
    write_state_ = WRITE_STATE_DO_CALLBACK;
  else
    write_state_ = WRITE_STATE_WRITE;

  return net::OK;
}

int CastSocket::DoWriteCallback() {
  DCHECK(!write_queue_.empty());
  WriteRequest& request = write_queue_.front();
  int bytes_consumed = request.io_buffer->BytesConsumed();

  // If inside connection flow, then there should be exaclty one item in
  // the write queue.
  if (ready_state_ == READY_STATE_CONNECTING) {
    write_queue_.pop();
    DCHECK(write_queue_.empty());
    PostTaskToStartConnectLoop(bytes_consumed);
  } else {
    WriteRequest& request = write_queue_.front();
    request.callback.Run(bytes_consumed);
    write_queue_.pop();
  }
  write_state_ = WRITE_STATE_WRITE;
  return net::OK;
}

int CastSocket::DoWriteError(int result) {
  DCHECK(!write_queue_.empty());
  DCHECK_LT(result, 0);

  // If inside connection flow, then there should be exactly one item in
  // the write queue.
  if (ready_state_ == READY_STATE_CONNECTING) {
    write_queue_.pop();
    DCHECK(write_queue_.empty());
    PostTaskToStartConnectLoop(result);
    // Connect loop will handle the error. Return net::OK so that write flow
    // does not try to report error also.
    return net::OK;
  }

  while (!write_queue_.empty()) {
    WriteRequest& request = write_queue_.front();
    request.callback.Run(result);
    write_queue_.pop();
  }
  return net::ERR_FAILED;
}

void CastSocket::PostTaskToStartReadLoop() {
  DCHECK(CalledOnValidThread());
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CastSocket::StartReadLoop, AsWeakPtr()));
}

void CastSocket::StartReadLoop() {
  // Read loop would have already been started if read state is not NONE
  if (read_state_ == READ_STATE_NONE) {
    read_state_ = READ_STATE_READ;
    DoReadLoop(net::OK);
  }
}

void CastSocket::DoReadLoop(int result) {
  DCHECK(CalledOnValidThread());
  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // write state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    ReadState state = read_state_;
    read_state_ = READ_STATE_NONE;

    switch (state) {
      case READ_STATE_READ:
        rv = DoRead();
        break;
      case READ_STATE_READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case READ_STATE_DO_CALLBACK:
        rv = DoReadCallback();
        break;
      case READ_STATE_ERROR:
        rv = DoReadError(rv);
        break;
      default:
        NOTREACHED() << "BUG in read flow. Unknown state: " << state;
        break;
    }
  } while (rv != net::ERR_IO_PENDING && read_state_ != READ_STATE_NONE);

  // Read loop is done - If the result is ERR_FAILED then close with error.
  if (rv == net::ERR_FAILED)
    CloseWithError(error_state_);
}

int CastSocket::DoRead() {
  read_state_ = READ_STATE_READ_COMPLETE;
  // Figure out whether to read header or body, and the remaining bytes.
  uint32 num_bytes_to_read = 0;
  if (header_read_buffer_->RemainingCapacity() > 0) {
    current_read_buffer_ = header_read_buffer_;
    num_bytes_to_read = header_read_buffer_->RemainingCapacity();
    DCHECK_LE(num_bytes_to_read, kMessageHeaderSize);
  } else {
    DCHECK_GT(current_message_size_, 0U);
    num_bytes_to_read = current_message_size_ - body_read_buffer_->offset();
    current_read_buffer_ = body_read_buffer_;
    DCHECK_LE(num_bytes_to_read, kMaxMessageSize);
  }
  DCHECK_GT(num_bytes_to_read, 0U);

  // Read up to num_bytes_to_read into |current_read_buffer_|.
  return socket_->Read(
      current_read_buffer_.get(),
      num_bytes_to_read,
      base::Bind(&CastSocket::DoReadLoop, AsWeakPtr()));
}

int CastSocket::DoReadComplete(int result) {
  VLOG_WITH_URL(2) << "DoReadComplete result = " << result
                   << " header offset = " << header_read_buffer_->offset()
                   << " body offset = " << body_read_buffer_->offset();
  if (result <= 0) {  // 0 means EOF: the peer closed the socket
    VLOG_WITH_URL(1) << "Read error, peer closed the socket";
    error_state_ = CHANNEL_ERROR_SOCKET_ERROR;
    read_state_ = READ_STATE_ERROR;
    return result == 0 ? net::ERR_FAILED : result;
  }

  // Some data was read.  Move the offset in the current buffer forward.
  DCHECK_LE(current_read_buffer_->offset() + result,
            current_read_buffer_->capacity());
  current_read_buffer_->set_offset(current_read_buffer_->offset() + result);
  read_state_ = READ_STATE_READ;

  if (current_read_buffer_.get() == header_read_buffer_.get() &&
      current_read_buffer_->RemainingCapacity() == 0) {
    // A full header is read, process the contents.
    if (!ProcessHeader()) {
      error_state_ = cast_channel::CHANNEL_ERROR_INVALID_MESSAGE;
      read_state_ = READ_STATE_ERROR;
    }
  } else if (current_read_buffer_.get() == body_read_buffer_.get() &&
             static_cast<uint32>(current_read_buffer_->offset()) ==
             current_message_size_) {
    // Full body is read, process the contents.
    if (ProcessBody()) {
      read_state_ = READ_STATE_DO_CALLBACK;
    } else {
      error_state_ = cast_channel::CHANNEL_ERROR_INVALID_MESSAGE;
      read_state_ = READ_STATE_ERROR;
    }
  }

  return net::OK;
}

int CastSocket::DoReadCallback() {
  read_state_ = READ_STATE_READ;
  const CastMessage& message = *(current_message_.get());
  if (IsAuthMessage(message)) {
    // An auth message is received, check that connect flow is running.
    if (ready_state_ == READY_STATE_CONNECTING) {
      challenge_reply_.reset(new CastMessage(message));
      PostTaskToStartConnectLoop(net::OK);
    } else {
      read_state_ = READ_STATE_ERROR;
    }
  } else if (delegate_) {
    MessageInfo message_info;
    if (CastMessageToMessageInfo(message, &message_info))
      delegate_->OnMessage(this, message_info);
    else
      read_state_ = READ_STATE_ERROR;
  }
  current_message_->Clear();
  return net::OK;
}

int CastSocket::DoReadError(int result) {
  DCHECK_LE(result, 0);
  // If inside connection flow, then get back to connect loop.
  if (ready_state_ == READY_STATE_CONNECTING) {
    PostTaskToStartConnectLoop(result);
    // does not try to report error also.
    return net::OK;
  }
  return net::ERR_FAILED;
}

bool CastSocket::ProcessHeader() {
  DCHECK_EQ(static_cast<uint32>(header_read_buffer_->offset()),
            kMessageHeaderSize);
  MessageHeader header;
  MessageHeader::ReadFromIOBuffer(header_read_buffer_.get(), &header);
  if (header.message_size > kMaxMessageSize)
    return false;

  VLOG_WITH_URL(2) << "Parsed header { message_size: "
                   << header.message_size << " }";
  current_message_size_ = header.message_size;
  return true;
}

bool CastSocket::ProcessBody() {
  DCHECK_EQ(static_cast<uint32>(body_read_buffer_->offset()),
            current_message_size_);
  if (!current_message_->ParseFromArray(
      body_read_buffer_->StartOfBuffer(), current_message_size_)) {
    return false;
  }
  current_message_size_ = 0;
  header_read_buffer_->set_offset(0);
  body_read_buffer_->set_offset(0);
  current_read_buffer_ = header_read_buffer_;
  return true;
}

// static
bool CastSocket::Serialize(const CastMessage& message_proto,
                           std::string* message_data) {
  DCHECK(message_data);
  message_proto.SerializeToString(message_data);
  size_t message_size = message_data->size();
  if (message_size > kMaxMessageSize) {
    message_data->clear();
    return false;
  }
  CastSocket::MessageHeader header;
  header.SetMessageSize(message_size);
  header.PrependToString(message_data);
  return true;
};

void CastSocket::CloseWithError(ChannelError error) {
  DCHECK(CalledOnValidThread());
  socket_.reset(NULL);
  ready_state_ = READY_STATE_CLOSED;
  error_state_ = error;
  if (delegate_)
    delegate_->OnError(this, error);
}

bool CastSocket::ParseChannelUrl(const GURL& url) {
  VLOG_WITH_URL(2) << "ParseChannelUrl";
  if (url.SchemeIs(kCastInsecureScheme)) {
    auth_required_ = false;
  } else if (url.SchemeIs(kCastSecureScheme)) {
    auth_required_ = true;
  } else {
    return false;
  }
  // TODO(mfoltz): Manual parsing, yech. Register cast[s] as standard schemes?
  // TODO(mfoltz): Test for IPv6 addresses.  Brackets or no brackets?
  // TODO(mfoltz): Maybe enforce restriction to IPv4 private and IPv6
  // link-local networks
  const std::string& path = url.path();
  // Shortest possible: //A:B
  if (path.size() < 5) {
    return false;
  }
  if (path.find("//") != 0) {
    return false;
  }
  size_t colon = path.find_last_of(':');
  if (colon == std::string::npos || colon < 3 || colon > path.size() - 2) {
    return false;
  }
  const std::string& ip_address_str = path.substr(2, colon - 2);
  const std::string& port_str = path.substr(colon + 1);
  VLOG_WITH_URL(2) << "IP: " << ip_address_str << " Port: " << port_str;
  int port;
  if (!base::StringToInt(port_str, &port))
    return false;
  net::IPAddressNumber ip_address;
  if (!net::ParseIPLiteralToNumber(ip_address_str, &ip_address))
    return false;
  ip_endpoint_ = net::IPEndPoint(ip_address, port);
  return true;
};

void CastSocket::FillChannelInfo(ChannelInfo* channel_info) const {
  channel_info->channel_id = channel_id_;
  channel_info->url = url_.spec();
  channel_info->ready_state = ready_state_;
  channel_info->error_state = error_state_;
}

bool CastSocket::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

CastSocket::MessageHeader::MessageHeader() : message_size(0) { }

void CastSocket::MessageHeader::SetMessageSize(size_t size) {
  DCHECK(size < static_cast<size_t>(kuint32max));
  DCHECK(size > 0);
  message_size = static_cast<size_t>(size);
}

void CastSocket::MessageHeader::PrependToString(std::string* str) {
  MessageHeader output = *this;
  output.message_size = base::HostToNet32(message_size);
  char char_array[kMessageHeaderSize];
  memcpy(&char_array, &output, arraysize(char_array));
  str->insert(0, char_array, arraysize(char_array));
}

void CastSocket::MessageHeader::ReadFromIOBuffer(
    net::GrowableIOBuffer* buffer, MessageHeader* header) {
  uint32 message_size;
  memcpy(&message_size, buffer->StartOfBuffer(), kMessageHeaderSize);
  header->message_size = base::NetToHost32(message_size);
}

std::string CastSocket::MessageHeader::ToString() {
  return "{message_size: " + base::UintToString(message_size) + "}";
}

CastSocket::WriteRequest::WriteRequest(const net::CompletionCallback& callback)
  : callback(callback) { }

bool CastSocket::WriteRequest::SetContent(const CastMessage& message_proto) {
  DCHECK(!io_buffer.get());
  std::string message_data;
  if (!Serialize(message_proto, &message_data))
    return false;
  io_buffer = new net::DrainableIOBuffer(new net::StringIOBuffer(message_data),
                                         message_data.size());
  return true;
}

CastSocket::WriteRequest::~WriteRequest() { }

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#undef VLOG_WITH_URL
