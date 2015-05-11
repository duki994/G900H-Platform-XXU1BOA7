// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#if defined(ENABLE_PUSH_API)

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
//#include "content/common/push_registration/push_registration.h"

#define IPC_MESSAGE_START PushRegistrationMsgStart

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED3(PushRegistrationMsg_RegisterSuccess,
                    int32 /* callbacks_id */,
                    base::string16 /* endpoint */,
                    base::string16 /* registration_id */)
IPC_MESSAGE_ROUTED1(PushRegistrationMsg_RegisterError,
                    int32 /* callbacks_id */)


IPC_MESSAGE_ROUTED1(PushRegistrationMsg_UnregisterSuccess,
                    int32 /* callbacks_id */)
IPC_MESSAGE_ROUTED1(PushRegistrationMsg_UnregisterError,
                    int32 /* callbacks_id */)

IPC_MESSAGE_ROUTED2(PushRegistrationMsg_IsRegisteredSuccess,
                    int32 /* callbacks_id */,
                    bool /* is_registered */)
IPC_MESSAGE_ROUTED1(PushRegistrationMsg_IsRegisteredError,
                    int32 /* callbacks_id */)

IPC_MESSAGE_ROUTED2(PushRegistrationMsg_HasPermissionSuccess,
                    int32 /* callbacks_id */,
                    bool /* is_registered */)
IPC_MESSAGE_ROUTED1(PushRegistrationMsg_HasPermissionError,
                    int32 /* callbacks_id */)

IPC_MESSAGE_ROUTED2(PushRegistrationMsg_PermissionSet,
                    int32 /* callbacks_id */,
                    bool /* is_allowed */)

// Messages sent from the renderer to the browser.

IPC_MESSAGE_CONTROL3(PushRegistrationHostMsg_Register,
                     int32 /* routing_id */,
                     int32 /* callbacks_id */,
                     GURL /* origin */)

IPC_MESSAGE_CONTROL3(PushRegistrationHostMsg_Unregister,
                     int32 /* routing_id */,
                     int32 /* callbacks_id */,
                     GURL /* origin */)

IPC_MESSAGE_CONTROL3(PushRegistrationHostMsg_IsRegistered,
                     int32 /* routing_id */,
                     int32 /* callbacks_id */,
                     GURL /* origin */)

IPC_MESSAGE_CONTROL3(PushRegistrationHostMsg_HasPermission,
                     int32 /* routing_id */,
                     int32 /* callbacks_id */,
                     GURL /* origin */)

IPC_MESSAGE_CONTROL3(PushRegistrationHostMsg_RequestPermission,
                     int32 /* routing_id */,
                     int32 /* callbacks_id */,
                     GURL /* origin */)

#endif  // defined(ENABLE_PUSH_API)
