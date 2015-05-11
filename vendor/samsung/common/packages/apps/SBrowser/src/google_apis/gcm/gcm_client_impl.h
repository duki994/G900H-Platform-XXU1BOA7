// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_GCM_CLIENT_IMPL_H_
#define GOOGLE_APIS_GCM_GCM_CLIENT_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/registration_request.h"
#include "google_apis/gcm/gcm_client.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "net/base/net_log.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class Clock;
}  // namespace base

namespace net {
class HttpNetworkSession;
}  // namespace net

namespace gcm {

class CheckinRequest;
class ConnectionFactory;
class GCMClientImplTest;
class UnregistrationRequest;

// Implements the GCM Client. It is used to coordinate MCS Client (communication
// with MCS) and other pieces of GCM infrastructure like Registration and
// Checkins. It also allows for registering user delegates that host
// applications that send and receive messages.
class GCM_EXPORT GCMClientImpl : public GCMClient {
 public:
  GCMClientImpl();
  virtual ~GCMClientImpl();

  // Overridden from GCMClient:
  virtual void Initialize(
      const checkin_proto::ChromeBuildProto& chrome_build_proto,
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      Delegate* delegate) OVERRIDE;
  virtual void Load() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void CheckOut() OVERRIDE;
  virtual void Register(const std::string& app_id,
                        const std::string& cert,
                        const std::vector<std::string>& sender_ids) OVERRIDE;
  virtual void Unregister(const std::string& app_id) OVERRIDE;
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message) OVERRIDE;

 private:
  // State representation of the GCMClient.
  enum State {
    // Uninitialized.
    UNINITIALIZED,
    // Initialized,
    INITIALIZED,
    // GCM store loading is in progress.
    LOADING,
    // Initial device checkin is in progress.
    INITIAL_DEVICE_CHECKIN,
    // Ready to accept requests.
    READY,
  };

  // The check-in info for the user. Returned by the server.
  struct GCM_EXPORT CheckinInfo {
    CheckinInfo() : android_id(0), secret(0) {}
    bool IsValid() const { return android_id != 0 && secret != 0; }
    void Reset() {
      android_id = 0;
      secret = 0;
    }

    uint64 android_id;
    uint64 secret;
  };

  // Collection of pending registration requests. Keys are app IDs, while values
  // are pending registration requests to obtain a registration ID for
  // requesting application.
  typedef std::map<std::string, RegistrationRequest*>
      PendingRegistrations;

  // Collection of pending unregistration requests. Keys are app IDs, while
  // values are pending unregistration requests to disable the registration ID
  // currently assigned to the application.
  typedef std::map<std::string, UnregistrationRequest*>
      PendingUnregistrations;

  friend class GCMClientImplTest;

  // Callbacks for the MCSClient.
  // Receives messages and dispatches them to relevant user delegates.
  void OnMessageReceivedFromMCS(const gcm::MCSMessage& message);
  // Receives confirmation of sent messages or information about errors.
  void OnMessageSentToMCS(int64 user_serial_number,
                          const std::string& app_id,
                          const std::string& message_id,
                          MCSClient::MessageSendStatus status);
  // Receives information about mcs_client_ errors.
  void OnMCSError();

  // Runs after GCM Store load is done to trigger continuation of the
  // initialization.
  void OnLoadCompleted(scoped_ptr<GCMStore::LoadResult> result);
  // Initializes mcs_client_, which handles the connection to MCS.
  void InitializeMCSClient(scoped_ptr<GCMStore::LoadResult> result);
  // Complets the first time device checkin.
  void OnFirstTimeDeviceCheckinCompleted(const CheckinInfo& checkin_info);
  // Starts a login on mcs_client_.
  void StartMCSLogin();
  // Resets state to before initialization.
  void ResetState();
  // Sets state to ready. This will initiate the MCS login and notify the
  // delegates.
  void OnReady();

  // Starts a first time device checkin.
  void StartCheckin(const CheckinInfo& checkin_info);
  // Completes the device checkin request.
  // |android_id| and |security_token| are expected to be non-zero or an error
  // is triggered. Function also cleans up the pending checkin.
  void OnCheckinCompleted(uint64 android_id,
                          uint64 security_token);

  // Callback for persisting device credentials in the |gcm_store_|.
  void SetDeviceCredentialsCallback(bool success);

  // Completes the registration request.
  void OnRegisterCompleted(const std::string& app_id,
                           RegistrationRequest::Status status,
                           const std::string& registration_id);

  // Completes the unregistration request.
  void OnUnregisterCompleted(const std::string& app_id, bool status);

  // Completes the GCM store destroy request.
  void OnGCMStoreDestroyed(bool success);

  // Handles incoming data message and dispatches it the a relevant user
  // delegate.
  void HandleIncomingMessage(const gcm::MCSMessage& message);

  // Fires OnMessageSendError event on |delegate|, with specified |app_id| and
  // message ID obtained from |incoming_message| if one is available.
  void NotifyDelegateOnMessageSendError(
      GCMClient::Delegate* delegate,
      const std::string& app_id,
      const IncomingMessage& incoming_message);

  // For testing purpose only.
  // Sets an |mcs_client_| for testing. Takes the ownership of |mcs_client|.
  // TODO(fgorski): Remove this method. Create GCMEngineFactory that will create
  // components of the engine.
  void SetMCSClientForTesting(scoped_ptr<MCSClient> mcs_client);

  // State of the GCM Client Implementation.
  State state_;

  Delegate* delegate_;

  // Device checkin info (android ID and security token used by device).
  CheckinInfo device_checkin_info_;

  // Clock used for timing of retry logic. Passed in for testing. Owned by
  // GCMClientImpl.
  scoped_ptr<base::Clock> clock_;

  // Information about the chrome build.
  // TODO(fgorski): Check if it can be passed in constructor and made const.
  checkin_proto::ChromeBuildProto chrome_build_proto_;

  // Persistent data store for keeping device credentials, messages and user to
  // serial number mappings.
  scoped_ptr<GCMStore> gcm_store_;

  scoped_refptr<net::HttpNetworkSession> network_session_;
  net::BoundNetLog net_log_;
  scoped_ptr<ConnectionFactory> connection_factory_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // Controls receiving and sending of packets and reliable message queueing.
  scoped_ptr<MCSClient> mcs_client_;

  scoped_ptr<CheckinRequest> checkin_request_;

  // Currently pending registrations. GCMClientImpl owns the
  // RegistrationRequests.
  PendingRegistrations pending_registrations_;
  STLValueDeleter<PendingRegistrations> pending_registrations_deleter_;

  // Currently pending unregistrations. GCMClientImpl owns the
  // UnregistrationRequests.
  PendingUnregistrations pending_unregistrations_;
  STLValueDeleter<PendingUnregistrations> pending_unregistrations_deleter_;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GCMClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMClientImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_GCM_CLIENT_IMPL_H_
