/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * (c) Copyright 2012 Samsung Telecommunication America, Inc.
 *                  All rights reserved
 *
 *             Wireless Terminals Lab in Dallas Technology Lab 
 *
 *
 * File: IEDMNativeHelperService.h
 * Author: kbalakrishnan@sta.samsung.com
 * Creation date: Wed Aug 29 12:00:00 2012
 * Rev: $Id$
 *
 */


#ifndef IEDMNATIVEHELPERSERVICE_H
#define IEDMNATIVEHELPERSERVICE_H

#include <binder/IInterface.h>
#include <binder/Parcel.h>

//WTL_EDM

namespace android {

class IEDMNativeHelperService : public IInterface
{
    public:
        enum {
            SEND_INTENT = IBinder::FIRST_CALL_TRANSACTION,
            IS_CAMERA_ENABLED,
            IS_SCREENCAPTURE_ENABLED,
            IS_AVRCPPROFILE_ENABLED,
            IS_BTOUTGOINGCALL_ENABLED,
            IS_MICROPHONE_ENABLED,
            IS_AUDITLOG_ENABLED,
            NATIVE_LOGGER
        };

    public:
        DECLARE_META_INTERFACE(EDMNativeHelperService);
        virtual void sendIntent(int restriction) = 0;
        virtual bool isCameraEnabled(int uid) = 0;
        virtual bool isScreenCaptureEnabled() = 0;
        virtual bool isAVRCPProfileEnabled() = 0;
        virtual bool isBTOutgoingCallEnabled() = 0;
        virtual bool isMicrophoneEnabled(int uid) = 0;
        virtual bool isAuditLogEnabled() = 0;
        virtual void nativeLogger(int severityGrade, int moduleGroup, int outcome, int uid,
                        const char* swComponent, const char* logMessage) = 0;
};

//----------------------------------------------------------------------------

class BnEDMNativeHelperService : public BnInterface<IEDMNativeHelperService>
{
    public:
        virtual status_t onTransact(uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

} // namespace android

#endif //IEDMNATIVEHELPERSERVICE_H
