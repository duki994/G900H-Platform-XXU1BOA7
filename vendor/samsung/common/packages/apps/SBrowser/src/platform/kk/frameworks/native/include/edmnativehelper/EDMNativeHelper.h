/* vim: expandtab:tw=68:ts=4:sw=4:
 *
 * (c) Copyright 2012 Samsung Telecommunication America, Inc.
 *                  All rights reserved
 *
 *             Wireless Terminals Lab in Dallas Technology Lab 
 *
 *
 * File: EDMNativeHelper.h
 * Author: kbalakrishnan@sta.samsung.com
 * Creation date: Wed Aug 29 12:00:00 2012
 * Rev: $Id$
 *
 */


#ifndef EDMNATIVEHELPER_H
#define EDMNATIVEHELPER_H

//WTL_EDM

namespace android {

// Defines the type of file descriptor returned
enum
{
    CAMERA_RESTRICTION = 1,
    MIC_RESTRICTION
};


class EDMNativeHelper
{
    public:
        static void sendIntent(int restriction);
        static bool isCameraEnabled(int uid);
        static bool isScreenCaptureEnabled();
        static bool isAVRCPProfileEnabled();
        static bool isBTOutgoingCallEnabled();
        static bool isMicrophoneEnabled(int uid);
        static bool isAuditLogEnabled();
        static void nativeLogger(int severityGrade, int moduleGroup, int outcome, int uid,
                const char* swComponent, const char* logMessage);
};


} //namespace android

#endif //EDMNATIVEHELPER_H
